/*
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright (c) 2017 by Delphix. All rights reserved.
 */

/*
 * Network Virtualization Service.
 */

#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/bitmap.h>
#include <sys/atomic.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ddi.h>

#include <sys/hyperv.h>
#include <sys/vmbus.h>
#include <sys/vmbus_xact.h>

#include "if_hnvar.h"
#include "hn_rndis.h"
#include "if_hnreg.h"
#include "hn_nvs.h"

static int			hn_nvs_conn_chim(struct hn_softc *);
static int			hn_nvs_conn_rxbuf(struct hn_softc *);
static void			hn_nvs_disconn_chim(struct hn_softc *);
static void			hn_nvs_disconn_rxbuf(struct hn_softc *);
static int			hn_nvs_conf_ndis(struct hn_softc *, int);
static int			hn_nvs_init_ndis(struct hn_softc *);
static int			hn_nvs_doinit(struct hn_softc *, uint32_t);
static int			hn_nvs_init(struct hn_softc *);
static const void		*hn_nvs_xact_execute(struct hn_softc *,
				    struct vmbus_xact *, void *, int,
				    size_t *, uint32_t);
static void			hn_nvs_sent_none(struct hn_nvs_sendctx *,
				    struct hn_softc *, struct vmbus_channel *,
				    const void *, int);

struct hn_nvs_sendctx		hn_nvs_sendctx_none =
    HN_NVS_SENDCTX_INITIALIZER(hn_nvs_sent_none, NULL);

static const uint32_t		hn_nvs_version[] = {
	HN_NVS_VERSION_5,
	HN_NVS_VERSION_4,
	HN_NVS_VERSION_2,
	HN_NVS_VERSION_1
};

static const void *
hn_nvs_xact_execute(struct hn_softc *sc, struct vmbus_xact *xact,
    void *req, int reqlen, size_t *resplen0, uint32_t type)
{
	struct hn_nvs_sendctx sndc;
	size_t resplen, min_resplen = *resplen0;
	const struct hn_nvs_hdr *hdr;
	int error;

	ASSERT3U(min_resplen, >=, sizeof (*hdr));

	/*
	 * Execute the xact setup by the caller.
	 */
	hn_nvs_sendctx_init(&sndc, hn_nvs_sent_xact, xact);

	vmbus_xact_activate(xact);
	error = hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_RC,
	    req, reqlen, &sndc);
	if (error) {
		vmbus_xact_deactivate(xact);
		return (NULL);
	}
	hdr = vmbus_chan_xact_wait(sc->hn_prichan, xact, &resplen,
	    B_TRUE);

	/*
	 * Check this NVS response message.
	 */
	if (resplen < min_resplen) {
		HN_WARN(sc, "invalid NVS resp len %lu", resplen);
		return (NULL);
	}
	if (hdr->nvs_type != type) {
		HN_WARN(sc, "unexpected NVS resp 0x%08x, expect 0x%08x",
		    hdr->nvs_type, type);
		return (NULL);
	}
	/* All pass! */
	*resplen0 = resplen;
	return (hdr);
}

static inline int
hn_nvs_req_send(struct hn_softc *sc, void *req, int reqlen)
{

	return (hn_nvs_send(sc->hn_prichan, VMBUS_CHANPKT_FLAG_NONE,
	    req, reqlen, &hn_nvs_sendctx_none));
}

static int
hn_nvs_conn_rxbuf(struct hn_softc *sc)
{
	struct vmbus_xact *xact = NULL;
	struct hn_nvs_rxbuf_conn *conn;
	const struct hn_nvs_rxbuf_connresp *resp;
	size_t resp_len;
	uint32_t status;
	int error, rxbuf_size;

	/*
	 * Limit RXBUF size for old NVS.
	 */
	if (sc->hn_nvs_ver <= HN_NVS_VERSION_2)
		rxbuf_size = HN_RXBUF_SIZE_COMPAT;
	else
		rxbuf_size = HN_RXBUF_SIZE;

	/*
	 * Connect the RXBUF GPADL to the primary channel.
	 *
	 * NOTE:
	 * Only primary channel has RXBUF connected to it.  Sub-channels
	 * just share this RXBUF.
	 */
	error = vmbus_chan_gpadl_connect(sc->hn_prichan,
	    sc->hn_rxbuf_dma.hv_paddr, rxbuf_size, &sc->hn_rxbuf_gpadl);
	if (error) {
		HN_WARN(sc, "rxbuf gpadl conn failed: %d", error);
		goto cleanup;
	}

	/*
	 * Connect RXBUF to NVS.
	 */

	xact = vmbus_xact_get(sc->hn_xact, sizeof (*conn));
	if (xact == NULL) {
		HN_WARN(sc, "no xact for nvs rxbuf conn");
		error = ENXIO;
		goto cleanup;
	}
	conn = vmbus_xact_req_data(xact);
	conn->nvs_type = HN_NVS_TYPE_RXBUF_CONN;
	conn->nvs_gpadl = sc->hn_rxbuf_gpadl;
	conn->nvs_sig = HN_NVS_RXBUF_SIG;

	resp_len = sizeof (*resp);
	resp = hn_nvs_xact_execute(sc, xact, conn, sizeof (*conn), &resp_len,
	    HN_NVS_TYPE_RXBUF_CONNRESP);
	if (resp == NULL) {
		HN_WARN(sc, "exec nvs rxbuf conn failed");
		error = EIO;
		goto cleanup;
	}

	status = resp->nvs_status;
	vmbus_xact_put(xact);
	xact = NULL;

	if (status != HN_NVS_STATUS_OK) {
		HN_WARN(sc, "nvs rxbuf conn failed: %u", status);
		error = EIO;
		goto cleanup;
	}
	sc->hn_flags |= HN_FLAG_RXBUF_CONNECTED;

	return (0);

cleanup:
	if (xact != NULL)
		vmbus_xact_put(xact);
	hn_nvs_disconn_rxbuf(sc);
	return (error);
}

static int
hn_nvs_conn_chim(struct hn_softc *sc)
{
	struct vmbus_xact *xact = NULL;
	struct hn_nvs_chim_conn *chim;
	const struct hn_nvs_chim_connresp *resp;
	size_t resp_len;
	uint32_t status, sectsz;
	int error;

	/*
	 * Connect chimney sending buffer GPADL to the primary channel.
	 *
	 * NOTE:
	 * Only primary channel has chimney sending buffer connected to it.
	 * Sub-channels just share this chimney sending buffer.
	 */
	error = vmbus_chan_gpadl_connect(sc->hn_prichan,
	    sc->hn_chim_dma.hv_paddr, HN_CHIM_SIZE, &sc->hn_chim_gpadl);
	if (error) {
		HN_WARN(sc, "chim gpadl conn failed: %d", error);
		goto cleanup;
	}

	/*
	 * Connect chimney sending buffer to NVS
	 */

	xact = vmbus_xact_get(sc->hn_xact, sizeof (*chim));
	if (xact == NULL) {
		HN_WARN(sc, "no xact for nvs chim conn");
		error = ENXIO;
		goto cleanup;
	}
	chim = vmbus_xact_req_data(xact);
	chim->nvs_type = HN_NVS_TYPE_CHIM_CONN;
	chim->nvs_gpadl = sc->hn_chim_gpadl;
	chim->nvs_sig = HN_NVS_CHIM_SIG;

	resp_len = sizeof (*resp);
	resp = hn_nvs_xact_execute(sc, xact, chim, sizeof (*chim), &resp_len,
	    HN_NVS_TYPE_CHIM_CONNRESP);
	if (resp == NULL) {
		HN_WARN(sc, "exec nvs chim conn failed");
		error = EIO;
		goto cleanup;
	}

	status = resp->nvs_status;
	sectsz = resp->nvs_sectsz;
	vmbus_xact_put(xact);
	xact = NULL;

	if (status != HN_NVS_STATUS_OK) {
		HN_WARN(sc, "nvs chim conn failed: %u", status);
		error = EIO;
		goto cleanup;
	}
	if (sectsz == 0) {
		/*
		 * Can't use chimney sending buffer; done!
		 */
		HN_WARN(sc, "zero chimney sending buffer section size");
		sc->hn_chim_szmax = 0;
		sc->hn_chim_cnt = 0;
		sc->hn_flags |= HN_FLAG_CHIM_CONNECTED;
		return (0);
	}

	sc->hn_chim_szmax = sectsz;
	sc->hn_chim_cnt = HN_CHIM_SIZE / sc->hn_chim_szmax;
	if (HN_CHIM_SIZE % sc->hn_chim_szmax != 0) {
		HN_WARN(sc, "chimney sending sections are "
		    "not properly aligned");
	}
	if ((sc->hn_chim_cnt & BT_ULMASK) != 0) {
		HN_WARN(sc, "discard %d chimney sending sections",
		    sc->hn_chim_cnt & BT_ULMASK);
	}

	sc->hn_chim_bmap_cnt = sc->hn_chim_cnt >> BT_ULSHIFT;
	sc->hn_chim_bmap = kmem_zalloc(sc->hn_chim_bmap_cnt * sizeof (ulong_t),
	    KM_SLEEP);

	/* Done! */
	sc->hn_flags |= HN_FLAG_CHIM_CONNECTED;
	HN_DEBUG(sc, 1, "chimney sending buffer %d/%d",
	    sc->hn_chim_szmax, sc->hn_chim_cnt);
	return (0);

cleanup:
	if (xact != NULL)
		vmbus_xact_put(xact);
	hn_nvs_disconn_chim(sc);
	return (error);
}

static void
hn_nvs_disconn_rxbuf(struct hn_softc *sc)
{
	int error;

	if (sc->hn_flags & HN_FLAG_RXBUF_CONNECTED) {
		struct hn_nvs_rxbuf_disconn disconn;

		/*
		 * Disconnect RXBUF from NVS.
		 */
		(void) memset(&disconn, 0, sizeof (disconn));
		disconn.nvs_type = HN_NVS_TYPE_RXBUF_DISCONN;
		disconn.nvs_sig = HN_NVS_RXBUF_SIG;

		/* NOTE: No response. */
		error = hn_nvs_req_send(sc, &disconn, sizeof (disconn));
		if (error) {
			HN_WARN(sc, "send nvs rxbuf disconn failed: %d",
			    error);
			/*
			 * Fine for a revoked channel, since the hypervisor
			 * does not drain TX bufring for a revoked channel.
			 */
			if (!vmbus_chan_is_revoked(sc->hn_prichan))
				sc->hn_flags |= HN_FLAG_RXBUF_REF;
		}
		sc->hn_flags &= ~HN_FLAG_RXBUF_CONNECTED;

		/*
		 * Wait for the hypervisor to receive this NVS request.
		 *
		 * NOTE:
		 * The TX bufring will not be drained by the hypervisor,
		 * if the primary channel is revoked.
		 */
		while (!vmbus_chan_tx_empty(sc->hn_prichan) &&
		    !vmbus_chan_is_revoked(sc->hn_prichan))
			delay(1);
		/*
		 * Linger long enough for NVS to disconnect RXBUF.
		 */
		delay(MSEC_TO_TICK(200));
	}

	if (vmbus_current_version < VMBUS_VERSION_WIN10 &&
	    sc->hn_rxbuf_gpadl != 0) {
		/*
		 * Disconnect RXBUF from primary channel.
		 */
		error = vmbus_chan_gpadl_disconnect(sc->hn_prichan,
		    sc->hn_rxbuf_gpadl);
		if (error) {
			HN_WARN(sc, "rxbuf gpadl disconn failed: %d",
			    error);
			sc->hn_flags |= HN_FLAG_RXBUF_REF;
		}
		sc->hn_rxbuf_gpadl = 0;
	}
}

static void
hn_nvs_disconn_chim(struct hn_softc *sc)
{
	int error;

	if (sc->hn_flags & HN_FLAG_CHIM_CONNECTED) {
		struct hn_nvs_chim_disconn disconn;

		/*
		 * Disconnect chimney sending buffer from NVS.
		 */
		(void) memset(&disconn, 0, sizeof (disconn));
		disconn.nvs_type = HN_NVS_TYPE_CHIM_DISCONN;
		disconn.nvs_sig = HN_NVS_CHIM_SIG;

		/* NOTE: No response. */
		error = hn_nvs_req_send(sc, &disconn, sizeof (disconn));
		if (error) {
			HN_WARN(sc, "send nvs chim disconn failed: %d",
			    error);
			/*
			 * Fine for a revoked channel, since the hypervisor
			 * does not drain TX bufring for a revoked channel.
			 */
			if (!vmbus_chan_is_revoked(sc->hn_prichan))
				sc->hn_flags |= HN_FLAG_CHIM_REF;
		}
		sc->hn_flags &= ~HN_FLAG_CHIM_CONNECTED;

		/*
		 * Wait for the hypervisor to receive this NVS request.
		 *
		 * NOTE:
		 * The TX bufring will not be drained by the hypervisor,
		 * if the primary channel is revoked.
		 */
		while (!vmbus_chan_tx_empty(sc->hn_prichan) &&
		    !vmbus_chan_is_revoked(sc->hn_prichan))
			delay(1);
		/*
		 * Linger long enough for NVS to disconnect chimney
		 * sending buffer.
		 */
		delay(MSEC_TO_TICK(200));
	}

	if (vmbus_current_version < VMBUS_VERSION_WIN10 &&
	    sc->hn_chim_gpadl != 0) {
		/*
		 * Disconnect chimney sending buffer from primary channel.
		 */
		error = vmbus_chan_gpadl_disconnect(sc->hn_prichan,
		    sc->hn_chim_gpadl);
		if (error) {
			HN_WARN(sc, "chim gpadl disconn failed: %d", error);
			sc->hn_flags |= HN_FLAG_CHIM_REF;
		}
		sc->hn_chim_gpadl = 0;
	}

	if (sc->hn_chim_bmap != NULL) {
		kmem_free(sc->hn_chim_bmap, sc->hn_chim_bmap_cnt *
		    sizeof (ulong_t));
		sc->hn_chim_bmap = NULL;
		sc->hn_chim_bmap_cnt = 0;
	}
}

static int
hn_nvs_doinit(struct hn_softc *sc, uint32_t nvs_ver)
{
	struct vmbus_xact *xact;
	struct hn_nvs_init *init;
	const struct hn_nvs_init_resp *resp;
	size_t resp_len;
	uint32_t status;

	xact = vmbus_xact_get(sc->hn_xact, sizeof (*init));
	if (xact == NULL) {
		HN_WARN(sc, "no xact for nvs init");
		return (ENXIO);
	}
	init = vmbus_xact_req_data(xact);
	init->nvs_type = HN_NVS_TYPE_INIT;
	init->nvs_ver_min = nvs_ver;
	init->nvs_ver_max = nvs_ver;

	resp_len = sizeof (*resp);
	resp = hn_nvs_xact_execute(sc, xact, init, sizeof (*init), &resp_len,
	    HN_NVS_TYPE_INIT_RESP);
	if (resp == NULL) {
		HN_WARN(sc, "exec init failed");
		vmbus_xact_put(xact);
		return (EIO);
	}

	status = resp->nvs_status;
	vmbus_xact_put(xact);

	if (status != HN_NVS_STATUS_OK) {
		/*
		 * Caller may try another NVS version, and will log
		 * error if there are no more NVS versions to try,
		 * so don't bark out loud here.
		 */
		HN_DEBUG(sc, 1, "nvs init failed for ver 0x%x", nvs_ver);
		return (EINVAL);
	}
	return (0);
}

/*
 * Configure MTU and enable VLAN.
 */
static int
hn_nvs_conf_ndis(struct hn_softc *sc, int mtu)
{
	struct hn_nvs_ndis_conf conf;
	int error;

	(void) memset(&conf, 0, sizeof (conf));
	conf.nvs_type = HN_NVS_TYPE_NDIS_CONF;
	conf.nvs_mtu = mtu;
	conf.nvs_caps = HN_NVS_NDIS_CONF_VLAN;

	/* NOTE: No response. */
	error = hn_nvs_req_send(sc, &conf, sizeof (conf));
	if (error) {
		HN_WARN(sc, "send nvs ndis conf failed: %d", error);
		return (error);
	}

	HN_DEBUG(sc, 1, "nvs ndis conf done");
	sc->hn_caps |= HN_CAP_MTU | HN_CAP_VLAN;
	return (0);
}

static int
hn_nvs_init_ndis(struct hn_softc *sc)
{
	struct hn_nvs_ndis_init ndis;
	int error;

	(void) memset(&ndis, 0, sizeof (ndis));
	ndis.nvs_type = HN_NVS_TYPE_NDIS_INIT;
	ndis.nvs_ndis_major = HN_NDIS_VERSION_MAJOR(sc->hn_ndis_ver);
	ndis.nvs_ndis_minor = HN_NDIS_VERSION_MINOR(sc->hn_ndis_ver);

	/* NOTE: No response. */
	error = hn_nvs_req_send(sc, &ndis, sizeof (ndis));
	if (error)
		HN_WARN(sc, "send nvs ndis init failed: %d", error);
	return (error);
}

static int
hn_nvs_init(struct hn_softc *sc)
{
	int i, error;

	if (i_ddi_devi_attached(sc->hn_dev)) {
		/*
		 * NVS version and NDIS version MUST NOT be changed.
		 */
		HN_DEBUG(sc, 1, "reinit NVS version 0x%x, "
		    "NDIS version %u.%u", sc->hn_nvs_ver,
		    HN_NDIS_VERSION_MAJOR(sc->hn_ndis_ver),
		    HN_NDIS_VERSION_MINOR(sc->hn_ndis_ver));

		error = hn_nvs_doinit(sc, sc->hn_nvs_ver);
		if (error) {
			HN_WARN(sc, "reinit NVS version 0x%x failed: %d",
			    sc->hn_nvs_ver, error);
			return (error);
		}
		goto done;
	}

	/*
	 * Find the supported NVS version and set NDIS version accordingly.
	 */
	for (i = 0; i < (sizeof (hn_nvs_version) / sizeof (uint32_t)); ++i) {
		error = hn_nvs_doinit(sc, hn_nvs_version[i]);
		if (!error) {
			sc->hn_nvs_ver = hn_nvs_version[i];

			/* Set NDIS version according to NVS version. */
			sc->hn_ndis_ver = HN_NDIS_VERSION_6_30;
			if (sc->hn_nvs_ver <= HN_NVS_VERSION_4)
				sc->hn_ndis_ver = HN_NDIS_VERSION_6_1;

			HN_DEBUG(sc, 1, "NVS version 0x%x, "
			    "NDIS version %u.%u", sc->hn_nvs_ver,
			    HN_NDIS_VERSION_MAJOR(sc->hn_ndis_ver),
			    HN_NDIS_VERSION_MINOR(sc->hn_ndis_ver));
			goto done;
		}
	}
	HN_WARN(sc, "no NVS available");
	return (ENXIO);

done:
	if (sc->hn_nvs_ver >= HN_NVS_VERSION_5)
		sc->hn_caps |= HN_CAP_HASHVAL;
	return (0);
}

int
hn_nvs_attach(struct hn_softc *sc, int mtu)
{
	int error;

	/*
	 * Initialize NVS.
	 */
	error = hn_nvs_init(sc);
	if (error)
		return (error);

	if (sc->hn_nvs_ver >= HN_NVS_VERSION_2) {
		/*
		 * Configure NDIS before initializing it.
		 */
		error = hn_nvs_conf_ndis(sc, mtu);
		if (error)
			return (error);
	}

	/*
	 * Initialize NDIS.
	 */
	error = hn_nvs_init_ndis(sc);
	if (error)
		return (error);

	/*
	 * Connect RXBUF.
	 */
	error = hn_nvs_conn_rxbuf(sc);
	if (error)
		return (error);

	/*
	 * Connect chimney sending buffer.
	 */
	error = hn_nvs_conn_chim(sc);
	if (error) {
		hn_nvs_disconn_rxbuf(sc);
		return (error);
	}
	return (0);
}

void
hn_nvs_detach(struct hn_softc *sc)
{

	/* NOTE: there are no requests to stop the NVS. */
	hn_nvs_disconn_rxbuf(sc);
	hn_nvs_disconn_chim(sc);
}

/*ARGSUSED*/
void
hn_nvs_sent_xact(struct hn_nvs_sendctx *sndc,
    struct hn_softc *sc, struct vmbus_channel *chan,
    const void *data, int dlen)
{

	vmbus_xact_wakeup(sndc->hn_cbarg, data, dlen);
}

/*ARGSUSED*/
static void
hn_nvs_sent_none(struct hn_nvs_sendctx *sndc,
    struct hn_softc *sc, struct vmbus_channel *chan,
    const void *data, int dlen)
{
}

int
hn_nvs_alloc_subchans(struct hn_softc *sc, int *nsubch0)
{
	struct vmbus_xact *xact;
	struct hn_nvs_subch_req *req;
	const struct hn_nvs_subch_resp *resp;
	int error, nsubch_req;
	uint32_t nsubch;
	size_t resp_len;

	nsubch_req = *nsubch0;
	ASSERT3S(nsubch_req, >, 0);

	xact = vmbus_xact_get(sc->hn_xact, sizeof (*req));
	if (xact == NULL) {
		HN_WARN(sc, "no xact for nvs subch alloc");
		return (ENXIO);
	}
	req = vmbus_xact_req_data(xact);
	req->nvs_type = HN_NVS_TYPE_SUBCH_REQ;
	req->nvs_op = HN_NVS_SUBCH_OP_ALLOC;
	req->nvs_nsubch = nsubch_req;

	resp_len = sizeof (*resp);
	resp = hn_nvs_xact_execute(sc, xact, req, sizeof (*req), &resp_len,
	    HN_NVS_TYPE_SUBCH_RESP);
	if (resp == NULL) {
		HN_WARN(sc, "exec nvs subch alloc failed");
		error = EIO;
		goto done;
	}
	if (resp->nvs_status != HN_NVS_STATUS_OK) {
		HN_WARN(sc, "nvs subch alloc failed: %x",
		    resp->nvs_status);
		error = EIO;
		goto done;
	}

	nsubch = resp->nvs_nsubch;
	if (nsubch > nsubch_req) {
		HN_WARN(sc, "%u subchans are allocated, requested %d",
		    nsubch, nsubch_req);
		nsubch = nsubch_req;
	}
	*nsubch0 = nsubch;
	error = 0;
done:
	vmbus_xact_put(xact);
	return (error);
}

int
hn_nvs_send_rndis_sglist(struct vmbus_channel *chan, uint32_t rndis_mtype,
    struct hn_nvs_sendctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt)
{
	struct hn_nvs_rndis rndis;

	rndis.nvs_type = HN_NVS_TYPE_RNDIS;
	rndis.nvs_rndis_mtype = rndis_mtype;
	rndis.nvs_chim_idx = HN_NVS_CHIM_IDX_INVALID;
	rndis.nvs_chim_sz = 0;

	return (hn_nvs_send_sglist(chan, gpa, gpa_cnt,
	    &rndis, sizeof (rndis), sndc));
}

int
hn_nvs_send_rndis_ctrl(struct vmbus_channel *chan,
    struct hn_nvs_sendctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt)
{

	return hn_nvs_send_rndis_sglist(chan, HN_NVS_RNDIS_MTYPE_CTRL,
	    sndc, gpa, gpa_cnt);
}

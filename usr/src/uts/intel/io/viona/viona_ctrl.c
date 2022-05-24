/*
 * Copyright (c) 2013  Chris Torek <torek @ torek net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
 *
 * Copyright 2015 Pluribus Networks Inc.
 * Copyright 2019 Joyent, Inc.
 * Copyright 2021 Oxide Computer Company
 * Copyright 2022 OmniOS Community Edition (OmniOSce) Association.
 */

#include <sys/types.h>

#include "viona_impl.h"

#define VTNET_CTRL_MAXSEGS	32

static void
viona_ctrl_done(viona_vring_t *ring, uint32_t len, uint16_t cookie)
{
	vq_pushchain(ring, len, cookie);

	membar_enter();
	viona_intr_ring(ring, B_FALSE);
}

static void
viona_ctrl(viona_link_t *link, viona_vring_t *ring)
{
	struct iovec		iov[VTNET_CTRL_MAXSEGS];
	vmm_page_t		*pages = NULL;
	uint16_t		cookie, len;
	uint8_t			*ackp;
	int			n;
	const struct virtio_net_ctrl_hdr *hdr;

	ASSERT(iov != NULL);

	n = vq_popchain(ring, iov, VTNET_CTRL_MAXSEGS, &cookie, &pages);
	if (n == 0) {
		VIONA_PROBE1(ctrl_absent, viona_vring_t *, ring);
		VIONA_RING_STAT_INCR(ring, ctrl_absent);
		return;
	} else if (n < 0) {
		/*
		 * Any error encountered in vq_popchain has already resulted in
		 * specific probe and statistic handling.  Further action here
		 * is unnecessary.
		 */
		return;
	}

	/*
	 * Since we have not negotiated VIRTIO_F_ANY_LAYOUT, the control
	 * message header, data and ack will be in at least three separate
	 * descriptors.
	 */
	if (n < 3) {
		VIONA_PROBE2(ctrl_short, viona_vring_t *, ring, int, n);
		VIONA_RING_STAT_INCR(ring, ctrl_short);
		goto out;
	}

	hdr = (const struct virtio_net_ctrl_hdr *)iov[0].iov_base;
	if (iov[0].iov_len < sizeof (struct virtio_net_ctrl_hdr))
		goto out;

	ackp = (uint8_t *)iov[n - 1].iov_base;
	*ackp = VIRTIO_NET_CQ_OK;

	VIONA_PROBE4(ctrl, viona_vring_t *, ring,
	    uint8_t, hdr->vnc_class, uint8_t, hdr->vnc_cmd, int, n);

	switch (hdr->vnc_class) {
	case VIRTIO_NET_CTRL_RX: {
		uint8_t *datap = (uint8_t *)iov[1].iov_base;
		size_t datalen = iov[1].iov_len;

		VIONA_PROBE3(ctrl_rx, viona_vring_t *, ring,
		    uint8_t, hdr->vnc_cmd, uint8_t, *datap);

		switch (hdr->vnc_cmd) {
		case VIRTIO_NET_CTRL_RX_PROMISC: {
			mac_client_promisc_type_t pt;
			int err = 1;

			if (datalen == sizeof (uint8_t)) {
				if (*datap == 1)
					pt = MAC_CLIENT_PROMISC_ALL;
				else
					pt = MAC_CLIENT_PROMISC_MULTI;
				err = viona_rx_set_promisc(link, pt);
			}
			if (err != 0)
				*ackp = VIRTIO_NET_CQ_ERR;
			break;
		}
		case VIRTIO_NET_CTRL_RX_ALLMULTI:
			/*
			 * At present, this driver always passes multicast
			 * frames to the guest; ignore the request and return
			 * success.
			 */
			break;
		default:
			/*
			 * We have only negotiated VIRTIO_NET_F_CTRL_RX and
			 * not VIRTIO_NET_F_CTRL_RX_EXTRA; no other command
			 * types are expected.
			 */
			*ackp = VIRTIO_NET_CQ_ERR;
			break;
		}
		break;
	}
	case VIRTIO_NET_CTRL_MAC:
		VIONA_PROBE2(ctrl_mac, viona_vring_t *, ring,
		    uint8_t, hdr->vnc_cmd);
		/* Mac address table programming not currently supported. */
		break;
	default:
		VIONA_PROBE3(ctrl_unknown, viona_vring_t *, ring,
		    uint8_t, hdr->vnc_class, uint8_t, hdr->vnc_cmd);
		*ackp = VIRTIO_NET_CQ_ERR;
		break;
	}

out:

	len = 0;
	for (uint_t i = 0; i < n; i++)
		len += iov[i].iov_len;

	vmm_drv_page_release_chain(pages);
	viona_ctrl_done(ring, len, cookie);
}

void
viona_worker_ctrl(viona_vring_t *ring, viona_link_t *link)
{
	proc_t *p = ttoproc(curthread);

	(void) thread_vsetname(curthread, "viona_ctrl_%p", ring);

	ASSERT(MUTEX_HELD(&ring->vr_lock));
	ASSERT3U(ring->vr_state, ==, VRS_RUN);

	mutex_exit(&ring->vr_lock);

	for (;;) {
		boolean_t bail = B_FALSE;
		boolean_t renew = B_FALSE;
		uint_t ntx = 0;

		viona_ring_disable_notify(ring);
		while (viona_ring_num_avail(ring) > 0) {
			viona_ctrl(link, ring);
			if (ntx++ >= ring->vr_size)
				break;
		}
		viona_ring_enable_notify(ring);

		/*
		 * Check for available descriptors on the ring once more in
		 * case a late addition raced with the NO_NOTIFY flag toggle.
		 *
		 * The barrier ensures that visibility of the no-notify
		 * store does not cross the viona_ring_num_avail() check below.
		 */
		membar_enter();
		bail = VRING_NEED_BAIL(ring, p);
		renew = vmm_drv_lease_expired(ring->vr_lease);
		if (!bail && !renew && viona_ring_num_avail(ring) > 0)
			continue;

		if ((link->l_features & VIRTIO_F_RING_NOTIFY_ON_EMPTY) != 0) {
			/*
			 * The NOTIFY_ON_EMPTY interrupt should not pay heed to
			 * the presence of AVAIL_NO_INTERRUPT.
			 */
			viona_intr_ring(ring, B_TRUE);
		}

		mutex_enter(&ring->vr_lock);

		while (!bail && !renew && viona_ring_num_avail(ring) == 0) {
			(void) cv_wait_sig(&ring->vr_cv, &ring->vr_lock);
			bail = VRING_NEED_BAIL(ring, p);
			renew = vmm_drv_lease_expired(ring->vr_lease);
		}

		if (bail) {
			break;
		} else if (renew) {
			ring->vr_state_flags |= VRSF_RENEW;

			if (!viona_ring_lease_renew(ring))
				break;
			ring->vr_state_flags &= ~VRSF_RENEW;
		}
		mutex_exit(&ring->vr_lock);
	}

	ASSERT(MUTEX_HELD(&ring->vr_lock));

	ring->vr_state = VRS_STOP;
}

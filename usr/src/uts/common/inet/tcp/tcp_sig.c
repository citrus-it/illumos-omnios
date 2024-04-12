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
 * Copyright 2024 Oxide Computer Company
 */

/*
 * XXX
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/strlog.h>
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/md5.h>
#include <sys/mkdev.h>
#include <sys/kmem.h>
#include <sys/zone.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <sys/strsun.h>
#include <sys/random.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <net/pfkeyv2.h>
#include <net/pfpolicy.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/nd.h>
#include <inet/ip_if.h>
#include <inet/ip_ndp.h>
#include <inet/ipdrop.h>
#include <inet/tcp_sig.h>
#include <sys/taskq.h>
#include <sys/policy.h>
#include <sys/strsun.h>
#include <sys/list.h>

#include <sys/kstat.h>
#include <sys/strsubr.h>

void
tcpsig_init(ip_stack_t *ipst)
{
	mutex_init(&ipst->ips_tcpsigdb_lock, NULL, MUTEX_DEFAULT, NULL);
}

void
tcpsig_destroy(ip_stack_t *ipst)
{
	mutex_destroy(&ipst->ips_tcpsigdb_lock);
}

static tcpsig_db_t *
tcpsig_db(ip_stack_t *ipst)
{
	mutex_enter(&ipst->ips_tcpsigdb_lock);
	if (ipst->ips_tcpsigdb == NULL) {
		tcpsig_db_t *db = kmem_alloc(sizeof (tcpsig_db_t), KM_SLEEP);

		rw_init(&db->td_lock, NULL, RW_DEFAULT, 0);
		list_create(&db->td_salist, sizeof (tcpsig_sa_t),
		    offsetof(tcpsig_sa_t, ts_link));

		ipst->ips_tcpsigdb = db;
	}
	mutex_exit(&ipst->ips_tcpsigdb_lock);

	return ((tcpsig_db_t *)ipst->ips_tcpsigdb);
}

static void
tcpsig_sa_hold(tcpsig_sa_t *sa)
{
	mutex_enter(&sa->ts_lock);
	sa->ts_refcnt++;
	mutex_exit(&sa->ts_lock);
}

void
tcpsig_sa_rele(tcpsig_sa_t *sa)
{
	VERIFY3U(sa->ts_refcnt, >, 0);
	mutex_enter(&sa->ts_lock);
	sa->ts_refcnt--;
	mutex_exit(&sa->ts_lock);
}

static bool
tcpsig_sa_match4(tcpsig_sa_t *sa, struct sockaddr_storage *src_s,
    struct sockaddr_storage *dst_s)
{
	sin_t msrc, mdst, *src, *dst, *sasrc, *sadst;

	if (src_s->ss_family != AF_INET)
		return (false);

	src = (sin_t *)src_s;
	dst = (sin_t *)dst_s;

	if (sa->ts_family == AF_INET6) {
		sin6_t *sasrc6 = (sin6_t *)&sa->ts_src;
		sin6_t *sadst6 = (sin6_t *)&sa->ts_dst;

		if (!IN6_IS_ADDR_V4MAPPED(&sasrc6->sin6_addr) ||
		    !IN6_IS_ADDR_V4MAPPED(&sadst6->sin6_addr)) {
			return (false);
		}

		msrc = sin_null;
		msrc.sin_family = AF_INET;
		msrc.sin_port = sasrc6->sin6_port;
		IN6_V4MAPPED_TO_INADDR(&sasrc6->sin6_addr, &msrc.sin_addr);
		sasrc = &msrc;

		mdst = sin_null;
		mdst.sin_family = AF_INET;
		mdst.sin_port = sadst6->sin6_port;
		IN6_V4MAPPED_TO_INADDR(&sadst6->sin6_addr, &mdst.sin_addr);
		sadst = &mdst;
	} else {
		sasrc = (sin_t *)&sa->ts_src;
		sadst = (sin_t *)&sa->ts_dst;
	}

	if (sasrc->sin_port != 0 && sasrc->sin_port != src->sin_port)
		return (false);
	if (sadst->sin_port != 0 && sadst->sin_port != dst->sin_port)
		return (false);

	if (sasrc->sin_addr.s_addr != src->sin_addr.s_addr)
		return (false);
	if (sadst->sin_addr.s_addr != dst->sin_addr.s_addr)
		return (false);

	return (true);
}

static bool
tcpsig_sa_match6(tcpsig_sa_t *sa, struct sockaddr_storage *src_s,
    struct sockaddr_storage *dst_s)
{
	sin6_t *src, *dst, *sasrc, *sadst;

	if (src_s->ss_family != AF_INET6 || sa->ts_src.ss_family != AF_INET6)
		return (false);

	src = (sin6_t *)src_s;
	dst = (sin6_t *)dst_s;

	sasrc = (sin6_t *)&sa->ts_src;
	sadst = (sin6_t *)&sa->ts_dst;

	if (sasrc->sin6_port != 0 && sasrc->sin6_port != src->sin6_port)
		return (false);
	if (sadst->sin6_port != 0 && sadst->sin6_port != dst->sin6_port)
		return (false);

	if (!IN6_ARE_ADDR_EQUAL(&sasrc->sin6_addr, &src->sin6_addr))
		return (false);
	if (!IN6_ARE_ADDR_EQUAL(&sadst->sin6_addr, &dst->sin6_addr))
		return (false);

	return (true);
}

static tcpsig_sa_t *
tcpsig_sa_find_held(struct sockaddr_storage *src, struct sockaddr_storage *dst,
    ip_stack_t *ipst)
{
	tcpsig_db_t *db = tcpsig_db(ipst);
	tcpsig_sa_t *sa = NULL;

	ASSERT(RW_LOCK_HELD(&db->td_lock));

	if (src->ss_family != dst->ss_family)
		return (NULL);

	for (sa = list_head(&db->td_salist); sa != NULL;
	    sa = list_next(&db->td_salist, sa)) {
		if (tcpsig_sa_match4(sa, src, dst) ||
		    tcpsig_sa_match6(sa, src, dst)) {
			tcpsig_sa_hold(sa);
			break;
		}
	}

	return (sa);
}

static tcpsig_sa_t *
tcpsig_sa_find(struct sockaddr_storage *src, struct sockaddr_storage *dst,
    ip_stack_t *ipst)
{
	tcpsig_db_t *db = tcpsig_db(ipst);
	tcpsig_sa_t *sa;

	rw_enter(&db->td_lock, RW_READER);
	sa = tcpsig_sa_find_held(src, dst, ipst);
	rw_exit(&db->td_lock);

	return (sa);
}

static void
tcpsig_sa_free(tcpsig_sa_t *sa)
{
	mutex_destroy(&sa->ts_lock);
	kmem_free(sa->ts_key.sak_key, sa->ts_key.sak_keylen);
	kmem_free(sa, sizeof (*sa));
}

static int
tcpsig_sa_flush(ip_stack_t *ipst, int *diagp)
{
	tcpsig_db_t *db = tcpsig_db(ipst);
	tcpsig_sa_t *nextsa;

	rw_enter(&db->td_lock, RW_WRITER);
	nextsa = list_head(&db->td_salist);
	while (nextsa != NULL) {
		tcpsig_sa_t *sa = nextsa;

		nextsa = list_next(&db->td_salist, sa);

		mutex_enter(&sa->ts_lock);
		if (sa->ts_refcnt != 0) {
			mutex_exit(&sa->ts_lock);
			continue;
		}

		list_remove(&db->td_salist, sa);

		mutex_exit(&sa->ts_lock);
		tcpsig_sa_free(sa);
	}

	rw_exit(&db->td_lock);

	return (0);
}

static int
tcpsig_sa_get(ip_stack_t *ipst, keysock_in_t *ksi, sadb_ext_t **extv,
    int *diagp)
{
	tcpsig_db_t *db;
	sadb_address_t *srcext, *dstext;
	struct sockaddr_storage *src, *dst;
	tcpsig_sa_t *sa;

	srcext = (sadb_address_t *)extv[SADB_EXT_ADDRESS_SRC];
	dstext = (sadb_address_t *)extv[SADB_EXT_ADDRESS_DST];

	if (srcext == NULL) {
		*diagp = SADB_X_DIAGNOSTIC_MISSING_SRC;
		return (EINVAL);
	}

	if (dstext == NULL) {
		*diagp = SADB_X_DIAGNOSTIC_MISSING_DST;
		return (EINVAL);
	}

	src = (struct sockaddr_storage *)(srcext + 1);
	dst = (struct sockaddr_storage *)(dstext + 1);

	sa = tcpsig_sa_find(src, dst, ipst);

	if (sa == NULL)
		return (ENOENT);

	tcpsig_sa_rele(sa);

	//XXX

	return (EOPNOTSUPP);
}

static int
tcpsig_sa_add(ip_stack_t *ipst, keysock_in_t *ksi, sadb_ext_t **extv,
    int *diagp)
{
	tcpsig_db_t *db;
	sadb_address_t *srcext, *dstext;
	struct sockaddr_storage *src, *dst;
	sadb_key_t *key;
	tcpsig_sa_t *sa, *dupsa;
	int ret = 0;

	srcext = (sadb_address_t *)extv[SADB_EXT_ADDRESS_SRC];
	dstext = (sadb_address_t *)extv[SADB_EXT_ADDRESS_DST];
	key = (sadb_key_t *)extv[SADB_X_EXT_STR_AUTH];

	if (srcext == NULL) {
		*diagp = SADB_X_DIAGNOSTIC_MISSING_SRC;
		return (EINVAL);
	}

	if (dstext == NULL) {
		*diagp = SADB_X_DIAGNOSTIC_MISSING_DST;
		return (EINVAL);
	}

	if (key == NULL) {
		*diagp = SADB_X_DIAGNOSTIC_MISSING_ASTR;
		return (EINVAL);
	}

	src = (struct sockaddr_storage *)(srcext + 1);
	dst = (struct sockaddr_storage *)(dstext + 1);

	if (src->ss_family != dst->ss_family) {
		*diagp = SADB_X_DIAGNOSTIC_AF_MISMATCH;
		return (EINVAL);
	}

	if (src->ss_family != AF_INET && src->ss_family != AF_INET6) {
		*diagp = SADB_X_DIAGNOSTIC_BAD_SRC_AF;
		return (EINVAL);
	}

	db = tcpsig_db(ipst);

	if ((dupsa = tcpsig_sa_find(src, dst, ipst)) != NULL) {
		tcpsig_sa_rele(dupsa);
		*diagp = SADB_X_DIAGNOSTIC_DUPLICATE_SA;
		return (EEXIST);
	}

	sa = kmem_zalloc(sizeof (*sa), KM_NOSLEEP);
	if (sa == NULL)
		return (ENOMEM);
	sa->ts_family = src->ss_family;
	if (sa->ts_family == AF_INET6) {
		bcopy(src, (sin6_t *)&sa->ts_src, sizeof (sin6_t));
		bcopy(dst, (sin6_t *)&sa->ts_dst, sizeof (sin6_t));
	} else {
		bcopy(src, (sin_t *)&sa->ts_src, sizeof (sin_t));
		bcopy(dst, (sin_t *)&sa->ts_dst, sizeof (sin_t));
	}

	sa->ts_key.sak_keylen = key->sadb_key_bits >> 3;
	sa->ts_key.sak_keybits = key->sadb_key_bits;

	sa->ts_key.sak_key = kmem_zalloc(sa->ts_key.sak_keylen, KM_NOSLEEP);
	if (sa->ts_key.sak_key == NULL) {
		kmem_free(sa, sizeof (*sa));
		return (ENOMEM);
	}
	bcopy(key + 1, sa->ts_key.sak_key, sa->ts_key.sak_keylen);
	bzero(key + 1, sa->ts_key.sak_keylen);

	mutex_init(&sa->ts_lock, NULL, MUTEX_DEFAULT, NULL);
	sa->ts_refcnt = 0;

	rw_enter(&db->td_lock, RW_WRITER);
	if ((dupsa = tcpsig_sa_find_held(src, dst, ipst)) != NULL) {
		/* Someone beat us to the addition */
		tcpsig_sa_rele(dupsa);
		tcpsig_sa_free(sa);
		*diagp = SADB_X_DIAGNOSTIC_DUPLICATE_SA;
		ret = EEXIST;
	} else {
		list_insert_tail(&db->td_salist, sa);
	}
	rw_exit(&db->td_lock);

	return (ret);
}

static void
tcpsig_pseudo_compute4(tcp_t *tcp, int tcplen, MD5_CTX *ctx, bool inbound)
{
	struct ip_pseudo {
		struct in_addr	ipp_src;
		struct in_addr	ipp_dst;
		uint8_t		ipp_pad;
		uint8_t		ipp_proto;
		uint16_t	ipp_len;
	} ipp;
	conn_t *connp = tcp->tcp_connp;

	if (inbound) {
		ipp.ipp_src.s_addr = connp->conn_faddr_v4;
		ipp.ipp_dst.s_addr = connp->conn_saddr_v4;
	} else {
		ipp.ipp_src.s_addr = connp->conn_saddr_v4;
		ipp.ipp_dst.s_addr = connp->conn_faddr_v4;
	}
	ipp.ipp_pad = 0;
        ipp.ipp_proto = IPPROTO_TCP;
        ipp.ipp_len = htons(tcplen);

	DTRACE_PROBE1(ipp4, struct ip_pseudo *, &ipp);

        MD5Update(ctx, (char *)&ipp, sizeof(ipp));
}

static void
tcpsig_pseudo_compute6(tcp_t *tcp, int tcplen, MD5_CTX *ctx, bool inbound)
{
        struct ip6_pseudo {
		struct in6_addr	ipp_src;
		struct in6_addr ipp_dst;
                uint32_t	ipp_len;
                uint32_t	ipp_nxt;
        } ip6p;
	conn_t *connp = tcp->tcp_connp;

	if (inbound) {
		ip6p.ipp_src = connp->conn_faddr_v6;
		ip6p.ipp_dst = connp->conn_saddr_v6;
	} else {
		ip6p.ipp_src = connp->conn_saddr_v6;
		ip6p.ipp_dst = connp->conn_faddr_v6;
	}
        ip6p.ipp_len = htons(tcplen);
        ip6p.ipp_nxt = htonl(IPPROTO_TCP);

	DTRACE_PROBE1(ipp6, struct ip6_pseudo *, &ip6p);

        MD5Update(ctx, (char *)&ip6p, sizeof(ip6p));
}

bool
tcpsig_signature(mblk_t *mp, tcp_t *tcp, tcpha_t *tcpha, int tcplen,
    uint8_t *digest, bool inbound)
{
	ip_stack_t *ipst = tcp->tcp_tcps->tcps_netstack->netstack_ip;
	conn_t *connp = tcp->tcp_connp;
	tcpsig_sa_t *sa;
	MD5_CTX context;	// Use crypto_digest_?

	/*
	 * The TCP_MD5SIG option is 20 bytes, including padding, which adds 5
	 * 32-bit words to the header's 4-bit field. Check that it can fit in
	 * the current packet.
	 */
	if (!inbound && (tcpha->tha_offset_and_reserved >> 4) > 10) {
		TCP_STAT(tcp->tcp_tcps, tcp_sig_no_space);
		return (false);
	}

	sa = inbound ? tcp->tcp_sig_sa_in : tcp->tcp_sig_sa_out;
	if (sa == NULL) {
		struct sockaddr_storage src, dst;

		if (connp->conn_ipversion == IPV6_VERSION) {
			sin6_t *sin6;

			sin6 = (sin6_t *)&src;
			*sin6 = sin6_null;
			sin6->sin6_family = AF_INET6;
			if (inbound) {
				sin6->sin6_addr = connp->conn_faddr_v6;
				sin6->sin6_port = connp->conn_fport;
			} else {
				sin6->sin6_addr = connp->conn_saddr_v6;
				sin6->sin6_port = connp->conn_lport;
			}

			sin6 = (sin6_t *)&dst;
			*sin6 = sin6_null;
			sin6->sin6_family = AF_INET6;
			if (inbound) {
				sin6->sin6_addr = connp->conn_saddr_v6;
				sin6->sin6_port = connp->conn_lport;
			} else {
				sin6->sin6_addr = connp->conn_faddr_v6;
				sin6->sin6_port = connp->conn_fport;
			}
		} else {
			sin_t *sin;

			sin = (sin_t *)&src;
			*sin = sin_null;
			sin->sin_family = AF_INET;
			if (inbound) {
				sin->sin_addr.s_addr = connp->conn_faddr_v4;
				sin->sin_port = connp->conn_fport;
			} else {
				sin->sin_addr.s_addr = connp->conn_saddr_v4;
				sin->sin_port = connp->conn_lport;
			}

			sin = (sin_t *)&dst;
			*sin = sin_null;
			sin->sin_family = AF_INET;
			if (inbound) {
				sin->sin_addr.s_addr = connp->conn_saddr_v4;
				sin->sin_port = connp->conn_lport;
			} else {
				sin->sin_addr.s_addr = connp->conn_faddr_v4;
				sin->sin_port = connp->conn_fport;
			}
		}

		sa = tcpsig_sa_find(&src, &dst, ipst);

		if (sa == NULL) {
			TCP_STAT(tcp->tcp_tcps, tcp_sig_match_failed);
			return (false);
		}

		/*
		 * tcpsig_sa_find() returns a held SA, so we don't need to take
		 * another one before adding it to tcp.
		 */
		if (inbound)
			tcp->tcp_sig_sa_in = sa;
		else
			tcp->tcp_sig_sa_out = sa;
	}

	tcpsig_sa_hold(sa);

	/* We have a key for this connection, generate the hash */
	MD5Init(&context);

	/* TCP pseudo-header */
	if (connp->conn_ipversion == IPV6_VERSION)
		tcpsig_pseudo_compute6(tcp, tcplen, &context, inbound);
	else
		tcpsig_pseudo_compute4(tcp, tcplen, &context, inbound);

	/* TCP header, excluding options and with a zero checksum */
	uint16_t offset = tcpha->tha_offset_and_reserved;
	uint16_t sum = tcpha->tha_sum;

	if (!inbound) {
		/* Account for the MD5 option we are going to add */
		tcpha->tha_offset_and_reserved += (5 << 4);
	}
	tcpha->tha_sum = 0;
	MD5Update(&context, tcpha, sizeof (*tcpha));
	tcpha->tha_offset_and_reserved = offset;
	tcpha->tha_sum = sum;

	/* TCP segment data */
	for (; mp != NULL; mp = mp->b_cont) {
		if (DB_TYPE(mp) != M_DATA)
			continue;
		MD5Update(&context, mp->b_rptr, mp->b_wptr - mp->b_rptr);
	}

	/* Connection-specific key */
	MD5Update(&context, sa->ts_key.sak_key, sa->ts_key.sak_keylen);
	tcpsig_sa_rele(sa);

	MD5Final(digest, &context);

	return (true);
}

bool
tcpsig_verify(mblk_t *mp, tcp_t *tcp, tcpha_t *tcpha, ip_recv_attr_t *ira,
    uint8_t *digest)
{
	uint8_t calc_digest[MD5_DIGEST_LENGTH];

	if (!tcpsig_signature(mp, tcp, tcpha,
	    ira->ira_pktlen - ira->ira_ip_hdr_length, calc_digest, true)) {
		/* The appropriate stat will already have been bumped */
		return (false);
	}

	if (bcmp(digest, calc_digest, sizeof (calc_digest)) != 0) {
		TCP_STAT(tcp->tcp_tcps, tcp_sig_verify_failed);
		return (false);
	}

	return (true);
}

static uint8_t *
tcpsig_make_addr_ext(uint8_t *start, uint8_t *end, uint16_t exttype,
    sa_family_t af, struct sockaddr_storage *addr)
{
	sin_t *sin;
	sin6_t *sin6;
	uint8_t *cur = start;
	int addrext_len;
	int sin_len;
	sadb_address_t *addrext	= (sadb_address_t *)cur;

	if (cur == NULL)
		return (NULL);

	cur += sizeof (*addrext);
	if (cur > end)
		return (NULL);

	addrext->sadb_address_proto = IPPROTO_TCP;
	addrext->sadb_address_prefixlen = 32;
	addrext->sadb_address_reserved = 0;
	addrext->sadb_address_exttype = exttype;

	switch (af) {
	case AF_INET:
		sin = (sin_t *)cur;
		sin_len = sizeof (*sin);
		cur += sin_len;
		if (cur > end)
			return (NULL);

		bzero(sin->sin_zero, sizeof (sin->sin_zero));
		bcopy(addr, sin, sizeof (*sin));
		break;
	case AF_INET6:
		sin6 = (sin6_t *)cur;
		sin_len = sizeof (*sin6);
		cur += sin_len;
		if (cur > end)
			return (NULL);

		bzero(sin6, sizeof (*sin6));
		bcopy(addr, sin6, sizeof (*sin6));
		break;
	}

	addrext_len = roundup(cur - start, sizeof (uint64_t));
	addrext->sadb_address_len = SADB_8TO64(addrext_len);

	cur = start + addrext_len;
	if (cur > end)
		cur = NULL;

	return (cur);
}

#if 0
static mblk_t *
tcpsig_dump_one(tcpsig_sa_t *sa, sadb_msg_t *samsg)
{
	size_t alloclen, addrsize, keysize;
	uint8_t *cur, *end;
	sadb_key_t *key;
	mblk_t *mp;

	alloclen = sizeof (sadb_msg_t);

	keysize = roundup(sizeof (sadb_key_t) + sa->ts_key.sak_keylen,
	    sizeof (uint64_t));
	alloclen += keysize;

	switch (sa->ts_family) {
	case AF_INET:
		addrsize = roundup(sizeof (sin_t) +
		    sizeof (sadb_address_t), sizeof (uint64_t));
		break;
	case AF_INET6:
		addrsize = roundup(sizeof (sin6_t) +
		    sizeof (sadb_address_t), sizeof (uint64_t));
		break;
	}
	alloclen += addrsize * 2;

	mp = allocb(alloclen, BPRI_HI);
	if (mp == NULL)
		return (mp);

	bzero(mp->b_rptr, alloclen);
	mp->b_wptr += alloclen;
	end = mp->b_wptr;

	newsamsg = (sadb_msg_t *)mp->b_rptr;
	*newsamsg = *samsg;
	newsamsg->sadb_msg_len = (uint16_t)SADB_8TO64(alloclen);

	cur = (uint8_t *)(newsamsg + 1);
	cur = tcpsig_make_addr_ext(cur, end, SADB_EXT_ADDRESS_SRC,
	    sa->ts_family, &sa->ts_src);
	cur = tcpsig_make_addr_ext(cur, end, SADB_EXT_ADDRESS_DST,
	    sa->ts_family, &sa->ts_dst);

	key = (sadb_key_t *)cur;
	key->sadb_key_exttype = SADB_X_EXT_STR_AUTH;
	key->sadb_key_bits = sa->ts_key.sak_keybits;
	key->sadb_key_len = SADB_8TO64(key->sadb_key_bits);
	key->sadb_key_reserved = 0;
	bcopy(sa->ts_key.sak_key, key + 1, sa->ts_key.sak_keylen);

	return (mp);
}

static int
tcpsig_sa_dump(ip_stack_t *ipst, sadb_msg_t *samsg, int *diag)
{
	tcpsig_db_t *db;
	tcpsig_sa_t *sa;
	int ret = 0;

	db = tcpsig_db(ipst);
	rw_enter(&db->td_lock, RW_READER);

	for (sa = list_head(&db->td_salist); sa != NULL;
	    sa = list_next(&db->td_salist, sa)) {
		mblkt_t *answer;

		answer = dupb(original_answer);
		if (answer == NULL)
			return (ENOBUFS);
		answer->b_cont = tcpsig_dump_one(sa, samsg);
		if (answer->b_conf == NULL) {
			freeb(answer);
			return (ENOMEM);
		}
		//putnext(pfkey_q, answer);
	}

out:

	rw_exit(&db->td_lock);

	return (ret);
}
#endif

void
tcpsig_sa_handler(keysock_t *ks, mblk_t *mp, sadb_msg_t *samsg,
    sadb_ext_t **extv)
{
	keysock_stack_t *keystack = ks->keysock_keystack;
	netstack_t *nst = keystack->keystack_netstack;
	ip_stack_t *ipst = nst->netstack_ip;
	keysock_in_t *ksi = (keysock_in_t *)mp->b_rptr;
	int diag = SADB_X_DIAGNOSTIC_NONE;
	int error;

	switch (samsg->sadb_msg_type) {
	case SADB_ADD:
		error = tcpsig_sa_add(ipst, ksi, extv, &diag);
		keysock_error(ks, mp, error, diag);
		break;
	case SADB_GET:
		error = tcpsig_sa_get(ipst, ksi, extv, &diag);
		keysock_error(ks, mp, error, diag);
		break;
	case SADB_FLUSH:
		error = tcpsig_sa_flush(ipst, &diag);
		keysock_error(ks, mp, error, diag);
		break;
	case SADB_DUMP:
#if 0
		error = tcpsig_sa_dump(ipst, &diag);
		keysock_error(ks, mp, error, diag);
		break;
#endif
	default:
		keysock_error(ks, mp, EOPNOTSUPP, diag);
		break;
	}
}


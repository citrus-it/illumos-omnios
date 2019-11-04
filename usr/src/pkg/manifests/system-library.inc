#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright 2012 OmniTI Computer Consulting, Inc.  All rights reserved.
# Copyright (c) 2013 Gary Mills
# Copyright 2017 Nexenta Systems, Inc.
# Copyright 2017 RackTop Systems.
# Copyright 2018, Joyent, Inc.
#

<include system-library.man3.inc>
<include system-library.man3avl.inc>
<include system-library.man3c.inc>
<include system-library.man3c_db.inc>
<include system-library.man3cfgadm.inc>
<include system-library.man3commputil.inc>
<include system-library.man3contract.inc>
<include system-library.man3devid.inc>
<include system-library.man3devinfo.inc>
<include system-library.man3dlpi.inc>
<include system-library.man3elf.inc>
<include system-library.man3exacct.inc>
<include system-library.man3ext.inc>
<include system-library.man3fstyp.inc>
<include system-library.man3gen.inc>
<include system-library.man3kstat.inc>
<include system-library.man3kvm.inc>
<include system-library.man3ldap.inc>
<include system-library.man3lgrp.inc>
<include system-library.man3lib.inc>
<include system-library.man3mail.inc>
<include system-library.man3malloc.inc>
<include system-library.man3mp.inc>
<include system-library.man3nsl.inc>
<include system-library.man3nvpair.inc>
<include system-library.man3ofmt.inc>
<include system-library.man3pam.inc>
<include system-library.man3proc.inc>
<include system-library.man3project.inc>
<include system-library.man3resolv.inc>
<include system-library.man3rpc.inc>
<include system-library.man3scf.inc>
<include system-library.man3sec.inc>
<include system-library.man3secdb.inc>
<include system-library.man3sip.inc>
<include system-library.man3socket.inc>
<include system-library.man3sysevent.inc>
<include system-library.man3uuid.inc>
<include system-library.man3volmgt.inc>
<include system-library.man3xnet.inc>
<include system-library.man4.inc>
<include system-library.man5.inc>
<include system-library.man7p.inc>
dir path=etc group=sys
dir path=etc/flash group=sys
dir path=etc/flash/postcreation group=sys mode=0700
dir path=etc/flash/precreation group=sys mode=0700
dir path=etc/flash/preexit group=sys mode=0700
dir path=lib
dir path=lib/$(ARCH64)
dir path=lib/$(ARCH32)
dir path=lib/crypto
dir path=lib/crypto/$(ARCH64)
dir path=lib/crypto/$(ARCH32)
dir path=lib/mpxio
dir path=lib/secure
dir path=lib/secure/$(ARCH64)
dir path=lib/secure/$(ARCH32)
dir path=usr group=sys
dir path=usr/bin
dir path=usr/lib
dir path=usr/lib/$(ARCH64)
dir path=usr/lib/$(ARCH32)
dir path=usr/lib/cfgadm
dir path=usr/lib/cfgadm/$(ARCH64)
dir path=usr/lib/iconv/$(ARCH32)
dir path=usr/lib/lwp
dir path=usr/lib/lwp/$(ARCH64)
dir path=usr/lib/lwp/$(ARCH32)
dir path=usr/lib/scsi
dir path=usr/lib/scsi/$(ARCH32)
dir path=usr/lib/scsi/plugins
dir path=usr/lib/scsi/plugins/scsi
dir path=usr/lib/scsi/plugins/scsi/engines
dir path=usr/lib/scsi/plugins/scsi/engines/$(ARCH32)
dir path=usr/lib/scsi/plugins/ses
dir path=usr/lib/scsi/plugins/ses/framework
dir path=usr/lib/scsi/plugins/ses/framework/$(ARCH32)
dir path=usr/lib/scsi/plugins/ses/vendor
dir path=usr/lib/scsi/plugins/smp
dir path=usr/lib/scsi/plugins/smp/engine
dir path=usr/lib/scsi/plugins/smp/engine/$(ARCH32)
dir path=usr/lib/scsi/plugins/smp/framework
dir path=usr/lib/scsi/plugins/smp/framework/$(ARCH32)
dir path=usr/lib/security
dir path=usr/lib/security/$(ARCH32)
dir path=usr/libexec
dir path=usr/libexec/$(ARCH32)
dir path=usr/share/man
dir path=usr/share/man/man3
dir path=usr/share/man/man3avl
dir path=usr/share/man/man3c
dir path=usr/share/man/man3c_db
dir path=usr/share/man/man3cfgadm
dir path=usr/share/man/man3commputil
dir path=usr/share/man/man3contract
dir path=usr/share/man/man3devid
dir path=usr/share/man/man3devinfo
dir path=usr/share/man/man3dlpi
dir path=usr/share/man/man3elf
dir path=usr/share/man/man3exacct
dir path=usr/share/man/man3ext
dir path=usr/share/man/man3fstyp
dir path=usr/share/man/man3gen
dir path=usr/share/man/man3kstat
dir path=usr/share/man/man3kvm
dir path=usr/share/man/man3ldap
dir path=usr/share/man/man3lgrp
dir path=usr/share/man/man3lib
dir path=usr/share/man/man3mail
dir path=usr/share/man/man3malloc
dir path=usr/share/man/man3mp
dir path=usr/share/man/man3nsl
dir path=usr/share/man/man3nvpair
dir path=usr/share/man/man3ofmt
dir path=usr/share/man/man3pam
dir path=usr/share/man/man3pool
dir path=usr/share/man/man3proc
dir path=usr/share/man/man3project
dir path=usr/share/man/man3resolv
dir path=usr/share/man/man3rpc
dir path=usr/share/man/man3scf
dir path=usr/share/man/man3sec
dir path=usr/share/man/man3secdb
dir path=usr/share/man/man3sip
dir path=usr/share/man/man3socket
dir path=usr/share/man/man3sysevent
dir path=usr/share/man/man3uuid
dir path=usr/share/man/man3volmgt
dir path=usr/share/man/man3xnet
dir path=usr/share/man/man5
dir path=usr/share/man/man7p
file path=etc/flash/precreation/caplib group=sys mode=0500
file path=lib/$(ARCH32)/libadm.so.1
file path=lib/$(ARCH32)/libaio.so.1
file path=lib/$(ARCH32)/libavl.so.1
file path=lib/$(ARCH32)/libc.so.1
file path=lib/$(ARCH32)/libc_db.so.1
file path=lib/$(ARCH32)/libcmdutils.so.1
file path=lib/$(ARCH32)/libcontract.so.1
file path=lib/$(ARCH32)/libcryptoutil.so.1
file path=lib/$(ARCH32)/libctf.so.1
file path=lib/$(ARCH32)/libcustr.so.1
file path=lib/$(ARCH32)/libdevice.so.1
file path=lib/$(ARCH32)/libdevid.so.1
file path=lib/$(ARCH32)/libdevinfo.so.1
file path=lib/$(ARCH32)/libdhcpagent.so.1
file path=lib/$(ARCH32)/libdhcputil.so.1
file path=lib/$(ARCH32)/libdl.so.1
file path=lib/$(ARCH32)/libdladm.so.1
file path=lib/$(ARCH32)/libdlpi.so.1
file path=lib/$(ARCH32)/libelfsign.so.1
file path=lib/$(ARCH32)/libdoor.so.1
file path=lib/$(ARCH32)/libdwarf.so.1
file path=lib/$(ARCH32)/libefi.so.1
file path=lib/$(ARCH32)/libelf.so.1
file path=lib/$(ARCH32)/libfdisk.so.1
file path=lib/$(ARCH32)/libgen.so.1
file path=lib/$(ARCH32)/libinetutil.so.1
file path=lib/$(ARCH32)/libintl.so.1
file path=lib/$(ARCH32)/libipadm.so.1
file path=lib/$(ARCH32)/libipmp.so.1
file path=lib/$(ARCH32)/libkcfd.so.1
file path=lib/$(ARCH32)/libkmf.so.1
file path=lib/$(ARCH32)/libkmfberder.so.1
file path=lib/$(ARCH32)/libkstat.so.1
file path=lib/$(ARCH32)/libld.so.4
file path=lib/$(ARCH32)/liblddbg.so.4
file path=lib/$(ARCH32)/libmd.so.1
file path=lib/$(ARCH32)/libmd5.so.1
file path=lib/$(ARCH32)/libmp.so.1
file path=lib/$(ARCH32)/libmp.so.2
file path=lib/$(ARCH32)/libnsl.so.1
file path=lib/$(ARCH32)/libnvpair.so.1
file path=lib/$(ARCH32)/libnwam.so.1
file path=lib/$(ARCH32)/libofmt.so.1
file path=lib/$(ARCH32)/libpam.so.1
file path=lib/$(ARCH32)/libproc.so.1
file path=lib/$(ARCH32)/libpthread.so.1
file path=lib/$(ARCH32)/librcm.so.1
file path=lib/$(ARCH32)/libresolv.so.1
file path=lib/$(ARCH32)/libresolv.so.2
file path=lib/$(ARCH32)/librestart.so.1
file path=lib/$(ARCH32)/librpcsvc.so.1
file path=lib/$(ARCH32)/librt.so.1
file path=lib/$(ARCH32)/librtld.so.1
file path=lib/$(ARCH32)/librtld_db.so.1
file path=lib/$(ARCH32)/libscf.so.1
file path=lib/$(ARCH32)/libsec.so.1
file path=lib/$(ARCH32)/libsecdb.so.1
file path=lib/$(ARCH32)/libsendfile.so.1
file path=lib/$(ARCH32)/libsqlite-sys.so.2.8.15
file path=lib/$(ARCH32)/libsysevent.so.1
file path=lib/$(ARCH32)/libthread.so.1
file path=lib/$(ARCH32)/libumem.so.1
file path=lib/$(ARCH32)/libuuid.so.1
file path=lib/$(ARCH32)/libw.so.1
file path=lib/$(ARCH32)/nss_compat.so.1
file path=lib/$(ARCH32)/nss_dns.so.1
file path=lib/$(ARCH32)/nss_files.so.1
file path=lib/$(ARCH32)/nss_nis.so.1
file path=lib/$(ARCH32)/nss_user.so.1
file path=lib/crypto/$(ARCH32)/kmf_mapper_cn.so.1
file path=lib/crypto/$(ARCH32)/kmf_nss.so.1
file path=lib/crypto/$(ARCH32)/kmf_openssl.so.1
file path=lib/crypto/$(ARCH32)/kmf_pkcs11.so.1
file path=lib/crypto/kmf_mapper_cn.so.1
file path=lib/crypto/kmf_nss.so.1
file path=lib/crypto/kmf_openssl.so.1
file path=lib/crypto/kmf_pkcs11.so.1
file path=lib/libadm.so.1
file path=lib/libaio.so.1
file path=lib/libavl.so.1
file path=lib/libc.so.1 reboot-needed=true
file path=lib/libc_db.so.1
file path=lib/libcmdutils.so.1
file path=lib/libcontract.so.1
file path=lib/libcryptoutil.so.1
file path=lib/libctf.so.1
file path=lib/libcustr.so.1
file path=lib/libdevice.so.1
file path=lib/libdevid.so.1
file path=lib/libdevinfo.so.1
file path=lib/libdhcputil.so.1
file path=lib/libdl.so.1
file path=lib/libdladm.so.1
file path=lib/libdlpi.so.1
file path=lib/libdoor.so.1
file path=lib/libdwarf.so.1
file path=lib/libefi.so.1
file path=lib/libelf.so.1
file path=lib/libfdisk.so.1
file path=lib/libgen.so.1
file path=lib/libinetutil.so.1
file path=lib/libintl.so.1
file path=lib/libkmf.so.1
file path=lib/libkmfberder.so.1
file path=lib/libkstat.so.1
file path=lib/libld.so.4
file path=lib/liblddbg.so.4
file path=lib/libmd.so.1
file path=lib/libmd5.so.1
file path=lib/libmp.so.2
file path=lib/libnsl.so.1
file path=lib/libnvpair.so.1
file path=lib/libofmt.so.1
file path=lib/libpam.so.1
file path=lib/libproc.so.1
file path=lib/libpthread.so.1
file path=lib/librcm.so.1
file path=lib/libresolv.so.2
file path=lib/librestart.so.1
file path=lib/librpcsvc.so.1
file path=lib/librt.so.1
file path=lib/librtld.so.1
file path=lib/librtld_db.so.1
file path=lib/libscf.so.1
file path=lib/libsec.so.1
file path=lib/libsecdb.so.1
file path=lib/libsendfile.so.1
file path=lib/libsysevent.so.1
file path=lib/libthread.so.1
file path=lib/libumem.so.1
file path=lib/libuuid.so.1
file path=lib/libw.so.1
file path=lib/mpxio/stmsboot_util mode=0555
file path=lib/nss_compat.so.1
file path=lib/nss_dns.so.1
file path=lib/nss_files.so.1
file path=lib/nss_nis.so.1
file path=lib/nss_user.so.1
file path=usr/lib/$(ARCH32)/extendedFILE.so.1
file path=usr/lib/$(ARCH32)/getloginx.so.1
file path=usr/lib/$(ARCH32)/libads.so.1
file path=usr/lib/$(ARCH32)/libadutils.so.1
file path=usr/lib/$(ARCH32)/libbsdmalloc.so.1
file path=usr/lib/$(ARCH32)/libcfgadm.so.1
file path=usr/lib/$(ARCH32)/libcommputil.so.1
file path=usr/lib/$(ARCH32)/libcrle.so.1
file path=usr/lib/$(ARCH32)/libcrypt.so.1
file path=usr/lib/$(ARCH32)/libdemangle-sys.so.1 mode=0555
file path=usr/lib/$(ARCH32)/libdisasm.so.1
file path=usr/lib/$(ARCH32)/libexacct.so.1
file path=usr/lib/$(ARCH32)/libfstyp.so.1
file path=usr/lib/$(ARCH32)/libhotplug.so.1
file path=usr/lib/$(ARCH32)/libidmap.so.1
file path=usr/lib/$(ARCH32)/libinetsvc.so.1
file path=usr/lib/$(ARCH32)/libipmi.so.1
file path=usr/lib/$(ARCH32)/libipp.so.1
file path=usr/lib/$(ARCH32)/libipsecutil.so.1
file path=usr/lib/$(ARCH32)/libkvm.so.1
file path=usr/lib/$(ARCH32)/libl.so.1
file path=usr/lib/$(ARCH32)/libldap.so.5
file path=usr/lib/$(ARCH32)/liblgrp.so.1
file path=usr/lib/$(ARCH32)/liblm.so.1
file path=usr/lib/$(ARCH32)/libmail.so.1
file path=usr/lib/$(ARCH32)/libmalloc.so.1
file path=usr/lib/$(ARCH32)/libmapmalloc.so.1
file path=usr/lib/$(ARCH32)/libmtmalloc.so.1
file path=usr/lib/$(ARCH32)/libnls.so.1
file path=usr/lib/$(ARCH32)/libpcidb.so.1
file path=usr/lib/$(ARCH32)/libpkcs11.so.1
file path=usr/lib/$(ARCH32)/libproject.so.1
file path=usr/lib/$(ARCH32)/libraidcfg.so.1
file path=usr/lib/$(ARCH32)/libreparse.so.1
file path=usr/lib/$(ARCH32)/libsched.so.1
file path=usr/lib/$(ARCH32)/libsctp.so.1
file path=usr/lib/$(ARCH32)/libsff.so.1
file path=usr/lib/$(ARCH32)/libsip.so.1
file path=usr/lib/$(ARCH32)/libsldap.so.1
file path=usr/lib/$(ARCH32)/libsmbios.so.1
file path=usr/lib/$(ARCH32)/libsoftcrypto.so.1
file path=usr/lib/$(ARCH32)/libsys.so.1
file path=usr/lib/$(ARCH32)/libuutil.so.1 mode=0555
file path=usr/lib/$(ARCH32)/libvolmgt.so.1
file path=usr/lib/$(ARCH32)/libwrap.so.1.0
file path=usr/lib/$(ARCH32)/nss_ad.so.1
file path=usr/lib/$(ARCH32)/nss_ldap.so.1
file path=usr/lib/$(ARCH32)/passwdutil.so.1
file path=usr/lib/$(ARCH32)/straddr.so.2
file path=usr/lib/$(ARCH32)/watchmalloc.so.1
file path=usr/lib/cfgadm/$(ARCH64)/ib.so.1
file path=usr/lib/cfgadm/$(ARCH64)/pci.so.1
file path=usr/lib/cfgadm/$(ARCH64)/sata.so.1
file path=usr/lib/cfgadm/$(ARCH64)/scsi.so.1
file path=usr/lib/cfgadm/$(ARCH64)/shp.so.1
file path=usr/lib/cfgadm/$(ARCH64)/usb.so.1
file path=usr/lib/cfgadm/ib.so.1
file path=usr/lib/cfgadm/pci.so.1
file path=usr/lib/cfgadm/sata.so.1
file path=usr/lib/cfgadm/scsi.so.1
file path=usr/lib/cfgadm/shp.so.1
file path=usr/lib/cfgadm/usb.so.1
file path=usr/lib/getloginx.so.1
file path=usr/lib/lib.b mode=0444
file path=usr/lib/libads.so.1
file path=usr/lib/libadutils.so.1
file path=usr/lib/libbsdmalloc.so.1
file path=usr/lib/libcfgadm.so.1
file path=usr/lib/libcommputil.so.1
file path=usr/lib/libcrle.so.1
file path=usr/lib/libcrypt.so.1
file path=usr/lib/libdemangle-sys.so.1 mode=0555
file path=usr/lib/libdisasm.so.1
file path=usr/lib/libexacct.so.1
file path=usr/lib/libfstyp.so.1
file path=usr/lib/libhotplug.so.1
file path=usr/lib/libidmap.so.1
file path=usr/lib/libipmi.so.1
file path=usr/lib/libipp.so.1
file path=usr/lib/libipsecutil.so.1
file path=usr/lib/libkvm.so.1
file path=usr/lib/libl.so.1
file path=usr/lib/libldap.so.5
file path=usr/lib/liblgrp.so.1
file path=usr/lib/liblm.so.1
file path=usr/lib/libmail.so.1
file path=usr/lib/libmalloc.so.1
file path=usr/lib/libmapmalloc.so.1
file path=usr/lib/libmtmalloc.so.1
file path=usr/lib/libnls.so.1
file path=usr/lib/libpcidb.so.1
file path=usr/lib/libpkcs11.so.1
file path=usr/lib/libproject.so.1
file path=usr/lib/libraidcfg.so.1
file path=usr/lib/libreparse.so.1
file path=usr/lib/libsaveargs.so.1
file path=usr/lib/libsched.so.1
file path=usr/lib/libsctp.so.1
file path=usr/lib/libsff.so.1
file path=usr/lib/libsip.so.1
file path=usr/lib/libsldap.so.1
file path=usr/lib/libsmbios.so.1
file path=usr/lib/libsoftcrypto.so.1
file path=usr/lib/libuutil.so.1 mode=0555
file path=usr/lib/libvolmgt.so.1
file path=usr/lib/libwrap.so.1.0
file path=usr/lib/nss_ad.so.1
file path=usr/lib/nss_ldap.so.1
file path=usr/lib/passwdutil.so.1
file path=usr/lib/scsi/$(ARCH32)/libscsi.so.1
file path=usr/lib/scsi/$(ARCH32)/libses.so.1
file path=usr/lib/scsi/$(ARCH32)/libsmp.so.1
file path=usr/lib/scsi/libscsi.so.1
file path=usr/lib/scsi/libses.so.1
file path=usr/lib/scsi/libsmp.so.1
file path=usr/lib/scsi/plugins/scsi/engines/$(ARCH32)/uscsi.so
file path=usr/lib/scsi/plugins/scsi/engines/uscsi.so
file path=usr/lib/scsi/plugins/ses/framework/$(ARCH32)/libses.so
file path=usr/lib/scsi/plugins/ses/framework/$(ARCH32)/ses2.so
file path=usr/lib/scsi/plugins/ses/framework/libses.so
file path=usr/lib/scsi/plugins/ses/framework/ses2.so
file path=usr/lib/scsi/plugins/smp/engine/$(ARCH32)/usmp.so
file path=usr/lib/scsi/plugins/smp/engine/usmp.so
file path=usr/lib/scsi/plugins/smp/framework/$(ARCH32)/sas2.so
file path=usr/lib/scsi/plugins/smp/framework/sas2.so
file path=usr/lib/security/$(ARCH64)/crypt_bsdbf.so.1
file path=usr/lib/security/$(ARCH64)/crypt_bsdmd5.so.1
file path=usr/lib/security/$(ARCH64)/crypt_sha256.so.1
file path=usr/lib/security/$(ARCH64)/crypt_sha512.so.1
file path=usr/lib/security/$(ARCH64)/crypt_sunmd5.so.1
file path=usr/lib/security/$(ARCH64)/pam_allow.so.1
file path=usr/lib/security/$(ARCH64)/pam_authtok_check.so.1
file path=usr/lib/security/$(ARCH64)/pam_authtok_get.so.1
file path=usr/lib/security/$(ARCH64)/pam_authtok_store.so.1
file path=usr/lib/security/$(ARCH64)/pam_deny.so.1
file path=usr/lib/security/$(ARCH64)/pam_dhkeys.so.1
file path=usr/lib/security/$(ARCH64)/pam_dial_auth.so.1
file path=usr/lib/security/$(ARCH64)/pam_ldap.so.1
file path=usr/lib/security/$(ARCH64)/pam_list.so.1
file path=usr/lib/security/$(ARCH64)/pam_passwd_auth.so.1
file path=usr/lib/security/$(ARCH64)/pam_roles.so.1
file path=usr/lib/security/$(ARCH64)/pam_sample.so.1
file path=usr/lib/security/$(ARCH64)/pam_timestamp.so.1
file path=usr/lib/security/$(ARCH64)/pam_unix_account.so.1
file path=usr/lib/security/$(ARCH64)/pam_unix_auth.so.1
file path=usr/lib/security/$(ARCH64)/pam_unix_cred.so.1
file path=usr/lib/security/$(ARCH64)/pam_unix_session.so.1
file path=usr/lib/security/$(ARCH64)/pkcs11_kernel.so.1
file path=usr/lib/security/$(ARCH64)/pkcs11_softtoken.so.1
file path=usr/lib/security/crypt_bsdbf.so.1
file path=usr/lib/security/crypt_bsdmd5.so.1
file path=usr/lib/security/crypt_sha256.so.1
file path=usr/lib/security/crypt_sha512.so.1
file path=usr/lib/security/crypt_sunmd5.so.1
file path=usr/lib/security/pam_allow.so.1
file path=usr/lib/security/pam_authtok_check.so.1
file path=usr/lib/security/pam_authtok_get.so.1
file path=usr/lib/security/pam_authtok_store.so.1
file path=usr/lib/security/pam_deny.so.1
file path=usr/lib/security/pam_dhkeys.so.1
file path=usr/lib/security/pam_dial_auth.so.1
file path=usr/lib/security/pam_ldap.so.1
file path=usr/lib/security/pam_list.so.1
file path=usr/lib/security/pam_passwd_auth.so.1
file path=usr/lib/security/pam_roles.so.1
file path=usr/lib/security/pam_sample.so.1
file path=usr/lib/security/pam_timestamp.so.1
file path=usr/lib/security/pam_unix_account.so.1
file path=usr/lib/security/pam_unix_auth.so.1
file path=usr/lib/security/pam_unix_cred.so.1
file path=usr/lib/security/pam_unix_session.so.1
file path=usr/lib/security/pkcs11_kernel.so.1
file path=usr/lib/security/pkcs11_softtoken.so.1
file path=usr/lib/straddr.so.2
file path=usr/lib/watchmalloc.so.1
file path=usr/libexec/ld.so mode=0555
file path=usr/libexec/$(ARCH32)/ld.so mode=0555
license cr_Sun license=cr_Sun
license include/sys/THIRDPARTYLICENSE.unicode \
    license=include/sys/THIRDPARTYLICENSE.unicode
license lib/libdemangle-sys/THIRDPARTYLICENSE \
    license=lib/libdemangle-sys/THIRDPARTYLICENSE
license lic_CDDL license=lic_CDDL
license share/man/man3c/THIRDPARTYLICENSE.arc4random \
    license=share/man/man3c/THIRDPARTYLICENSE.arc4random
license share/man/man3c/THIRDPARTYLICENSE.getentropy \
    license=share/man/man3c/THIRDPARTYLICENSE.getentropy
# libwrap is part of tcp wrappers along with tcpd
license usr/src/cmd/tcpd/THIRDPARTYLICENSE \
    license=usr/src/cmd/tcpd/THIRDPARTYLICENSE
license usr/src/common/crypto/THIRDPARTYLICENSE.cryptogams \
    license=usr/src/common/crypto/THIRDPARTYLICENSE.cryptogams
license usr/src/common/crypto/aes/amd64/THIRDPARTYLICENSE.gladman \
    license=usr/src/common/crypto/aes/amd64/THIRDPARTYLICENSE.gladman
license usr/src/common/crypto/aes/amd64/THIRDPARTYLICENSE.openssl \
    license=usr/src/common/crypto/aes/amd64/THIRDPARTYLICENSE.openssl
license usr/src/common/crypto/ecc/THIRDPARTYLICENSE \
    license=usr/src/common/crypto/ecc/THIRDPARTYLICENSE
license usr/src/common/crypto/md5/amd64/THIRDPARTYLICENSE \
    license=usr/src/common/crypto/md5/amd64/THIRDPARTYLICENSE
license usr/src/common/mpi/THIRDPARTYLICENSE \
    license=usr/src/common/mpi/THIRDPARTYLICENSE
license usr/src/lib/libbsdmalloc/THIRDPARTYLICENSE \
    license=usr/src/lib/libbsdmalloc/THIRDPARTYLICENSE
license usr/src/lib/libdwarf/THIRDPARTYLICENSE \
    license=usr/src/lib/libdwarf/THIRDPARTYLICENSE
license usr/src/lib/libinetutil/common/THIRDPARTYLICENSE \
    license=usr/src/lib/libinetutil/common/THIRDPARTYLICENSE
license usr/src/lib/libkmf/THIRDPARTYLICENSE \
    license=usr/src/lib/libkmf/THIRDPARTYLICENSE
license usr/src/lib/libldap5/THIRDPARTYLICENSE \
    license=usr/src/lib/libldap5/THIRDPARTYLICENSE
license usr/src/lib/libmp/common/THIRDPARTYLICENSE \
    license=usr/src/lib/libmp/common/THIRDPARTYLICENSE
license usr/src/lib/libresolv/THIRDPARTYLICENSE \
    license=usr/src/lib/libresolv/THIRDPARTYLICENSE
license usr/src/lib/libresolv2/THIRDPARTYLICENSE \
    license=usr/src/lib/libresolv2/THIRDPARTYLICENSE
license usr/src/lib/pam_modules/authtok_check/THIRDPARTYLICENSE \
    license=usr/src/lib/pam_modules/authtok_check/THIRDPARTYLICENSE
license usr/src/lib/passwdutil/THIRDPARTYLICENSE \
    license=usr/src/lib/passwdutil/THIRDPARTYLICENSE
link path=lib/$(ARCH64)/ld.so.1 target=../../usr/libexec/ld.so
link path=lib/$(ARCH32)/libadm.so target=libadm.so.1
link path=lib/$(ARCH32)/libaio.so target=libaio.so.1
link path=lib/$(ARCH32)/libavl.so target=libavl.so.1
link path=lib/$(ARCH32)/libc.so reboot-needed=true target=libc.so.1
link path=lib/$(ARCH32)/libc_db.so target=libc_db.so.1
link path=lib/$(ARCH32)/libcmdutils.so target=libcmdutils.so.1
link path=lib/$(ARCH32)/libcontract.so target=libcontract.so.1
link path=lib/$(ARCH32)/libcryptoutil.so target=libcryptoutil.so.1
link path=lib/$(ARCH32)/libctf.so target=libctf.so.1
link path=lib/$(ARCH32)/libdevice.so target=libdevice.so.1
link path=lib/$(ARCH32)/libdevid.so target=libdevid.so.1
link path=lib/$(ARCH32)/libdevinfo.so target=libdevinfo.so.1
link path=lib/$(ARCH32)/libdl.so target=libdl.so.1
link path=lib/$(ARCH32)/libdladm.so target=libdladm.so.1
link path=lib/$(ARCH32)/libdlpi.so target=libdlpi.so.1
link path=lib/$(ARCH32)/libdoor.so target=libdoor.so.1
link path=lib/$(ARCH32)/libefi.so target=libefi.so.1
link path=lib/$(ARCH32)/libelf.so target=libelf.so.1
link path=lib/$(ARCH32)/libelfsign.so target=libelfsign.so.1
link path=lib/$(ARCH32)/libfdisk.so target=libfdisk.so.1
link path=lib/$(ARCH32)/libgen.so target=libgen.so.1
link path=lib/$(ARCH32)/libintl.so target=libintl.so.1
link path=lib/$(ARCH32)/libipmp.so target=libipmp.so.1
link path=lib/$(ARCH32)/libkmf.so target=libkmf.so.1
link path=lib/$(ARCH32)/libkmfberder.so target=libkmfberder.so.1
link path=lib/$(ARCH32)/libkstat.so target=libkstat.so.1
link path=lib/$(ARCH32)/libmd.so target=libmd.so.1
link path=lib/$(ARCH32)/libmd5.so target=libmd5.so.1
link path=lib/$(ARCH32)/libmp.so target=libmp.so.2
link path=lib/$(ARCH32)/libnsl.so target=libnsl.so.1
link path=lib/$(ARCH32)/libnvpair.so target=libnvpair.so.1
link path=lib/$(ARCH32)/libnwam.so target=libnwam.so.1
link path=lib/$(ARCH32)/libofmt.so target=libofmt.so.1
link path=lib/$(ARCH32)/libpam.so target=libpam.so.1
link path=lib/$(ARCH32)/libposix4.so target=libposix4.so.1
link path=lib/$(ARCH32)/libposix4.so.1 target=librt.so.1
link path=lib/$(ARCH32)/libproc.so target=libproc.so.1
link path=lib/$(ARCH32)/libpthread.so target=libpthread.so.1
link path=lib/$(ARCH32)/librcm.so target=librcm.so.1
link path=lib/$(ARCH32)/libresolv.so target=libresolv.so.2
link path=lib/$(ARCH32)/librestart.so target=librestart.so.1
link path=lib/$(ARCH32)/librpcsvc.so target=librpcsvc.so.1
link path=lib/$(ARCH32)/librt.so target=librt.so.1
link path=lib/$(ARCH32)/librtld_db.so target=librtld_db.so.1
link path=lib/$(ARCH32)/libscf.so target=libscf.so.1
link path=lib/$(ARCH32)/libsec.so target=libsec.so.1
link path=lib/$(ARCH32)/libsecdb.so target=libsecdb.so.1
link path=lib/$(ARCH32)/libsendfile.so target=libsendfile.so.1
link path=lib/$(ARCH32)/libsysevent.so target=libsysevent.so.1
link path=lib/$(ARCH32)/libthread.so target=libthread.so.1
link path=lib/$(ARCH32)/libthread_db.so target=libc_db.so.1
link path=lib/$(ARCH32)/libthread_db.so.1 target=libc_db.so.1
link path=lib/$(ARCH32)/libumem.so target=libumem.so.1
link path=lib/$(ARCH32)/libuuid.so target=libuuid.so.1
link path=lib/$(ARCH32)/libw.so target=libw.so.1
link path=lib/32 target=$(ARCH32)
link path=lib/64 target=$(ARCH64)
link path=lib/crypto/32 target=$(ARCH32)
link path=lib/crypto/64 target=$(ARCH64)
link path=lib/ld.so.1 target=../usr/libexec/$(ARCH32)/ld.so
link path=lib/libadm.so target=libadm.so.1
link path=lib/libaio.so target=libaio.so.1
link path=lib/libavl.so target=libavl.so.1
link path=lib/libc.so target=libc.so.1
link path=lib/libc_db.so target=libc_db.so.1
link path=lib/libcmdutils.so target=libcmdutils.so.1
link path=lib/libcontract.so target=libcontract.so.1
link path=lib/libcryptoutil.so target=./libcryptoutil.so.1
link path=lib/libctf.so target=libctf.so.1
link path=lib/libdevice.so target=libdevice.so.1
link path=lib/libdevid.so target=libdevid.so.1
link path=lib/libdevinfo.so target=libdevinfo.so.1
link path=lib/libdl.so target=libdl.so.1
link path=lib/libdladm.so target=libdladm.so.1
link path=lib/libdlpi.so target=libdlpi.so.1
link path=lib/libdoor.so target=libdoor.so.1
link path=lib/libefi.so target=libefi.so.1
link path=lib/libelf.so target=libelf.so.1
link path=lib/libfdisk.so target=libfdisk.so.1
link path=lib/libgen.so target=libgen.so.1
link path=lib/libintl.so target=libintl.so.1
link path=lib/libkmf.so target=libkmf.so.1
link path=lib/libkmfberder.so target=libkmfberder.so.1
link path=lib/libkstat.so target=libkstat.so.1
link path=lib/libmd.so target=libmd.so.1
link path=lib/libmd5.so target=libmd5.so.1
link path=lib/libmp.so target=libmp.so.2
link path=lib/libnsl.so target=libnsl.so.1
link path=lib/libnvpair.so target=libnvpair.so.1
link path=lib/libofmt.so target=libofmt.so.1
link path=lib/libpam.so target=libpam.so.1
link path=lib/libposix4.so target=libposix4.so.1
link path=lib/libposix4.so.1 target=librt.so.1
link path=lib/libproc.so target=libproc.so.1
link path=lib/libpthread.so target=libpthread.so.1
link path=lib/librcm.so target=./librcm.so.1
link path=lib/libresolv.so target=libresolv.so.2
link path=lib/librpcsvc.so target=librpcsvc.so.1
link path=lib/librt.so target=librt.so.1
link path=lib/librtld_db.so target=librtld_db.so.1
link path=lib/libscf.so target=libscf.so.1
link path=lib/libsec.so target=libsec.so.1
link path=lib/libsecdb.so target=libsecdb.so.1
link path=lib/libsendfile.so target=libsendfile.so.1
link path=lib/libsysevent.so target=./libsysevent.so.1
link path=lib/libthread.so target=libthread.so.1
link path=lib/libthread_db.so target=libc_db.so.1
link path=lib/libthread_db.so.1 target=libc_db.so.1
link path=lib/libumem.so target=libumem.so.1
link path=lib/libuuid.so target=libuuid.so.1
link path=lib/libw.so target=libw.so.1
link path=lib/secure/32 target=$(ARCH32)
link path=lib/secure/64 target=$(ARCH64)
link path=usr/lib/$(ARCH32)/libadm.so \
    target=../../../lib/$(ARCH32)/libadm.so.1
link path=usr/lib/$(ARCH32)/libadm.so.1 \
    target=../../../lib/$(ARCH32)/libadm.so.1
link path=usr/lib/$(ARCH32)/libads.so target=./libads.so.1
link path=usr/lib/$(ARCH32)/libadutils.so target=./libadutils.so.1
link path=usr/lib/$(ARCH32)/libaio.so \
    target=../../../lib/$(ARCH32)/libaio.so.1
link path=usr/lib/$(ARCH32)/libaio.so.1 \
    target=../../../lib/$(ARCH32)/libaio.so.1
link path=usr/lib/$(ARCH32)/libavl.so \
    target=../../../lib/$(ARCH32)/libavl.so.1
link path=usr/lib/$(ARCH32)/libavl.so.1 \
    target=../../../lib/$(ARCH32)/libavl.so.1
link path=usr/lib/$(ARCH32)/libbsdmalloc.so target=libbsdmalloc.so.1
link path=usr/lib/$(ARCH32)/libc.so target=../../../lib/$(ARCH32)/libc.so.1
link path=usr/lib/$(ARCH32)/libc.so.1 target=../../../lib/$(ARCH32)/libc.so.1
link path=usr/lib/$(ARCH32)/libc_db.so \
    target=../../../lib/$(ARCH32)/libc_db.so.1
link path=usr/lib/$(ARCH32)/libc_db.so.1 \
    target=../../../lib/$(ARCH32)/libc_db.so.1
link path=usr/lib/$(ARCH32)/libcfgadm.so target=libcfgadm.so.1
link path=usr/lib/$(ARCH32)/libcmdutils.so \
    target=../../../lib/$(ARCH32)/libcmdutils.so.1
link path=usr/lib/$(ARCH32)/libcmdutils.so.1 \
    target=../../../lib/$(ARCH32)/libcmdutils.so.1
link path=usr/lib/$(ARCH32)/libcommputil.so target=libcommputil.so.1
link path=usr/lib/$(ARCH32)/libcontract.so \
    target=../../../lib/$(ARCH32)/libcontract.so.1
link path=usr/lib/$(ARCH32)/libcontract.so.1 \
    target=../../../lib/$(ARCH32)/libcontract.so.1
link path=usr/lib/$(ARCH32)/libcrypt.so target=./libcrypt.so.1
link path=usr/lib/$(ARCH32)/libcrypt_d.so target=./libcrypt.so
link path=usr/lib/$(ARCH32)/libcrypt_d.so.1 target=./libcrypt.so.1
link path=usr/lib/$(ARCH32)/libcrypt_i.so target=./libcrypt.so
link path=usr/lib/$(ARCH32)/libcrypt_i.so.1 target=./libcrypt.so.1
link path=usr/lib/$(ARCH32)/libctf.so \
    target=../../../lib/$(ARCH32)/libctf.so.1
link path=usr/lib/$(ARCH32)/libctf.so.1 \
    target=../../../lib/$(ARCH32)/libctf.so.1
link path=usr/lib/$(ARCH32)/libdemangle-sys.so target=./libdemangle-sys.so.1
link path=usr/lib/$(ARCH32)/libdevice.so \
    target=../../../lib/$(ARCH32)/libdevice.so.1
link path=usr/lib/$(ARCH32)/libdevice.so.1 \
    target=../../../lib/$(ARCH32)/libdevice.so.1
link path=usr/lib/$(ARCH32)/libdevid.so \
    target=../../../lib/$(ARCH32)/libdevid.so.1
link path=usr/lib/$(ARCH32)/libdevid.so.1 \
    target=../../../lib/$(ARCH32)/libdevid.so.1
link path=usr/lib/$(ARCH32)/libdevinfo.so \
    target=../../../lib/$(ARCH32)/libdevinfo.so.1
link path=usr/lib/$(ARCH32)/libdevinfo.so.1 \
    target=../../../lib/$(ARCH32)/libdevinfo.so.1
link path=usr/lib/$(ARCH32)/libdhcputil.so.1 \
    target=../../../lib/$(ARCH32)/libdhcputil.so.1
link path=usr/lib/$(ARCH32)/libdisasm.so target=libdisasm.so.1
link path=usr/lib/$(ARCH32)/libdhcpagent.so.1 target=../../....//lib/$(MACH32)/libdhcpagent.so.1
link path=usr/lib/$(ARCH32)/libdl.so target=../../../lib/$(ARCH32)/libdl.so.1
link path=usr/lib/$(ARCH32)/libdl.so.1 \
    target=../../../lib/$(ARCH32)/libdl.so.1
link path=usr/lib/$(ARCH32)/libdlpi.so \
    target=../../../lib/$(ARCH32)/libdlpi.so.1
link path=usr/lib/$(ARCH32)/libdlpi.so.1 \
    target=../../../lib/$(ARCH32)/libdlpi.so.1
link path=usr/lib/$(ARCH32)/libdoor.so \
    target=../../../lib/$(ARCH32)/libdoor.so.1
link path=usr/lib/$(ARCH32)/libdoor.so.1 \
    target=../../../lib/$(ARCH32)/libdoor.so.1
link path=usr/lib/$(ARCH32)/libdwarf.so.1 \
    target=../../../lib/$(ARCH32)/libdwarf.so.1
link path=usr/lib/$(ARCH32)/libefi.so \
    target=../../../lib/$(ARCH32)/libefi.so.1
link path=usr/lib/$(ARCH32)/libefi.so.1 \
    target=../../../lib/$(ARCH32)/libefi.so.1
link path=usr/lib/$(ARCH32)/libelf.so \
    target=../../../lib/$(ARCH32)/libelf.so.1
link path=usr/lib/$(ARCH32)/libelf.so.1 \
    target=../../../lib/$(ARCH32)/libelf.so.1
link path=usr/lib/$(ARCH32)/libexacct.so target=libexacct.so.1
link path=usr/lib/$(ARCH32)/libfdisk.so \
    target=../../../lib/$(ARCH32)/libfdisk.so.1
link path=usr/lib/$(ARCH32)/libfdisk.so.1 \
    target=../../../lib/$(ARCH32)/libfdisk.so.1
link path=usr/lib/$(ARCH32)/libfstyp.so target=./libfstyp.so.1
link path=usr/lib/$(ARCH32)/libgen.so \
    target=../../../lib/$(ARCH32)/libgen.so.1
link path=usr/lib/$(ARCH32)/libgen.so.1 \
    target=../../../lib/$(ARCH32)/libgen.so.1
link path=usr/lib/$(ARCH32)/libhotplug.so target=libhotplug.so.1
link path=usr/lib/$(ARCH32)/libidmap.so target=./libidmap.so.1
link path=usr/lib/$(ARCH32)/libinetutil.so.1 \
    target=../../../lib/$(ARCH32)/libinetutil.so.1
link path=usr/lib/$(ARCH32)/libintl.so \
    target=../../../lib/$(ARCH32)/libintl.so.1
link path=usr/lib/$(ARCH32)/libintl.so.1 \
    target=../../../lib/$(ARCH32)/libintl.so.1
link path=usr/lib/$(ARCH32)/libipmi.so target=./libipmi.so.1
link path=usr/lib/$(ARCH32)/libipp.so target=libipp.so.1
link path=usr/lib/$(ARCH32)/libkstat.so \
    target=../../../lib/$(ARCH32)/libkstat.so.1
link path=usr/lib/$(ARCH32)/libkstat.so.1 \
    target=../../../lib/$(ARCH32)/libkstat.so.1
link path=usr/lib/$(ARCH32)/libkvm.so target=libkvm.so.1
link path=usr/lib/$(ARCH32)/libl.so target=libl.so.1
link path=usr/lib/$(ARCH32)/libldap.so target=libldap.so.5
link path=usr/lib/$(ARCH32)/liblddbg.so.4 \
    target=../../../lib/$(ARCH32)/liblddbg.so.4
link path=usr/lib/$(ARCH32)/liblgrp.so target=liblgrp.so.1
link path=usr/lib/$(ARCH32)/liblm.so target=liblm.so.1
link path=usr/lib/$(ARCH32)/libmail.so target=libmail.so.1
link path=usr/lib/$(ARCH32)/libmalloc.so target=libmalloc.so.1
link path=usr/lib/$(ARCH32)/libmapmalloc.so target=libmapmalloc.so.1
link path=usr/lib/$(ARCH32)/libmd.so target=../../../lib/$(ARCH32)/libmd.so.1
link path=usr/lib/$(ARCH32)/libmd.so.1 \
    target=../../../lib/$(ARCH32)/libmd.so.1
link path=usr/lib/$(ARCH32)/libmd5.so \
    target=../../../lib/$(ARCH32)/libmd5.so.1
link path=usr/lib/$(ARCH32)/libmd5.so.1 \
    target=../../../lib/$(ARCH32)/libmd5.so.1
link path=usr/lib/$(ARCH32)/libmp.so target=../../../lib/$(ARCH32)/libmp.so.2
link path=usr/lib/$(ARCH32)/libmp.so.2 \
    target=../../../lib/$(ARCH32)/libmp.so.2
link path=usr/lib/$(ARCH32)/libmtmalloc.so target=libmtmalloc.so.1
link path=usr/lib/$(ARCH32)/libnls.so target=libnls.so.1
link path=usr/lib/$(ARCH32)/libnsl.so \
    target=../../../lib/$(ARCH32)/libnsl.so.1
link path=usr/lib/$(ARCH32)/libnsl.so.1 \
    target=../../../lib/$(ARCH32)/libnsl.so.1
link path=usr/lib/$(ARCH32)/libnvpair.so \
    target=../../../lib/$(ARCH32)/libnvpair.so.1
link path=usr/lib/$(ARCH32)/libnvpair.so.1 \
    target=../../../lib/$(ARCH32)/libnvpair.so.1
link path=usr/lib/$(ARCH32)/libpam.so \
    target=../../../lib/$(ARCH32)/libpam.so.1
link path=usr/lib/$(ARCH32)/libpam.so.1 \
    target=../../../lib/$(ARCH32)/libpam.so.1
link path=usr/lib/$(ARCH32)/libpkcs11.so target=libpkcs11.so.1
link path=usr/lib/$(ARCH32)/libposix4.so \
    target=../../../lib/$(ARCH32)/librt.so.1
link path=usr/lib/$(ARCH32)/libposix4.so.1 \
    target=../../../lib/$(ARCH32)/librt.so.1
link path=usr/lib/$(ARCH32)/libproc.so \
    target=../../../lib/$(ARCH32)/libproc.so.1
link path=usr/lib/$(ARCH32)/libproc.so.1 \
    target=../../../lib/$(ARCH32)/libproc.so.1
link path=usr/lib/$(ARCH32)/libproject.so target=libproject.so.1
link path=usr/lib/$(ARCH32)/libpthread.so \
    target=../../../lib/$(ARCH32)/libpthread.so.1
link path=usr/lib/$(ARCH32)/libpthread.so.1 \
    target=../../../lib/$(ARCH32)/libpthread.so.1
link path=usr/lib/$(ARCH32)/librcm.so \
    target=../../../lib/$(ARCH32)/librcm.so.1
link path=usr/lib/$(ARCH32)/librcm.so.1 \
    target=../../../lib/$(ARCH32)/librcm.so.1
link path=usr/lib/$(ARCH32)/libreparse.so target=libreparse.so.1
link path=usr/lib/$(ARCH32)/libresolv.so \
    target=../../../lib/$(ARCH32)/libresolv.so.2
link path=usr/lib/$(ARCH32)/libresolv.so.2 \
    target=../../../lib/$(ARCH32)/libresolv.so.2
link path=usr/lib/$(ARCH32)/librestart.so \
    target=../../../lib/$(ARCH32)/librestart.so.1
link path=usr/lib/$(ARCH32)/librestart.so.1 \
    target=../../../lib/$(ARCH32)/librestart.so.1
link path=usr/lib/$(ARCH32)/librpcsvc.so \
    target=../../../lib/$(ARCH32)/librpcsvc.so.1
link path=usr/lib/$(ARCH32)/librpcsvc.so.1 \
    target=../../../lib/$(ARCH32)/librpcsvc.so.1
link path=usr/lib/$(ARCH32)/librt.so target=../../../lib/$(ARCH32)/librt.so.1
link path=usr/lib/$(ARCH32)/librt.so.1 \
    target=../../../lib/$(ARCH32)/librt.so.1
link path=usr/lib/$(ARCH32)/librtld.so.1 \
    target=../../../lib/$(ARCH32)/librtld.so.1
link path=usr/lib/$(ARCH32)/librtld_db.so \
    target=../../../lib/$(ARCH32)/librtld_db.so.1
link path=usr/lib/$(ARCH32)/librtld_db.so.1 \
    target=../../../lib/$(ARCH32)/librtld_db.so.1
link path=usr/lib/$(ARCH32)/libscf.so \
    target=../../../lib/$(ARCH32)/libscf.so.1
link path=usr/lib/$(ARCH32)/libscf.so.1 \
    target=../../../lib/$(ARCH32)/libscf.so.1
link path=usr/lib/$(ARCH32)/libsched.so target=libsched.so.1
link path=usr/lib/$(ARCH32)/libsctp.so target=./libsctp.so.1
link path=usr/lib/$(ARCH32)/libsec.so \
    target=../../../lib/$(ARCH32)/libsec.so.1
link path=usr/lib/$(ARCH32)/libsec.so.1 \
    target=../../../lib/$(ARCH32)/libsec.so.1
link path=usr/lib/$(ARCH32)/libsecdb.so \
    target=../../../lib/$(ARCH32)/libsecdb.so.1
link path=usr/lib/$(ARCH32)/libsecdb.so.1 \
    target=../../../lib/$(ARCH32)/libsecdb.so.1
link path=usr/lib/$(ARCH32)/libsendfile.so \
    target=../../../lib/$(ARCH32)/libsendfile.so.1
link path=usr/lib/$(ARCH32)/libsendfile.so.1 \
    target=../../../lib/$(ARCH32)/libsendfile.so.1
link path=usr/lib/$(ARCH32)/libsip.so target=./libsip.so.1
link path=usr/lib/$(ARCH32)/libsldap.so target=libsldap.so.1
link path=usr/lib/$(ARCH32)/libsmbios.so target=libsmbios.so.1
link path=usr/lib/$(ARCH32)/libsoftcrypto.so target=./libsoftcrypto.so.1
link path=usr/lib/$(ARCH32)/libsys.so target=libsys.so.1
link path=usr/lib/$(ARCH32)/libsysevent.so \
    target=../../../lib/$(ARCH32)/libsysevent.so.1
link path=usr/lib/$(ARCH32)/libsysevent.so.1 \
    target=../../../lib/$(ARCH32)/libsysevent.so.1
link path=usr/lib/$(ARCH32)/libthread.so \
    target=../../../lib/$(ARCH32)/libthread.so.1
link path=usr/lib/$(ARCH32)/libthread.so.1 \
    target=../../../lib/$(ARCH32)/libthread.so.1
link path=usr/lib/$(ARCH32)/libthread_db.so \
    target=../../../lib/$(ARCH32)/libc_db.so.1
link path=usr/lib/$(ARCH32)/libthread_db.so.1 \
    target=../../../lib/$(ARCH32)/libc_db.so.1
link path=usr/lib/$(ARCH32)/libumem.so \
    target=../../../lib/$(ARCH32)/libumem.so.1
link path=usr/lib/$(ARCH32)/libumem.so.1 \
    target=../../../lib/$(ARCH32)/libumem.so.1
link path=usr/lib/$(ARCH32)/libuuid.so \
    target=../../../lib/$(ARCH32)/libuuid.so.1
link path=usr/lib/$(ARCH32)/libuuid.so.1 \
    target=../../../lib/$(ARCH32)/libuuid.so.1
link path=usr/lib/$(ARCH32)/libuutil.so \
    target=libuutil.so.1
link path=usr/lib/$(ARCH32)/libvolmgt.so target=libvolmgt.so.1
link path=usr/lib/$(ARCH32)/libw.so target=../../../lib/$(ARCH32)/libw.so.1
link path=usr/lib/$(ARCH32)/libw.so.1 target=../../../lib/$(ARCH32)/libw.so.1
link path=usr/lib/$(ARCH32)/libwrap.so target=libwrap.so.1.0
link path=usr/lib/$(ARCH32)/libwrap.so.1 target=libwrap.so.1.0
link path=usr/lib/$(ARCH32)/nss_compat.so.1 \
    target=../../../lib/$(ARCH32)/nss_compat.so.1
link path=usr/lib/$(ARCH32)/nss_dns.so.1 \
    target=../../../lib/$(ARCH32)/nss_dns.so.1
link path=usr/lib/$(ARCH32)/nss_files.so.1 \
    target=../../../lib/$(ARCH32)/nss_files.so.1
link path=usr/lib/$(ARCH32)/nss_nis.so.1 \
    target=../../../lib/$(ARCH32)/nss_nis.so.1
link path=usr/lib/$(ARCH32)/nss_user.so.1 \
    target=../../../lib/$(ARCH32)/nss_user.so.1
link path=usr/lib/$(ARCH32)/straddr.so target=straddr.so.2
link path=usr/lib/32 target=$(ARCH32)
link path=usr/lib/64 target=$(ARCH64)
link path=usr/lib/cfgadm/$(ARCH32)/ib.so target=./ib.so.1
link path=usr/lib/cfgadm/$(ARCH32)/pci.so target=./pci.so.1
link path=usr/lib/cfgadm/$(ARCH32)/sata.so target=./sata.so.1
link path=usr/lib/cfgadm/$(ARCH32)/scsi.so target=./scsi.so.1
link path=usr/lib/cfgadm/$(ARCH32)/shp.so target=./shp.so.1
link path=usr/lib/cfgadm/$(ARCH32)/usb.so target=./usb.so.1
link path=usr/lib/cfgadm/ib.so target=./ib.so.1
link path=usr/lib/cfgadm/pci.so target=./pci.so.1
link path=usr/lib/cfgadm/sata.so target=./sata.so.1
link path=usr/lib/cfgadm/scsi.so target=./scsi.so.1
link path=usr/lib/cfgadm/shp.so target=./shp.so.1
link path=usr/lib/cfgadm/usb.so target=./usb.so.1
link path=usr/lib/libadm.so target=../../lib/libadm.so.1
link path=usr/lib/libadm.so.1 target=../../lib/libadm.so.1
link path=usr/lib/libads.so target=./libads.so.1
link path=usr/lib/libadutils.so target=./libadutils.so.1
link path=usr/lib/libaio.so target=../../lib/libaio.so.1
link path=usr/lib/libaio.so.1 target=../../lib/libaio.so.1
link path=usr/lib/libavl.so target=../../lib/libavl.so.1
link path=usr/lib/libavl.so.1 target=../../lib/libavl.so.1
link path=usr/lib/libbsdmalloc.so target=./libbsdmalloc.so.1
link path=usr/lib/libc.so target=../../lib/libc.so.1
link path=usr/lib/libc.so.1 target=../../lib/libc.so.1
link path=usr/lib/libc_db.so target=../../lib/libc_db.so.1
link path=usr/lib/libc_db.so.1 target=../../lib/libc_db.so.1
link path=usr/lib/libcfgadm.so target=./libcfgadm.so.1
link path=usr/lib/libcmdutils.so target=../../lib/libcmdutils.so.1
link path=usr/lib/libcmdutils.so.1 target=../../lib/libcmdutils.so.1
link path=usr/lib/libcommputil.so target=./libcommputil.so.1
link path=usr/lib/libcontract.so target=../../lib/libcontract.so.1
link path=usr/lib/libcontract.so.1 target=../../lib/libcontract.so.1
link path=usr/lib/libcrypt.so target=./libcrypt.so.1
link path=usr/lib/libcrypt_d.so target=./libcrypt.so
link path=usr/lib/libcrypt_d.so.1 target=./libcrypt.so.1
link path=usr/lib/libcrypt_i.so target=./libcrypt.so
link path=usr/lib/libcrypt_i.so.1 target=./libcrypt.so.1
link path=usr/lib/libctf.so target=../../lib/libctf.so.1
link path=usr/lib/libctf.so.1 target=../../lib/libctf.so.1
link path=usr/lib/libdemangle-sys.so target=./libdemangle-sys.so.1
link path=usr/lib/libdevice.so target=../../lib/libdevice.so.1
link path=usr/lib/libdevice.so.1 target=../../lib/libdevice.so.1
link path=usr/lib/libdevid.so target=../../lib/libdevid.so.1
link path=usr/lib/libdevid.so.1 target=../../lib/libdevid.so.1
link path=usr/lib/libdevinfo.so target=../../lib/libdevinfo.so.1
link path=usr/lib/libdevinfo.so.1 target=../../lib/libdevinfo.so.1
link path=usr/lib/libdhcputil.so.1 target=../../lib/libdhcputil.so.1
link path=usr/lib/libdisasm.so target=./libdisasm.so.1
link path=usr/lib/libdl.so target=../../lib/libdl.so.1
link path=usr/lib/libdl.so.1 target=../../lib/libdl.so.1
link path=usr/lib/libdlpi.so target=../../lib/libdlpi.so.1
link path=usr/lib/libdlpi.so.1 target=../../lib/libdlpi.so.1
link path=usr/lib/libdoor.so target=../../lib/libdoor.so.1
link path=usr/lib/libdoor.so.1 target=../../lib/libdoor.so.1
link path=usr/lib/libdwarf.so.1 target=../../lib/libdwarf.so.1
link path=usr/lib/libefi.so target=../../lib/libefi.so.1
link path=usr/lib/libefi.so.1 target=../../lib/libefi.so.1
link path=usr/lib/libelf.so target=../../lib/libelf.so.1
link path=usr/lib/libelf.so.1 target=../../lib/libelf.so.1
link path=usr/lib/libexacct.so target=./libexacct.so.1
link path=usr/lib/libfdisk.so target=../../lib/libfdisk.so.1
link path=usr/lib/libfdisk.so.1 target=../../lib/libfdisk.so.1
link path=usr/lib/libfstyp.so target=./libfstyp.so.1
link path=usr/lib/libgen.so target=../../lib/libgen.so.1
link path=usr/lib/libgen.so.1 target=../../lib/libgen.so.1
link path=usr/lib/libhotplug.so target=./libhotplug.so.1
link path=usr/lib/libidmap.so target=./libidmap.so.1
link path=usr/lib/libinetutil.so.1 target=../../lib/libinetutil.so.1
link path=usr/lib/libintl.so target=../../lib/libintl.so.1
link path=usr/lib/libintl.so.1 target=../../lib/libintl.so.1
link path=usr/lib/libipmi.so target=./libipmi.so.1
link path=usr/lib/libipp.so target=./libipp.so.1
link path=usr/lib/libkstat.so target=../../lib/libkstat.so.1
link path=usr/lib/libkstat.so.1 target=../../lib/libkstat.so.1
link path=usr/lib/libkvm.so target=./libkvm.so.1
link path=usr/lib/libl.so target=./libl.so.1
link path=usr/lib/libldap.so target=libldap.so.5
link path=usr/lib/liblddbg.so.4 target=../../lib/liblddbg.so.4
link path=usr/lib/liblgrp.so target=./liblgrp.so.1
link path=usr/lib/liblm.so target=./liblm.so.1
link path=usr/lib/libmail.so target=./libmail.so.1
link path=usr/lib/libmalloc.so target=./libmalloc.so.1
link path=usr/lib/libmapmalloc.so target=./libmapmalloc.so.1
link path=usr/lib/libmd.so target=../../lib/libmd.so.1
link path=usr/lib/libmd.so.1 target=../../lib/libmd.so.1
link path=usr/lib/libmd5.so target=../../lib/libmd5.so.1
link path=usr/lib/libmd5.so.1 target=../../lib/libmd5.so.1
link path=usr/lib/libmp.so target=../../lib/libmp.so.2
link path=usr/lib/libmp.so.2 target=../../lib/libmp.so.2
link path=usr/lib/libmtmalloc.so target=./libmtmalloc.so.1
link path=usr/lib/libnls.so target=./libnls.so.1
link path=usr/lib/libnsl.so target=../../lib/libnsl.so.1
link path=usr/lib/libnsl.so.1 target=../../lib/libnsl.so.1
link path=usr/lib/libnvpair.so target=../../lib/libnvpair.so.1
link path=usr/lib/libnvpair.so.1 target=../../lib/libnvpair.so.1
link path=usr/lib/libpam.so target=../../lib/libpam.so.1
link path=usr/lib/libpam.so.1 target=../../lib/libpam.so.1
link path=usr/lib/libpkcs11.so target=./libpkcs11.so.1
link path=usr/lib/libposix4.so target=../../lib/librt.so.1
link path=usr/lib/libposix4.so.1 target=../../lib/librt.so.1
link path=usr/lib/libproc.so target=../../lib/libproc.so.1
link path=usr/lib/libproc.so.1 target=../../lib/libproc.so.1
link path=usr/lib/libproject.so target=./libproject.so.1
link path=usr/lib/libpthread.so target=../../lib/libpthread.so.1
link path=usr/lib/libpthread.so.1 target=../../lib/libpthread.so.1
link path=usr/lib/librcm.so target=../../lib/librcm.so.1
link path=usr/lib/librcm.so.1 target=../../lib/librcm.so.1
link path=usr/lib/libreparse.so target=./libreparse.so.1
link path=usr/lib/libresolv.so target=../../lib/libresolv.so.2
link path=usr/lib/libresolv.so.2 target=../../lib/libresolv.so.2
link path=usr/lib/librestart.so.1 target=../../lib/librestart.so.1
link path=usr/lib/librpcsvc.so target=../../lib/librpcsvc.so.1
link path=usr/lib/librpcsvc.so.1 target=../../lib/librpcsvc.so.1
link path=usr/lib/librt.so target=../../lib/librt.so.1
link path=usr/lib/librt.so.1 target=../../lib/librt.so.1
link path=usr/lib/librtld.so.1 target=../../lib/librtld.so.1
link path=usr/lib/librtld_db.so target=../../lib/librtld_db.so.1
link path=usr/lib/librtld_db.so.1 target=../../lib/librtld_db.so.1
link path=usr/lib/libscf.so target=../../lib/libscf.so.1
link path=usr/lib/libscf.so.1 target=../../lib/libscf.so.1
link path=usr/lib/libsched.so target=./libsched.so.1
link path=usr/lib/libsctp.so target=./libsctp.so.1
link path=usr/lib/libsec.so target=../../lib/libsec.so.1
link path=usr/lib/libsec.so.1 target=../../lib/libsec.so.1
link path=usr/lib/libsecdb.so target=../../lib/libsecdb.so.1
link path=usr/lib/libsecdb.so.1 target=../../lib/libsecdb.so.1
link path=usr/lib/libsendfile.so target=../../lib/libsendfile.so.1
link path=usr/lib/libsendfile.so.1 target=../../lib/libsendfile.so.1
link path=usr/lib/libsip.so target=./libsip.so.1
link path=usr/lib/libsldap.so target=libsldap.so.1
link path=usr/lib/libsmbios.so target=libsmbios.so.1
link path=usr/lib/libsoftcrypto.so target=./libsoftcrypto.so.1
link path=usr/lib/libsysevent.so target=../../lib/libsysevent.so.1
link path=usr/lib/libsysevent.so.1 target=../../lib/libsysevent.so.1
link path=usr/lib/libthread.so target=../../lib/libthread.so.1
link path=usr/lib/libthread.so.1 target=../../lib/libthread.so.1
link path=usr/lib/libthread_db.so target=../../lib/libc_db.so.1
link path=usr/lib/libthread_db.so.1 target=../../lib/libc_db.so.1
link path=usr/lib/libumem.so target=../../lib/libumem.so.1
link path=usr/lib/libumem.so.1 target=../../lib/libumem.so.1
link path=usr/lib/libuuid.so target=../../lib/libuuid.so.1
link path=usr/lib/libuuid.so.1 target=../../lib/libuuid.so.1
link path=usr/lib/libuutil.so target=libuutil.so.1
link path=usr/lib/libvolmgt.so target=./libvolmgt.so.1
link path=usr/lib/libw.so target=../../lib/libw.so.1
link path=usr/lib/libw.so.1 target=../../lib/libw.so.1
link path=usr/lib/libwrap.so target=libwrap.so.1.0
link path=usr/lib/libwrap.so.1 target=libwrap.so.1.0
link path=usr/lib/lwp/$(ARCH32)/libthread.so.1 \
    target=../../$(ARCH32)/libthread.so.1
link path=usr/lib/lwp/$(ARCH32)/libthread_db.so.1 \
    target=../../$(ARCH32)/libthread_db.so.1
link path=usr/lib/lwp/32 target=$(ARCH32)
link path=usr/lib/lwp/64 target=$(ARCH64)
link path=usr/lib/lwp/libthread.so.1 target=../libthread.so.1
link path=usr/lib/lwp/libthread_db.so.1 target=../libthread_db.so.1
link path=usr/lib/nss_compat.so.1 target=../../lib/nss_compat.so.1
link path=usr/lib/nss_dns.so.1 target=../../lib/nss_dns.so.1
link path=usr/lib/nss_files.so.1 target=../../lib/nss_files.so.1
link path=usr/lib/nss_nis.so.1 target=../../lib/nss_nis.so.1
link path=usr/lib/nss_user.so.1 target=../../lib/nss_user.so.1
link path=usr/lib/scsi/$(ARCH32)/libscsi.so target=./libscsi.so.1
link path=usr/lib/scsi/$(ARCH32)/libses.so target=./libses.so.1
link path=usr/lib/scsi/$(ARCH32)/libsmp.so target=./libsmp.so.1
link path=usr/lib/scsi/libscsi.so target=./libscsi.so.1
link path=usr/lib/scsi/libses.so target=./libses.so.1
link path=usr/lib/scsi/libsmp.so target=./libsmp.so.1
link path=usr/lib/security/$(ARCH64)/crypt_bsdbf.so target=./crypt_bsdbf.so.1
link path=usr/lib/security/$(ARCH64)/crypt_bsdmd5.so \
    target=./crypt_bsdmd5.so.1
link path=usr/lib/security/$(ARCH64)/crypt_sha256.so \
    target=./crypt_sha256.so.1
link path=usr/lib/security/$(ARCH64)/crypt_sha512.so \
    target=./crypt_sha512.so.1
link path=usr/lib/security/$(ARCH64)/crypt_sunmd5.so \
    target=./crypt_sunmd5.so.1
link path=usr/lib/security/$(ARCH64)/pam_allow.so target=./pam_allow.so.1
link path=usr/lib/security/$(ARCH64)/pam_authtok_check.so \
    target=./pam_authtok_check.so.1
link path=usr/lib/security/$(ARCH64)/pam_authtok_get.so \
    target=./pam_authtok_get.so.1
link path=usr/lib/security/$(ARCH64)/pam_authtok_store.so \
    target=./pam_authtok_store.so.1
link path=usr/lib/security/$(ARCH64)/pam_deny.so target=./pam_deny.so.1
link path=usr/lib/security/$(ARCH64)/pam_dhkeys.so target=./pam_dhkeys.so.1
link path=usr/lib/security/$(ARCH64)/pam_dial_auth.so \
    target=./pam_dial_auth.so.1
link path=usr/lib/security/$(ARCH64)/pam_ldap.so target=./pam_ldap.so.1
link path=usr/lib/security/$(ARCH64)/pam_list.so target=./pam_list.so.1
link path=usr/lib/security/$(ARCH64)/pam_passwd_auth.so \
    target=./pam_passwd_auth.so.1
link path=usr/lib/security/$(ARCH64)/pam_roles.so target=./pam_roles.so.1
link path=usr/lib/security/$(ARCH64)/pam_sample.so target=./pam_sample.so.1
link path=usr/lib/security/$(ARCH64)/pam_timestamp.so \
    target=./pam_timestamp.so.1
link path=usr/lib/security/$(ARCH64)/pam_unix_account.so \
    target=./pam_unix_account.so.1
link path=usr/lib/security/$(ARCH64)/pam_unix_auth.so \
    target=./pam_unix_auth.so.1
link path=usr/lib/security/$(ARCH64)/pam_unix_cred.so \
    target=./pam_unix_cred.so.1
link path=usr/lib/security/$(ARCH64)/pam_unix_session.so \
    target=./pam_unix_session.so.1
link path=usr/lib/security/$(ARCH64)/pkcs11_kernel.so \
    target=./pkcs11_kernel.so.1
link path=usr/lib/security/$(ARCH64)/pkcs11_softtoken.so \
    target=./pkcs11_softtoken.so.1
link path=usr/lib/security/64 target=$(ARCH64)
link path=usr/lib/security/crypt_bsdbf.so target=./crypt_bsdbf.so.1
link path=usr/lib/security/crypt_bsdmd5.so target=./crypt_bsdmd5.so.1
link path=usr/lib/security/crypt_sha256.so target=./crypt_sha256.so.1
link path=usr/lib/security/crypt_sha512.so target=./crypt_sha512.so.1
link path=usr/lib/security/crypt_sunmd5.so target=./crypt_sunmd5.so.1
link path=usr/lib/security/pam_allow.so target=./pam_allow.so.1
link path=usr/lib/security/pam_authtok_check.so \
    target=./pam_authtok_check.so.1
link path=usr/lib/security/pam_authtok_get.so target=./pam_authtok_get.so.1
link path=usr/lib/security/pam_authtok_store.so \
    target=./pam_authtok_store.so.1
link path=usr/lib/security/pam_deny.so target=./pam_deny.so.1
link path=usr/lib/security/pam_dhkeys.so target=./pam_dhkeys.so.1
link path=usr/lib/security/pam_dial_auth.so target=./pam_dial_auth.so.1
link path=usr/lib/security/pam_ldap.so target=./pam_ldap.so.1
link path=usr/lib/security/pam_list.so target=./pam_list.so.1
link path=usr/lib/security/pam_passwd_auth.so target=./pam_passwd_auth.so.1
link path=usr/lib/security/pam_roles.so target=./pam_roles.so.1
link path=usr/lib/security/pam_sample.so target=./pam_sample.so.1
link path=usr/lib/security/pam_timestamp.so target=./pam_timestamp.so.1
link path=usr/lib/security/pam_unix_account.so target=./pam_unix_account.so.1
link path=usr/lib/security/pam_unix_auth.so target=./pam_unix_auth.so.1
link path=usr/lib/security/pam_unix_cred.so target=./pam_unix_cred.so.1
link path=usr/lib/security/pam_unix_session.so target=./pam_unix_session.so.1
link path=usr/lib/security/pkcs11_kernel.so target=./pkcs11_kernel.so.1
link path=usr/lib/security/pkcs11_softtoken.so target=./pkcs11_softtoken.so.1
link path=usr/lib/straddr.so target=./straddr.so.2

# CDDL HEADER START

# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source. A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.

# CDDL HEADER END

############################################################################
# Additional settings needed to build illumos-gate on OmniOS.
############################################################################

# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.

export OOCE_RELVER=`grep '^VERSION=r' /etc/os-release | cut -c10-15`

case $OOCE_RELVER in
	151030)
		PERL_VERSION=5.28
		TOOLS_PYTHON=/usr/bin/python3.5
		;;
	15103[1-2])
		PERL_VERSION=5.30
		TOOLS_PYTHON=/usr/bin/python3.5
		;;
	15103[3-9])
		PERL_VERSION=5.30
		TOOLS_PYTHON=/usr/bin/python3.7
		export PYTHON3_VERSION=3.7
		export PYTHON3_PKGVERS=-37
		;;
	*)
		echo "Unhandled OmniOS release, '$OOCE_RELVER'"
		exit 1
		;;
esac

export PERL_VERSION TOOLS_PYTHON

export BUILDPERL32=
export BUILDPERL64=
export PERL_PKGVERS=
export PERL_VARIANT=-thread-multi

export BUILDPY2=
export BUILDPY3=
export BUILDPY2TOOLS=
export BUILDPY3TOOLS=

export BLD_JAVA_7='#'
export BLD_JAVA_8=

export ON_CLOSED_BINS=/opt/onbld/closed

export ENABLE_SMB_PRINTING='#'

# On OmniOS, gcc resides in /opt/gcc-<version> - adjust variables
export GNUC_ROOT=/opt/gcc-7/
for name in PRIMARY_CC PRIMARY_CCC SHADOW_CCS SHADOW_CCCS; do
        typeset -n var=$name
        var="`echo $var | sed '
                s^/usr/gcc^/opt/gcc^g
                s^/opt/gcc/^/opt/gcc-^g
        '`"
done

export ONNV_BUILDNUM=$OOCE_RELVER
export PKGVERS_BRANCH=$ONNV_BUILDNUM.0


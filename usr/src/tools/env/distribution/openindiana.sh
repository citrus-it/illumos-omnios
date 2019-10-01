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
# Additional settings needed to build illumos-gate on OpenIndiana.
############################################################################

# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.

export BUILDPERL64='#'
export PERL_VERSION="5.22"
export PERL_PKGVERS="-522"

export BUILDPY2=
export BUILDPY3=
export BUILDPY2TOOLS='#'
export BUILDPY3TOOLS=

export BLD_JAVA_7='#'
export BLD_JAVA_8=

export ON_CLOSED_BINS=/opt/onbld/closed

export PKGVERS_BRANCH=9999.99.0.0


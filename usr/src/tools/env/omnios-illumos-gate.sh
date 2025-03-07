
######################################################################
# OmniOS-specific overrides

# Enable the non-DEBUG build
NIGHTLY_OPTIONS=${NIGHTLY_OPTIONS/F/}

export PERL_VERSION=5.40
export PERL_PKGVERS=
export PERL_VARIANT=-thread-multi
export BUILDPERL32='#'

export JAVA_ROOT=/usr/jdk/openjdk17.0
export JAVA_HOME=$JAVA_ROOT
# The options for Java 11 are also suitable for 17
export BLD_JAVA_11=

export BUILDPY2='#'
export BUILDPY3=
export BUILDPY2TOOLS='#'
export BUILDPY3TOOLS=
export PYTHON3_VERSION=3.13
export PYTHON3_PKGVERS=-313
export PYTHON3_SUFFIX=
export TOOLS_PYTHON=/usr/bin/python$PYTHON3_VERSION

export ON_CLOSED_BINS=/opt/onbld/closed

# On OmniOS, gcc resides in /opt/gcc-<version> - adjust variables
for name in GNUC_ROOT PRIMARY_CC PRIMARY_CCC SHADOW_CCS SHADOW_CCCS; do
        typeset -n var=$name
        var="`echo $var | sed '
                s^/usr/gcc^/opt/gcc^
                s^/opt/gcc/^/opt/gcc-^
        '`"
done

ENABLE_SMB_PRINTING='#'

export ONNV_BUILDNUM=`grep '^VERSION=r' /etc/os-release | cut -c10-15`
export PKGVERS_BRANCH=$ONNV_BUILDNUM.0


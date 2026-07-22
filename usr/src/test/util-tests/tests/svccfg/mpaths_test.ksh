#!/usr/bin/ksh
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2026 Oxide Computer Company
#

#
# Tests for the managed_paths manifest element: import into a scratch
# repository, verification of the resulting properties, export back to
# manifest form, round-tripping, and rejection of invalid input.
#

unalias -a
set -o pipefail
export LANG=C.UTF-8

SVCCFG=${SVCCFG:-"/usr/sbin/svccfg"}

mp_exit=0
mp_arg0=$(basename $0)
mp_dir=$(dirname $0)
mp_work="/tmp/$mp_arg0.$$"
mp_svc="test/mpaths"

function fatal
{
	typeset msg="$*"
	echo "TEST FAILED: $msg" >&2
	exit 1
}

function warn
{
	typeset msg="$*"
	echo "TEST FAILED: $msg" >&2
	mp_exit=1
}

function cleanup
{
	rm -rf $mp_work/
}

#
# Run svccfg against the scratch repository named by the first argument.
#
function repo_svccfg
{
	typeset repo="$1"
	shift
	SVCCFG_REPOSITORY="$mp_work/$repo.db" $SVCCFG "$@"
}

#
# Check that a property in the given repository has the expected type and
# value, as shown by svccfg listprop.
#
function check_prop
{
	typeset repo="$1" fmri="$2" prop="$3" ptype="$4" value="$5"
	typeset out

	out=$(repo_svccfg $repo -s $fmri listprop $prop) ||
	    fatal "failed to list $fmri $prop"
	out=$(echo "$out" | tr -s ' \t' ' ')
	if [[ "$out" != "$prop $ptype $value" ]]; then
		warn "$fmri $prop: expected \"$prop $ptype $value\"," \
		    "got \"$out\""
	else
		printf "TEST PASSED: %s %s = %s\n" "$fmri" "$prop" "$value"
	fi
}

#
# Check that a property is absent.
#
function check_absent
{
	typeset repo="$1" fmri="$2" prop="$3"
	typeset out

	out=$(repo_svccfg $repo -s $fmri listprop $prop) ||
	    fatal "failed to list $fmri $prop"
	if [[ -n "$out" ]]; then
		warn "$fmri $prop: expected no property, got \"$out\""
	else
		printf "TEST PASSED: %s %s is absent\n" "$fmri" "$prop"
	fi
}

#
# Import a manifest, provided on stdin, that must be rejected.
#
function check_bad_manifest
{
	typeset desc="$1"
	typeset mfst="$mp_work/bad.xml"

	cat > $mfst || fatal "failed to write $mfst"
	rm -f $mp_work/bad.db
	if SVCCFG_REPOSITORY=$mp_work/bad.db $SVCCFG import $mfst \
	    >/dev/null 2>&1; then
		warn "import of manifest with $desc unexpectedly succeeded"
	else
		printf "TEST PASSED: import rejected: %s\n" "$desc"
	fi
}

#
# Emit a manifest with the given directory element in place, for the
# negative tests.
#
function bad_manifest
{
	typeset dir="$1"

	cat <<-EOF
	<?xml version="1.0"?>
	<!DOCTYPE service_bundle SYSTEM
	    "/usr/share/lib/xml/dtd/service_bundle.dtd.1">
	<service_bundle type='manifest' name='mpathsbad'>
	<service name='test/mpathsbad' type='service' version='1'>
		<managed_paths>
			$dir
		</managed_paths>
		<exec_method type='method' name='start' exec=':true'
		    timeout_seconds='0' />
		<exec_method type='method' name='stop' exec=':true'
		    timeout_seconds='0' />
		<instance name='default' enabled='false' />
	</service>
	</service_bundle>
	EOF
}

trap cleanup EXIT
mkdir -p $mp_work || fatal "failed to create $mp_work"

#
# The manifest must validate against the installed DTD.
#
if $SVCCFG validate $mp_dir/mpaths.xml; then
	printf "TEST PASSED: manifest validates\n"
else
	warn "svccfg validate of mpaths.xml failed"
fi

#
# Import into a scratch repository and check the resulting properties.
# The service has two managed_paths blocks whose entries must be
# concatenated with continuous numbering, and the instance has its own
# independently numbered group.
#
repo_svccfg main import $mp_dir/mpaths.xml ||
    fatal "failed to import mpaths.xml"

check_prop main $mp_svc managed_paths/path_0 astring /var/run/mpaths
check_prop main $mp_svc managed_paths/empty_0 boolean true
check_prop main $mp_svc managed_paths/path_1 astring /var/run/mpaths/sub
check_prop main $mp_svc managed_paths/user_1 astring root
check_prop main $mp_svc managed_paths/group_1 astring root
check_prop main $mp_svc managed_paths/mode_1 astring 0700
check_prop main $mp_svc managed_paths/path_2 astring '/var/tmp/mpaths-%i'
check_prop main $mp_svc managed_paths/env_2 astring MPATHS_DIR
check_absent main $mp_svc managed_paths/path_3
check_absent main $mp_svc managed_paths/user_0
check_absent main $mp_svc managed_paths/empty_1

check_prop main $mp_svc:default managed_paths/path_0 astring \
    /var/tmp/mpaths-inst
check_prop main $mp_svc:default managed_paths/env_0 astring MPATHS_DIR
check_absent main $mp_svc:default managed_paths/path_1

#
# Re-import the same manifest and make sure that the entries are replaced
# rather than concatenated again.
#
repo_svccfg main import $mp_dir/mpaths.xml || fatal "failed to re-import"
check_absent main $mp_svc managed_paths/path_3

#
# Export and check the managed_paths blocks. The two service level blocks
# are merged into one on export.
#
repo_svccfg main export $mp_svc > $mp_work/export1.xml ||
    fatal "failed to export $mp_svc"

sed -n '/<managed_paths>/,/<\/managed_paths>/p' $mp_work/export1.xml |
    sed -e 's/^ *//' > $mp_work/got.blocks

cat > $mp_work/want.blocks <<EOF
<managed_paths>
<directory path='/var/run/mpaths' empty='true'/>
<directory path='/var/run/mpaths/sub' user='root' group='root' mode='0700'/>
<directory path='/var/tmp/mpaths-%i' env='MPATHS_DIR'/>
</managed_paths>
<managed_paths>
<directory path='/var/tmp/mpaths-inst' env='MPATHS_DIR'/>
</managed_paths>
EOF

if diff -u $mp_work/want.blocks $mp_work/got.blocks; then
	printf "TEST PASSED: exported managed_paths blocks\n"
else
	warn "exported managed_paths blocks did not match"
fi

#
# The exported manifest must round-trip: importing it into a fresh
# repository and exporting again must produce identical output.
#
repo_svccfg rt import $mp_work/export1.xml ||
    fatal "failed to import export1.xml"
repo_svccfg rt export $mp_svc > $mp_work/export2.xml ||
    fatal "failed to re-export"

if diff -u $mp_work/export1.xml $mp_work/export2.xml; then
	printf "TEST PASSED: export round-trip\n"
else
	warn "export round-trip did not match"
fi

#
# Invalid manifests must be rejected at import time.
#
bad_manifest "<directory path='/a' mode='998' />" |
    check_bad_manifest "a non-octal mode"
bad_manifest "<directory path='/a' mode='77777' />" |
    check_bad_manifest "an out of range mode"
bad_manifest "<directory path='/a' env='FOO=BAR' />" |
    check_bad_manifest "an environment variable containing ="
bad_manifest "<directory path='/a' env='SMF_FOO' />" |
    check_bad_manifest "an environment variable with the SMF_ prefix"
bad_manifest "<directory path='/a' empty='sometimes' />" |
    check_bad_manifest "an invalid empty value"
bad_manifest "<directory mode='0755' />" |
    check_bad_manifest "a missing path attribute"

if (( mp_exit == 0 )); then
	printf "All tests passed successfully\n"
fi

exit $mp_exit

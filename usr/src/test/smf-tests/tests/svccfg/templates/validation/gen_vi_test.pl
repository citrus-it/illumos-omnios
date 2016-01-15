#!/usr/perl5/bin/perl
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

package validate;

use strict;
use warnings;

use lib "$ENV{STF_SUITE}/tests/include";
use gen_xml_manifest;

use Getopt::Std;
use Class::Struct;
use File::Basename;

struct sw_t => { reverse qw{
	$ on
	$ off
	$ message
	@ pieces
}};

struct tests_t => { reverse qw{
	@ testlist
}};


sub conv_type() {
	my $x = shift;

	$x =~ s/_//g;

	$x =~ tr/a-z/A-Z/;

	return ($x);
}


my %options = ();
getopts("i:o:", \%options);

my $dir = dirname($0);

$options{i} = "${dir}/validation_combo_sets" unless defined $options{i};
$options{o} = "./v_sets" unless defined $options{o};

my $cursw = -1;
my $swcnt = 0;
my $outfile = $options{o};
my $infile = $options{i};
my @switches = ();

open(INFILE, "< $infile") || die "Unalbe to open $infile\n";
while (<INFILE>) {
	chop($_);
	my ($swnumber, $rest) = split(/:/, $_, 2);

	#
	# Skip blank lines and comments
	#
	if (!$swnumber || $swnumber =~ /^#/) {
		next;
	}

	if ($cursw != $swnumber) {
		$cursw = $swnumber;
		if (defined $validate::newsw) {
			push(@switches, $validate::newsw);
		}
		$validate::newsw = sw_t->new();
		$swcnt++;
	}

	(my $type, $rest) = split(/:/, $rest, 2);
	if ($type eq "valid") {
		$validate::newsw->on($rest);
		next;
	}

	if ($type eq "invalid") {
		(my $mess, $rest) = split(/:/, $rest, 2);
		$validate::newsw->off($rest);
		$validate::newsw->message($mess);
		next;
	}

	push(@{$validate::newsw->pieces}, "$type:$rest");
}

push(@switches, $validate::newsw);

#
# Now create the arrays of different possible switch combinations
# Storing a number and an o or f.  The number will be an offset into
# the @switches array and the o or f will indicate whether to use
# the on or off setting.
#
my @s;
my %t = ();
for (my $x = 0; $x < $swcnt; $x++) {
	$s[$x] = $x;
	$t{$s[$x]}[0] = "$s[$x]o";
	$t{$s[$x]}[1] = "$s[$x]f";
}

my @sw_sets = ();
for (my $x = 1; $x < (2**$swcnt); $x++) {
	my $st = tests_t->new();
	for (my $y = $swcnt; $y >= 0; $y--) {
		if (($x >> $y) & 1) {
			push(@{$st->testlist}, $s[$y]);
		}
	}

	push(@sw_sets, $st);
}

my @t_sets = ();
foreach my $set (@sw_sets) {
	my $sc = $#{$set->testlist} + 1;

	for (my $x = 0; $x < (2**$sc); $x++) {
		my $ts = tests_t->new();
		for (my $y = $sc; $y >= 0; $y--) {
			if (defined ${$set->testlist}[$y]) {
				push(@{$ts->testlist},
				    $t{${$set->testlist}[$y]}[($x >> $y) & 1]);
			}
		}

		push(@t_sets, $ts);
	}
}

my $cntr = 0;
open(TMPFILE, "> $outfile");
foreach $a (@t_sets) {
	my $offsw = 0;
	my $ems = "";
	my @lines = ();
	$cntr++;

	foreach $b (@{$a->testlist}) {
		if (!$b) {
			next;
		}

		my $myb = $b;
		my $onoff = chop($myb);
		my $myswtch = $switches[$myb];
		foreach my $c (@{$myswtch->pieces}) {
			my $lfnd = 0;
			foreach my $l (@lines) {
				if ("$c" eq "$l") {
					$lfnd = 1;
					last;
				}
			}

			if ($lfnd == 0) {
				printf(TMPFILE "$c\n");
				push(@lines, $c);
			}
		}

		my $d;
		if ($onoff eq "f") {
			my $newms;
			$offsw = 1;
			$d = $myswtch->off;
			if ($ems ne "") {
				$newms = $myswtch->message;
				$ems = "$ems:$newms";
			} else {
				$newms = $myswtch->message;
				$ems = "$newms";
			}
		} else {
			$d = $myswtch->on;
		}
		printf(TMPFILE "$d\n");
	}

	if ($offsw == 1) {
		printf(TMPFILE "negative:vitests_$cntr:$ems\n");
	} else {
		printf(TMPFILE "positive:vitests_$cntr\n");
	}

}

close(TMPFILE);

&gen_xml_manifest($outfile);

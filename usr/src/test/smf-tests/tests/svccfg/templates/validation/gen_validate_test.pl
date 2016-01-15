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

#
# Generate tests to validate each of the validation possibilities
# singularly.
#

package validate;

use strict;
use warnings;

use lib "$ENV{STF_SUITE}/tests/include";
use gen_xml_manifest;

use Getopt::Std;
use Class::Struct;
use File::Basename;

sub usage() {
	printf("Usage :\n");
	printf("\tgen_validate_tests.pl -i <infile> [-p outfile prefix]\n");
	printf("\n\t outfile prefix defaults to test\n");
}

sub gen_test_set() {
	my $ctp = "";

	foreach my $p (@validate::pieces) {
		$ctp = $ctp."$p\n";
	}

	my $subtest = 0;
	my $pnline = "";
	foreach my $t (@validate::tests) {
		printf(TFILE "$ctp");

		$subtest++;
		my ($t, $p) = split(/:/, $t, 2);
		if ($t eq "invalid") {
			(my $e, $p) = split(/:/, $p, 2);
			$pnline = "negative:test${validate::cur_test}." .
			    "${subtest}:${e}";
		} else {
			$pnline = "positive:test${validate::cur_test}." .
			    "${subtest}:";
		}

		printf(TFILE "$p\n");
		printf(TFILE "$pnline\n");
	}
}

#
# Get the command line arguments :
#	-i : input file
#	-p : output file prefix
#
my %options = ();
getopts("i:o:", \%options);

my $dir = dirname($0);

$options{i} = "${dir}/validation_sets" unless defined $options{i};
$options{o} = "validate_test" unless defined $options{o};

my $outfile = $options{o};
my $infile = $options{i};

$validate::cur_test = -1;
unlink($outfile);
open(TFILE, "> ${outfile}") || die "Unable to open $outfile\n";
open(INFILE, "< $infile") || die "Unable to open $infile\n";
while (<INFILE>) {
	chop($_);
	my ($test_number, $rest) = split(/:/, $_, 2);

	#
	# Skip blank lines and comments
	#
	if (!$test_number || $test_number =~ /^#/) {
		next;
	}

	if ($validate::cur_test != $test_number) {
		if (! defined(@validate::tests)) {
			@validate::tests = ();
			@validate::pieces = ();
			$validate::cur_test = $test_number;
		} else {
			&gen_test_set;

			@validate::tests = ();
			@validate::pieces = ();
			$validate::cur_test = $test_number;
		}
	} 

	(my $type, $rest) = split(/:/, $rest, 2);
	if ($type eq "valid" || $type eq "invalid") {
		push(@validate::tests, "$type:$rest");
	} else {
		push(@validate::pieces, "$type:$rest");
	}
}

#
# Generate the last test set...
#
&gen_test_set;

close(TFILE);
close(INFILE);

&gen_xml_manifest($outfile);

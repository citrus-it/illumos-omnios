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

use lib "$ENV{STF_SUITE}/tests/include";
use piece_types;
use create_piece;
use write_piece;

use Getopt::Std;
use Class::Struct;

struct value => { reverse qw{
	$ leftside
	$ rightside
	$ middle
}};

struct piece => { reverse qw{
	$ type
	$ ident
	$ write
	$ close
	@ values
	@ associations
}};

#
# Open and initialize the test xml file and then call the recursive
# write_piece() function to work through the piece tree.
#
sub generate_file() {
	my $ofile = shift;
	open(TESTXML, ">$ofile");

	#
	# Write out the basic headers to the file.
	#
	printf(TESTXML "<?xml version=\"1.0\"?>\n");
	printf(TESTXML "<!DOCTYPE service_bundle SYSTEM \"/usr/share/lib/xml/dtd/service_bundle.dtd.1\">\n");
	printf(TESTXML "\n\n");

	#
	# Now start at the top piece and write out the assocations
	# tree.
	#
	$tablvl = -1;
	&write_piece($pieces);

	close(TESTXML);
}

#
# Associate a piece with its parent piece.
#
sub associate_piece() {
	my @args = @_;
	my $argc = @args;

	my $asstype = shift;
	my $assname = shift;
	my $asspiece = shift;

	#
	# So go through and look for the associative type.
	# When a type that matches is found, then verify
	# that the ident matches.  If it matches then insert
	# the association into the correct place.
	#
	if ($argc == 3) {
		@pset = $pieces;
	} else {
		@pset = shift;
	}

	foreach $assp (@pset) {
		if ($assp->type == $asstype && $assp->ident eq $assname) {
			#
			# Set the assocation and return out.  We need to
			# insert the association in the correct position.
			#
			$c = 0;
			foreach $a (@{$assp->associations}) {
				if ($asspiece->type < $a->type) {
					splice(@{$assp->associations}, $c, 0, $asspiece);
					return (1);
				}

				$c++;
			}
			push(@{$assp->associations}, $asspiece);
		} else {
			foreach $a (@{$assp->associations}) {
				$x = &associate_piece($asstype, $assname, $p, $a);
				if ($x == 1) {
					return (1);
				}
			}
		}
	}
}

#
# Initialize the pieces array for the manifest.
#
sub initialize_pieces() {

	&SERVICEBUNDLE("type=manifest,name=validate");
	&SERVICE("$SERVICEBUNDLE:validate", "name=$def_service_name,type=service,version=1");
	&EXECMETHOD("$SERVICE:$def_service_name", "type=method,name=start,exec=:true,timeout_seconds=60");
	&EXECMETHOD("$SERVICE:$def_service_name", "type=method,name=stop,exec=:true,timeout_seconds=60");
}

#
# Generate an xml manifest and error, then start add the test
# assertion to the list of tests.
#
sub generate_test() {
	my $test_type = shift;
	my $rest = shift;
	my ($filename, $errors) = split(/:/, $rest, 2);

	$ofile = "${testdir}/$filename";
	&generate_file($ofile);

	if ($errors) {
		$errfile = "${ofile}.errs";
		open (ERRFILE, "> ${errfile}");
		while ($errors) {
			(my $err, $errors) = split(/:/, $errors, 2);
			printf(ERRFILE "%s\n", $err);
		}
	} else {
		$errfile = "";
	}

	system("stf_addassert -u root -t '$filename' -c 'runtest $ofile $errfile'");
}

#
# Convert a type into a common usuable value, by removing underscores
# and setting values to caps
#
sub convert_type() {
	my $x = shift;

	$x =~ s/_//g;

	$x =~ tr/a-z/A-Z/;

	return ($x);
}

#
# Process a line of the data_set file
#
sub process_line() {
	my $type = shift;
	my $rest = shift;

	my $type = &convert_type($type);
	my ($asstype, $assname, $values) = split(/:/, $rest, 3);
	my $asstype = &convert_type($asstype);
	my $rtype = ${$type};

	if ($asstype eq "SERVICE" && $assname == "DEFAULT") {
		$assname = $def_service_name;
	}

	#
	# Will want to extend this to deal with a service name
	# that is not the default.
	#
	if ($asstype eq "TEMPLATE" && $template_piece == 0) {
		&TEMPLATE("$SERVICE:$def_service_name", "");
	}

	if (defined &{$type}) {
		&{$type}("${$asstype}:$assname", $values);
	} else {
		if ($type) {
			printf("Unknown type $type\n");
		}
	}
}

#
# Generate the xml manifests to be used during testing
#
sub gen_xml_manifest() {
	my $infile = shift;

	our $pieces = "uninitialized";
	our $def_service_name="system/template_validate";
	&initialize_pieces();

	our $testdir = "/var/tmp/validation_test_manifests";
	if ( ! -d $testdir) {
		mkdir($testdir, 0777);
	}

	open(INFILE, "< $infile") || die "Unable to open $infile\n";
	while (<INFILE>) {
		chop($_);
		my ($type, $rest) = split(/:/, $_, 2);

		if ($type eq "positive" || $type eq "negative") {
			&generate_test($type, $rest);

			$pieces = "uninitialized";
			$def_service_name="system/template_validate";
			$template_piece = 0;
			&initialize_pieces();
		} else {
			&process_line($type, $rest);
		}
	}
}
return (1);

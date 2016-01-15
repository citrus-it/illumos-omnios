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
# A number of functions to create each of the different pieces
# that will make up a manifest.
#

#
# Create a service_bundle piece
#
sub SERVICEBUNDLE() {
	my $values = shift;

	if ($pieces eq "uninitialized") {
		$pieces = piece->new();
	}

	$p = $pieces;

	$p->type($SERVICEBUNDLE);
	$p->write(\&write_service_bundle);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}
}

#
# Create a service piece
#
sub SERVICE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($SERVICE);
	$p->write(\&write_service);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}
	
#
# Create a exec_method piece
#
sub EXECMETHOD() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($EXECMETHOD);
	$p->write(\&write_exec_method);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}



#
# Create a property group piece
#
sub PROPERTYGROUP() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($PROPERTYGROUP);
	$p->write(\&write_property_group);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a createdefaultinstance piece
#
sub CREATEDEFAULTINSTANCE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($CREATEDEFAULTINSTANCE);
	$p->write(\&write_create_default_instance);
	@{$p->associations};

	my $setident = 0;
	if ($values ne "true" && $values ne "false") {
		return();
	}

	$v = value->new();
	$v->leftside($values);
	push(@{$p->values}, $v);

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a single_instance piece
#
sub SINGLEINSTANCE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($SINGLEINSTANCE);
	$p->write(\&write_single_instance);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a restarter piece
#
sub RESTARTER() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($RESTARTER);
	$p->write(\&write_restarter);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a dependency piece
#
sub DEPENDENCY() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($DEPENDENCY);
	$p->write(\&write_dependency);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a dependent piece
#
sub DEPENDENT() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($DEPENDENT);
	$p->write(\&write_dependent);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a method_context piece
#
sub METHODCONTEXT() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($METHODCONTEXT);
	$p->write(\&write_method_context);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a instance piece
#
sub INSTANCE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($INSTANCE);
	$p->write(\&write_instance);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a stability piece
#
sub STABILITY() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($STABILITY);
	$p->write(\&write_stability);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a template piece
#
sub TEMPLATE() {
	my $association = shift;
	my $values = shift;

	$template_piece = 1;
	$p = piece->new();
	$p->type($TEMPLATE);
	$p->write(\&write_template);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	if ($setident != 1) {
		$p->ident("DEFAULT");
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a common_name piece
#
sub COMMONNAME() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($COMMONNAME);
	$p->write(\&write_common_name);
	@{$p->associations};

	my $setident = 0;
	while ($values) {
		($lang, $cname, $values) = split(/,/, $values, 3);
		$v = value->new();
		$v->leftside("lang");
		$v->rightside($lang);

		push(@{$p->values}, $v);

		$v = value->new();
		$v->leftside("${lang}_cname");
		$v->rightside($cname);

		push(@{$p->values}, $v);
	}

	$p->ident("$cname");

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a description piece
#
sub DESCRIPTION() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($DESCRIPTION);
	$p->write(\&write_description);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a documentation piece
#
sub DOCUMENTATION() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($DOCUMENTATION);
	$p->write(\&write_documentation);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a pg_pattern piece
#
sub PGPATTERN() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($PGPATTERN);
	$p->write(\&write_pg_pattern);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a prop_pattern piece
#
sub PROPPATTERN() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($PROPPATTERN);
	$p->write(\&write_prop_pattern);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a units piece
#
sub UNITS() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($UNITS);
	$p->write(\&write_units);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a visibility piece
#
sub VISIBILITY() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($VISIBILITY);
	$p->write(\&write_visibility);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a cardinality piece
#
sub CARDINALITY() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($CARDINALITY);
	$p->write(\&write_cardinality);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a internal_separator piece
#
sub INTERNALSEPARATORS() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($INTERNALSEPARATORS);
	$p->write(\&write_internal_separators);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a values piece
#
sub VALUES() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($VALUES);
	$p->write(\&write_values);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "ident") {
			$setident = 1;
			$p->ident($r);
		}
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a constraints piece
#
sub CONSTRAINTS() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($CONSTRAINTS);
	$p->write(\&write_constraints);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "ident") {
			$setident = 1;
			$p->ident($r);
		}
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a value piece
#
sub VALUE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($VALUE);
	$p->write(\&write_value);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a range piece
#
sub RANGE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($RANGE);
	$p->write(\&write_range);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a value_set piece
#
sub VALUESET() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($VALUESET);
	$p->write(\&write_valueset);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a all_values piece
#
sub ALLVALUES() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($ALLVALUES);
	$p->write(\&write_allvalues);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a doclink piece
#
sub DOCLINK() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($DOCLINK);
	$p->write(\&write_doclink);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a manpage piece
#
sub MANPAGE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($MANPAGE);
	$p->write(\&write_manpage);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a service_fmri piece
#
sub SERVICEFMRI() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($SERVICEFMRI);
	$p->write(\&write_service_fmri);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a method_profile piece
#
sub METHODPROFILE() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($METHODPROFILE);
	$p->write(\&write_method_profile);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a method_credential piece
#
sub METHODCREDENTIAL() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($METHODCREDENTIAL);
	$p->write(\&write_method_credential);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a method_environment piece
#
sub METHODENVIRONMENT() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($METHODENVIRONMENT);
	$p->write(\&write_method_environment);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a prop_val piece
#
sub PROPVAL() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($PROPVAL);
	$p->write(\&write_propval);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a property piece
#
sub PROPERTY() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($PROPERTY);
	$p->write(\&write_property);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		($l, $r) = split(/=/, $val);

		if ($l eq "name") {
			$setident = 1;
			$p->ident($r);
		}

		$v = value->new();
		$v->leftside($l);
		$v->rightside($r);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a count_list piece
#
sub COUNTLIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($COUNTLIST);
	$p->write(\&write_count_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a interger_list piece
#
sub INTEGERLIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($INTEGERLIST);
	$p->write(\&write_integer_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a opaque_list piece
#
sub OPAQUELIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($OPAQUELIST);
	$p->write(\&write_opaque_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a host_list piece
#
sub HOSTLIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($HOSTLIST);
	$p->write(\&write_host_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a hostname_list piece
#
sub HOSTNAMELIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($HOSTNAMELIST);
	$p->write(\&write_host_name_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a netaddress4_list piece
#
sub NETADDRESSV4LIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($NETADDRESSV4LIST);
	$p->write(\&write_net_address_v4_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a netaddressv6_list piece
#
sub NETADDRESSV6LIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($NETADDRESSV6LIST);
	$p->write(\&write_net_address_v6_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a time_list piece
#
sub TIMELIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($TIMELIST);
	$p->write(\&write_time_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a astring_list piece
#
sub ASTRINGLIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($ASTRINGLIST);
	$p->write(\&write_astring_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a ustring_list piece
#
sub USTRINGLIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($USTRINGLIST);
	$p->write(\&write_ustring_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a boolean_list piece
#
sub BOOLEANLIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($BOOLEANLIST);
	$p->write(\&write_boolean_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a fmri_list piece
#
sub FMRILIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($FMRILIST);
	$p->write(\&write_fmri_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}

#
# Create a uri_list piece
#
sub URILIST() {
	my $association = shift;
	my $values = shift;

	$p = piece->new();
	$p->type($URILIST);
	$p->write(\&write_uri_list);
	@{$p->associations};

	my $setident = 0;
	@vals = split(/,/, $values);
	foreach $val (@vals) {
		$v = value->new();
		$v->rightside($val);

		push(@{$p->values}, $v);
	}

	($asstype, $assname) = split(/:/, $association);
	&associate_piece($asstype, $assname, $p);
}
return (1);

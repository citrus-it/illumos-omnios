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
# Write the piece out to the xml file
#
# This recursively works through the piece tree building out the
# xml file from the start to the end.
#
sub write_piece() {
	my $p = shift;

	$p->write->($p);

	$tablvl++;
	foreach $ap (@{$p->associations}) {
		&write_piece($ap);
	}
	if (length($p->close) > 0) {
		printf(TESTXML "%s\n", $p->close);
	}

	$tablvl--;
}


#
# Write a service bundle piece
#
sub write_service_bundle() {
	my $p = shift;

	printf(TESTXML "<service_bundle ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</service_bundle>");
}

#
# Write a service piece
#
sub write_service() {
	my $p = shift;

	printf(TESTXML "<service ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</service>");
}

#
# Write an exec_method piece
#
sub write_exec_method() {
	my $p = shift;

	printf(TESTXML "<exec_method ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a default_instance piece
#
sub write_create_default_instance() {
	my $p = shift;

	$v = ${$p->values}[0];
	printf(TESTXML "<create_default_instance ");
	printf(TESTXML "enabled='%s' />\n", $v->leftside);
	$p->close("");
}

#
# Write a single instance piece
#
sub write_single_instance() {
	my $p = shift;

	printf(TESTXML "<single_instance/>\n");
	$p->close("");
}

#
# Write a restarter piece
#
sub write_restarter() {
	my $p = shift;

	printf(TESTXML "<restarter>\n");
	$p->close("</restarter>");

}

#
# Write a dependency piece
#
sub write_dependency() {
	my $p = shift;

	printf(TESTXML "<dependency ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</dependency>");
}

#
# Write a dependent piece
#
sub write_dependent() {
	my $p = shift;

	printf(TESTXML "<dependent ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</dependent>");
}

#
# Write a method piece
#
sub write_method_context() {
	my $p = shift;

	printf(TESTXML "<method_context>\n");
	$p->close("</method_context>\n");
}

#
# Write a property group piece
#
sub write_property_group() {
	my $p = shift;

	printf(TESTXML "<property_group ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</property_group>");
}

#
# Write a instance piece
#
sub write_instance() {
	my $p = shift;

	printf(TESTXML "<instance ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</instance>");
}

#
# Write a stability piece
#
sub write_stability() {
	my $p = shift;

	printf(TESTXML "<instance ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a template piece
#
sub write_template() {
	my $p = shift;

	printf(TESTXML "<template>\n");
	$p->close("</template>");
}

#
# Write a common_name piece
#
sub write_common_name() {
	my $p = shift;

	printf(TESTXML "<common_name>\n");
	foreach $v (@{$p->values}) {
		if ($v->leftside eq "lang") {
			my $lang = $v->rightside;
			my $cname = "";
			foreach $vn (@{$p->values}) {
				if ($vn->leftside eq "${lang}_cname") {
					$cname = $vn->rightside;
					last;
				}
			}

			if ($cname) {
				printf(TESTXML "\t<loctext xml:lang='$lang'>\n");
				printf(TESTXML "\t\t$cname\n");
				printf(TESTXML "\t</loctext>\n");
			}
		}
	}

	printf(TESTXML "</common_name>\n");
}

#
# Write a description piece
#
sub write_description() {
	my $p = shift;

	printf(TESTXML "<description>\n");
	foreach $v (@{$p->values}) {
		if ($v->leftside eq "lang") {
			my $lang = $v->rightside;
			my $cname = "";
			foreach $vn (@{$p->values}) {
				if ($vn->leftside eq "${lang}_cname") {
					$cname = $vn->rightside;
					last;
				}
			}

			if ($cname) {
				printf(TESTXML "\t<loctext xml:lang='$lang'>\n");
				printf(TESTXML "\t\t$cname\n");
				printf(TESTXML "\t</loctext>\n");
			}
		}
	}

	printf(TESTXML "</description>\n");
}

#
# Write a documentation piece
#
sub write_documentation() {
	my $p = shift;

	printf(TESTXML "<documentation>\n");
	$p->close("</documentation>");
}

#
# Write a pg_pattern piece
#
sub write_pg_pattern() {
	my $p = shift;

	printf(TESTXML "<pg_pattern ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</pg_pattern>");
}

#
# Write a prop_pattern piece
#
sub write_prop_pattern() {
	my $p = shift;

	printf(TESTXML "<prop_pattern ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</prop_pattern>");
}

#
# Write a units piece
#
sub write_units() {
	my $p = shift;

	printf(TESTXML "<units>\n");
	foreach $v (@{$p->values}) {
		if ($v->leftside eq "lang") {
			my $lang = $v->rightside;
			my $cname = "";
			foreach $vn (@{$p->values}) {
				if ($vn->leftside eq "${lang}_cname") {
					$cname = $vn->rightside;
					last;
				}
			}

			if ($cname) {
				printf(TESTXML "\t<loctext xml:lang='$lang'>\n");
				printf(TESTXML "\t\t$cname\n");
				printf(TESTXML "\t</loctext>\n");
			}
		}
	}

	printf(TESTXML "</units>\n");
}

#
# Write a visibility piece
#
sub write_visibility() {
	my $p = shift;

	printf(TESTXML "<prop_pattern ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a cardinality piece
#
sub write_cardinality() {
	my $p = shift;

	printf(TESTXML "<cardinality ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write an internal separators piece
#
sub write_internal_separators() {
	printf("write_internal_separators Not yet implemented\n");
}

#
# Write a values piece
#
sub write_values() {
	my $p = shift;

	printf(TESTXML "<values>");
	$p->close("</values>");
}

#
# Write a constraints piece
#
sub write_constraints() {
	my $p = shift;

	printf(TESTXML "<constraints>\n");
	$p->close("</constraints>");
}

#
# Write a choices piece
#
sub write_choices() {
	my $p = shift;

	printf(TESTXML "<choices>");
	$p->close("</choices>");
}

#
# Write a value piece
#
sub write_value() {
	my $p = shift;

	printf(TESTXML "<value ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</value>");
}

#
# Write a range piece
#
sub write_range() {
	my $p = shift;

	printf(TESTXML "<range ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a valueset piece
#
sub write_valueset() {
	my $p = shift;

	printf(TESTXML "<valueset ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write an allvalues piece
#
sub write_allvalues() {
	my $p = shift;

	printf(TESTXML "<allvalues ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a doc_link piece
#
sub write_doc_link() {
	my $p = shift;

	printf(TESTXML "<doc_link");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\n");
		printf(TESTXML "\t%s='%s'", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a manpage piece
#
sub write_manpage() {
	my $p = shift;

	printf(TESTXML "<manpage ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a service fmri piece
#
sub write_service_fmri() {
	my $p = shift;

	printf(TESTXML "<service_fmri ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a method_propfile piece
#
sub write_method_profile() {
	printf("write_method_credential Not yet implemented\n");
}

#
# Write a method credential piece
#
sub write_method_credential() {
	my $p = shift;

	printf(TESTXML "<method_credential ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a method environment piece
#
sub write_method_environment() {
	printf("write_mathod_environment Not yet implemented\n");
}

#
# Write a propval piece
#
sub write_propval() {
	my $p = shift;

	printf(TESTXML "<propval ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML "/>\n");
	$p->close("");
}

#
# Write a property piece
#
sub write_property() {
	my $p = shift;

	printf(TESTXML "<property ");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "%s='%s' ", $v->leftside, $v->rightside);
	}

	printf(TESTXML ">\n");
	$p->close("</property>");
}

#
# Write a count list piece
#
sub write_count_list() {
	my $p = shift;

	printf(TESTXML "<count_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</count_list>\n");
}

#
# Write a integer list piece
#
sub write_integer_list() {
	my $p = shift;

	printf(TESTXML "<integer_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</integer_list>\n");
}

#
# Write an opaque list piece
#
sub write_opaque_list() {
	my $p = shift;

	printf(TESTXML "<opaque_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</opaque_list>\n");
}

#
# Write a host list piece
#
sub write_host_list() {
	my $p = shift;

	printf(TESTXML "<host_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</host_list>\n");
}

#
# Write a hostname list piece
#
sub write_host_name_list() {
	my $p = shift;

	printf(TESTXML "<host_name_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</host_name_list>\n");
}

#
# Write a netaddress v4 list piece
#
sub write_net_address_v4_list() {
	my $p = shift;

	printf(TESTXML "<net_address_v4_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</net_address_v4_list>\n");
}

#
# Write a netaddress v6 list piece
#
sub write_net_address_v6_list() {
	my $p = shift;

	printf(TESTXML "<net_address_v6_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</net_address_v6_list>\n");
}

#
# Write a time list piece
#
sub write_time_list() {
	my $p = shift;

	printf(TESTXML "<time_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</time_list>\n");
}

#
# Write a astring list piece
#
sub write_astring_list() {
	my $p = shift;

	printf(TESTXML "<astring_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</astring_list>\n");
	$p->close("");
}

#
# Write a ustring list piece
#
sub write_ustring_list() {
	my $p = shift;

	printf(TESTXML "<ustring_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</ustring_list>\n");
}

#
# Write a boolean list piece
#
sub write_boolean_list() {
	my $p = shift;

	printf(TESTXML "<boolean_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</boolean_list>\n");
}

#
# Write a fmri list piece
#
sub write_fmri_list() {
	my $p = shift;

	printf(TESTXML "<fmri_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</fmri_list>\n");
}

#
# Write a uri list piece
#
sub write_uri_list() {
	my $p = shift;

	printf(TESTXML "<uri_list>\n");
	foreach my $v (@{$p->values}) {
		printf(TESTXML "\t<value_node value='%s' />\n", $v->rightside);
	}

	printf(TESTXML "</uri_list>\n");
}
return (1);

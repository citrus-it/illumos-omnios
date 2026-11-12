/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 Oxide Computer Company
 */

#include <sys/ccompile.h>

#include "fmevt.h"

/*
 * Post-processing according to the FMEV_RULESET_OXIDE_IREPORT ruleset.
 * Produce a single event of class ireport.oxide.<raw-class>.<raw-subclass>.
 * If either the raw class or subclass is NULL, drop the event.
 * The event will have the raw attributes.
 */
uint_t
fmevt_pp_oxide_ireport(char *classes[FMEVT_FANOUT_MAX],
    nvlist_t *attr[FMEVT_FANOUT_MAX], const char *ruleset __unused,
    const nvlist_t *detector __unused, nvlist_t *rawattr,
    const struct fmevt_ppargs *eap)
{
	if (eap->pp_rawclass == NULL || eap->pp_rawsubclass == NULL)
		return (0);

	if (snprintf(classes[0], FMEVT_MAX_CLASS, "%s.%s.%s.%s",
	    FM_IREPORT_CLASS, "oxide",
	    eap->pp_rawclass, eap->pp_rawsubclass) >= FMEVT_MAX_CLASS - 1) {
		return (0);
	}

	attr[0] = rawattr;
	return (1);
}

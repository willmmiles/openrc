/*
   rc-update
   Manage init scripts and runlevels
   */

/*
 * Copyright 2007 Roy Marples
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "einfo.h"
#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

static const char *applet = NULL;

/* Return the number of changes made:
 *  -1 = no changes (error)
 *   0 = no changes (nothing to do)
 *  1+ = number of runlevels updated
 */
static int add (const char *runlevel, const char *service)
{
	int retval = -1;

	if (! rc_service_exists (service))
		eerror ("%s: service `%s' does not exist", applet, service);
	else if (rc_service_in_runlevel (service, runlevel)) {
		ewarn ("%s: %s already installed in runlevel `%s'; skipping",
		       applet, service, runlevel);
		retval = 0;
	} else if (rc_service_add (runlevel, service)) {
		einfo ("%s added to runlevel %s", service, runlevel);
		retval = 1;
	} else
		eerror ("%s: failed to add service `%s' to runlevel `%s': %s",
			applet, service, runlevel, strerror (errno));

	return (retval);
}

static int delete (const char *runlevel, const char *service)
{
	int retval = -1;

	errno = 0;
	if (rc_service_delete (runlevel, service)) {
		einfo ("%s removed from runlevel %s", service, runlevel);
		return 1;
	}

	if (errno == ENOENT)
		eerror ("%s: service `%s' is not in the runlevel `%s'",
			applet, service, runlevel);
	else
		eerror ("%s: failed to remove service `%s' from runlevel `%s': %s",
			applet, service, runlevel, strerror (errno));

	return (retval);
}

static void show (char **runlevels, bool verbose)
{
	char *service;
	char **services = rc_services_in_runlevel (NULL);
	char *runlevel;
	int i;
	int j;

	STRLIST_FOREACH (services, service, i) {
		char **in = NULL;
		bool inone = false;

		STRLIST_FOREACH (runlevels, runlevel, j) {
			if (rc_service_in_runlevel (service, runlevel)) {
				rc_strlist_add (&in, runlevel);
				inone = true;
			} else {
				char buffer[PATH_MAX];
				memset (buffer, ' ', strlen (runlevel));
				buffer[strlen (runlevel)] = 0;
				rc_strlist_add (&in, buffer);
			}
		}

		if (! inone && ! verbose)
			continue;

		printf (" %20s |", service);
		STRLIST_FOREACH (in, runlevel, j)
			printf (" %s", runlevel);
		printf ("\n");
		rc_strlist_free (in);
	}

	rc_strlist_free (services);
}

#include "_usage.h"
#define getoptstring "ads" getoptstring_COMMON
static struct option longopts[] = {
	{ "add",      0, NULL, 'a'},
	{ "delete",   0, NULL, 'd'},
	{ "show",     0, NULL, 's'},
	longopts_COMMON
};
static const char * const longopts_help[] = {
	"Add the init.d to runlevels",
	"Delete init.d from runlevels",
	"Show init.d's in runlevels",
	longopts_help_COMMON
};
#include "_usage.c"

#define DOADD    (1 << 1)
#define DODELETE (1 << 2)
#define DOSHOW   (1 << 3)

int rc_update (int argc, char **argv)
{
	int i;
	char *service = NULL;
	char **runlevels = NULL;
	char *runlevel;
	int action = 0;
	bool verbose = false;
	int opt;
	int retval = EXIT_FAILURE;

	applet = basename_c (argv[0]);

	while ((opt = getopt_long (argc, argv, getoptstring,
				   longopts, (int *) 0)) != -1)
	{
		switch (opt) {
			case 'a':
				action |= DOADD;
				break;
			case 'd':
				action |= DODELETE;
				break;
			case 's':
				action |= DOSHOW;
				break;

				case_RC_COMMON_GETOPT
		}
	}

	verbose = rc_yesno (getenv ("EINFO_VERBOSE"));

	if ((action & DOSHOW   && action != DOSHOW) ||
	    (action & DOADD    && action != DOADD) ||
	    (action & DODELETE && action != DODELETE))
		eerrorx ("%s: cannot mix commands", applet);

	/* We need to be backwards compatible */
	if (! action) {
		if (optind < argc) {
			if (strcmp (argv[optind], "add") == 0)
				action = DOADD;
			else if (strcmp (argv[optind], "delete") == 0 ||
				 strcmp (argv[optind], "del") == 0)
				action = DODELETE;
			else if (strcmp (argv[optind], "show") == 0)
				action = DOSHOW;
			if (action)
				optind++;
			else
				eerrorx ("%s: invalid command `%s'", applet, argv[optind]);
		}
		if (! action)
			usage (EXIT_FAILURE);
	}

	if (optind >= argc) {
		if (! action & DOSHOW)
			eerrorx ("%s: no service specified", applet);
	} else {
		service = argv[optind];
		optind++;

		while (optind < argc)
			if (rc_runlevel_exists (argv[optind]))
				rc_strlist_add (&runlevels, argv[optind++]);
			else {
				rc_strlist_free (runlevels);
				eerrorx ("%s: `%s' is not a valid runlevel", applet, argv[optind]);
			}
	}

	retval = EXIT_SUCCESS;
	if (action & DOSHOW) {
		if (service)
			rc_strlist_add (&runlevels, service);
		if (! runlevels)
			runlevels = rc_runlevel_list ();

		show (runlevels, verbose);
	} else {
		if (! service)
			eerror ("%s: no service specified", applet);
		else {
			int num_updated = 0;
			int (*actfunc)(const char *, const char *);
			int ret;

			if (action & DOADD) {
				actfunc = add;
			} else if (action & DODELETE) {
				actfunc = delete;
			} else
				eerrorx ("%s: invalid action", applet);

			if (! runlevels)
				rc_strlist_add (&runlevels, rc_runlevel_get ());

			if (! runlevels)
				eerrorx ("%s: no runlevels found", applet);

			STRLIST_FOREACH (runlevels, runlevel, i) {
				if (! rc_runlevel_exists (runlevel)) {
					eerror ("%s: runlevel `%s' does not exist", applet, runlevel);
					continue;
				}

				ret = actfunc (runlevel, service);
				if (ret < 0)
					retval = EXIT_FAILURE;
				num_updated += ret;
			}

			if (retval == EXIT_SUCCESS && num_updated == 0 && action & DODELETE)
				ewarnx ("%s: service `%s' not found in any of the specified runlevels", applet, service);
		}
	}

	rc_strlist_free (runlevels);
	return (retval);
}

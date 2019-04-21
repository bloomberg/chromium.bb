/*
 * fontconfig/fc-conflist/fc-conflist.c
 *
 * Copyright © 2003 Keith Packard
 * Copyright © 2014 Red Hat, Inc.
 * Red Hat Author(s): Akira TAGOH
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#ifdef linux
#define HAVE_GETOPT_LONG 1
#endif
#define HAVE_GETOPT 1
#endif

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(x)		(dgettext(GETTEXT_PACKAGE, x))
#else
#define dgettext(d, s)	(s)
#define _(x)		(x)
#endif

#ifndef HAVE_GETOPT
#define HAVE_GETOPT 0
#endif
#ifndef HAVE_GETOPT_LONG
#define HAVE_GETOPT_LONG 0
#endif

#if HAVE_GETOPT_LONG
#undef  _GNU_SOURCE
#define _GNU_SOURCE
#include <getopt.h>
static const struct option longopts[] = {
    {"version", 0, 0, 'V'},
    {"help", 0, 0, 'h'},
    {NULL,0,0,0},
};
#else
#if HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#endif

static void
usage (char *program, int error)
{
    FILE *file = error ? stderr : stdout;
#if HAVE_GETOPT_LONG
    fprintf (file, _("usage: %s [-Vh] [--version] [--help]\n"),
	     program);
#else
    fprintf (file, _("usage: %s [-Vh]\n"),
	     program);
#endif
    fprintf (file, _("Show the ruleset files information on the system\n"));
    fprintf (file, "\n");
#if HAVE_GETOPT_LONG
    fprintf (file, _("  -V, --version        display font config version and exit\n"));
    fprintf (file, _("  -h, --help           display this help and exit\n"));
#else
    fprintf (file, _("  -V         (version)      display font config version and exit\n"));
    fprintf (file, _("  -h         (help)         display this help and exit\n"));
#endif
    exit (error);
}

int
main (int argc, char **argv)
{
    FcConfig *config;
    FcConfigFileInfoIter iter;

#if HAVE_GETOPT_LONG || HAVE_GETOPT
    int		c;

    setlocale (LC_ALL, "");
#if HAVE_GETOPT_LONG
    while ((c = getopt_long (argc, argv, "Vh", longopts, NULL)) != -1)
#else
    while ((c = getopt (argc, argv, "Vh")) != -1)
#endif
    {
	switch (c) {
	case 'V':
	    fprintf (stderr, "fontconfig version %d.%d.%d\n",
		     FC_MAJOR, FC_MINOR, FC_REVISION);
	    exit (0);
	case 'h':
	    usage (argv[0], 0);
	default:
	    usage (argv[0], 1);
	}
    }
#endif

    config = FcConfigGetCurrent ();
    FcConfigFileInfoIterInit (config, &iter);
    do
    {
	FcChar8 *name, *desc;
	FcBool enabled;

	if (FcConfigFileInfoIterGet (config, &iter, &name, &desc, &enabled))
	{
	    printf ("%c %s: %s\n", enabled ? '+' : '-', name, desc);
	    FcStrFree (name);
	    FcStrFree (desc);
	}
    } while (FcConfigFileInfoIterNext (config, &iter));

    FcFini ();

    return 0;
}

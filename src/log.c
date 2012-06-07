/*
 * Copyright © 2012 Martin Minarik
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <wayland-server.h>
#include <wayland-util.h>

#include "log.h"

static FILE *weston_logfile = NULL;

static char cached_timestamp[21];
static int cached_tv_sec = 0;

static int weston_log_timestamp(void)
{
	struct timespec tp;
	unsigned int time;

	clock_gettime(CLOCK_REALTIME, &tp);
	time = (tp.tv_sec * 1000000L) + (tp.tv_nsec / 1000);

	if (cached_tv_sec != tp.tv_sec) {
		char string[26];
		struct tm *lt = localtime(&tp.tv_sec);

		cached_tv_sec = tp.tv_sec;

		strftime (string,24,"%Y-%m-%d %H:%M:%S", lt);
		strncpy (cached_timestamp, string, 20);
	}

	return fprintf(weston_logfile, "[%s.%03u] ", cached_timestamp, tp.tv_nsec/1000000);
}

static int weston_log_print(const char *fmt, va_list arg)
{
	int l;
	l = vfprintf(weston_logfile, fmt, arg);
	fflush(weston_logfile);
	return l;
}

static void
custom_handler(const char *fmt, va_list arg)
{
	weston_log_timestamp();
	fprintf(weston_logfile, "libwayland: ");
	weston_log_print(fmt, arg);
}

void
weston_log_file_open(const char *filename)
{
	wl_log_set_handler_server(custom_handler);

	weston_logfile = fopen(filename, "a");
	if (weston_logfile == NULL)
		weston_logfile = stderr;
}

void
weston_log_file_close()
{
	if (weston_logfile != stderr)
		fclose(weston_logfile);
	weston_logfile = stderr;
}

WL_EXPORT int
weston_log(const char *fmt, ...)
{
	int l;
	va_list argp;
	va_start(argp, fmt);
	l = weston_log_timestamp();
	l += weston_log_print(fmt, argp);
	va_end(argp);
	return l;
}

WL_EXPORT int
weston_log_continue(const char *fmt, ...)
{
	int l;
	va_list argp;
	va_start(argp, fmt);
	l = weston_log_print(fmt, argp);
	va_end(argp);
	return l;
}

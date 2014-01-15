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

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <wayland-util.h>

#include "compositor.h"

static FILE *weston_logfile = NULL;

static int cached_tm_mday = -1;

static int weston_log_timestamp(void)
{
	struct timeval tv;
	struct tm *brokendown_time;
	char string[128];

	gettimeofday(&tv, NULL);

	brokendown_time = localtime(&tv.tv_sec);
	if (brokendown_time == NULL)
		return fprintf(weston_logfile, "[(NULL)localtime] ");

	if (brokendown_time->tm_mday != cached_tm_mday) {
		strftime(string, sizeof string, "%Y-%m-%d %Z", brokendown_time);
		fprintf(weston_logfile, "Date: %s\n", string);

		cached_tm_mday = brokendown_time->tm_mday;
	}

	strftime(string, sizeof string, "%H:%M:%S", brokendown_time);

	return fprintf(weston_logfile, "[%s.%03li] ", string, tv.tv_usec/1000);
}

static void
custom_handler(const char *fmt, va_list arg)
{
	weston_log_timestamp();
	fprintf(weston_logfile, "libwayland: ");
	vfprintf(weston_logfile, fmt, arg);
}

void
weston_log_file_open(const char *filename)
{
	wl_log_set_handler_server(custom_handler);

	if (filename != NULL)
		weston_logfile = fopen(filename, "a");

	if (weston_logfile == NULL)
		weston_logfile = stderr;
	else
		setvbuf(weston_logfile, NULL, _IOLBF, 256);
}

void
weston_log_file_close()
{
	if ((weston_logfile != stderr) && (weston_logfile != NULL))
		fclose(weston_logfile);
	weston_logfile = stderr;
}

WL_EXPORT int
weston_vlog(const char *fmt, va_list ap)
{
	int l;

	l = weston_log_timestamp();
	l += vfprintf(weston_logfile, fmt, ap);

	return l;
}

WL_EXPORT int
weston_log(const char *fmt, ...)
{
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = weston_vlog(fmt, argp);
	va_end(argp);

	return l;
}

WL_EXPORT int
weston_vlog_continue(const char *fmt, va_list argp)
{
	return vfprintf(weston_logfile, fmt, argp);
}

WL_EXPORT int
weston_log_continue(const char *fmt, ...)
{
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = weston_vlog_continue(fmt, argp);
	va_end(argp);

	return l;
}

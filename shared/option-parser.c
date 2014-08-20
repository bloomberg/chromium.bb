/*
 * Copyright © 2012 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config-parser.h"

static int
handle_option(const struct weston_option *option, char *value)
{
	char* p;

	switch (option->type) {
	case WESTON_OPTION_INTEGER:
		* (int32_t *) option->data = strtol(value, &p, 0);
		return *value && !*p;
	case WESTON_OPTION_UNSIGNED_INTEGER:
		* (uint32_t *) option->data = strtoul(value, &p, 0);
		return *value && !*p;
	case WESTON_OPTION_STRING:
		* (char **) option->data = strdup(value);
		return 1;
	default:
		assert(0);
	}
}

static int
long_option(const struct weston_option *options, int count, char *arg)
{
	int k, len;

	for (k = 0; k < count; k++) {
		if (!options[k].name)
			continue;

		len = strlen(options[k].name);
		if (strncmp(options[k].name, arg + 2, len) != 0)
			continue;

		if (options[k].type == WESTON_OPTION_BOOLEAN) {
			if (!arg[len + 2]) {
				* (int32_t *) options[k].data = 1;

				return 1;
			}
		} else if (arg[len+2] == '=') {
			return handle_option(options + k, arg + len + 3);
		}
	}

	return 0;
}

static int
short_option(const struct weston_option *options, int count, char *arg)
{
	int k;

	if (!arg[1])
		return 0;

	for (k = 0; k < count; k++) {
		if (options[k].short_name != arg[1])
			continue;

		if (options[k].type == WESTON_OPTION_BOOLEAN) {
			if (!arg[2]) {
				* (int32_t *) options[k].data = 1;

				return 1;
			}
		} else {
			return handle_option(options + k, arg + 2);
		}
	}

	return 0;
}

int
parse_options(const struct weston_option *options,
	      int count, int *argc, char *argv[])
{
	int i, j;

	for (i = 1, j = 1; i < *argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				if (long_option(options, count, argv[i]))
					continue;
			} else if (short_option(options, count, argv[i]))
				continue;
		}
		argv[j++] = argv[i];
	}
	argv[j] = NULL;
	*argc = j;

	return j;
}

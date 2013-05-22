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

static void
handle_option(const struct weston_option *option, char *value)
{
	switch (option->type) {
	case WESTON_OPTION_INTEGER:
		* (int32_t *) option->data = strtol(value, NULL, 0);
		return;
	case WESTON_OPTION_UNSIGNED_INTEGER:
		* (uint32_t *) option->data = strtoul(value, NULL, 0);
		return;
	case WESTON_OPTION_STRING:
		* (char **) option->data = strdup(value);
		return;
	case WESTON_OPTION_BOOLEAN:
		* (int32_t *) option->data = 1;
		return;
	default:
		assert(0);
	}
}

int
parse_options(const struct weston_option *options,
	      int count, int *argc, char *argv[])
{
	int i, j, k, len = 0;

	for (i = 1, j = 1; i < *argc; i++) {
		for (k = 0; k < count; k++) {
			if (options[k].name)
				len = strlen(options[k].name);
			if (options[k].name &&
			    argv[i][0] == '-' &&
			    argv[i][1] == '-' &&
			    strncmp(options[k].name, &argv[i][2], len) == 0 &&
			    (argv[i][len + 2] == '=' || argv[i][len + 2] == '\0')) {
				handle_option(&options[k], &argv[i][len + 3]);
				break;
			} else if (options[k].short_name &&
				   argv[i][0] == '-' &&
				   options[k].short_name == argv[i][1]) {
				handle_option(&options[k], &argv[i][2]);
				break;
			}
		}
		if (k == count)
			argv[j++] = argv[i];
	}
	argv[j] = NULL;
	*argc = j;

	return j;
}

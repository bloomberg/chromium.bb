/*
 * Copyright © 2012 Collabora, Ltd.
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include "test-runner.h"

static int
parse_count(const char *str, int *value)
{
	char *end;
	long v;

	errno = 0;
	v = strtol(str, &end, 0);
	if ((errno == ERANGE && (v == LONG_MAX || v == LONG_MIN)) ||
	    (errno != 0 && v == 0) ||
	    (end == str) ||
	    (*end != '\0')) {
		return -1;
	}

	if (v < 0 || v > INT_MAX) {
		return -1;
	}

	*value = v;
	return 0;
}

int main(int argc, char *argv[])
{
	int expected;

	if (argc != 2)
		goto help_out;

	if (parse_count(argv[1], &expected) < 0)
		goto help_out;

	if (count_open_fds() == expected)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;

help_out:
	fprintf(stderr, "Usage: %s N\n"
		"where N is the expected number of open file descriptors.\n"
		"This program exits with a failure if the number "
		"does not match exactly.\n", argv[0]);

	return EXIT_FAILURE;
}

/*
 * Copyright © 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include "drmtest.h"

/**
 * Checks drmGetDeviceNameFromFd
 *
 * This tests that we can get the actual version out, and that setting invalid
 * major/minor numbers fails appropriately.  It does not check the actual
 * behavior differenses resulting from an increased DI version.
 */
int main(int argc, char **argv)
{
	int fd;
	const char *name = "/dev/dri/card0";
	char *v;

	fd = open("/dev/dri/card0", O_RDWR);
	if (fd < 0)
		return 0;

	v = drmGetDeviceNameFromFd(fd);
	close(fd);

	assert(strcmp(name, v) == 0);
	drmFree(v);

	return 0;
}

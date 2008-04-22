/*
 * Copyright © 2008 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "mmfs.h"

static void
create_mmfs_device()
{
	struct stat sb;
	int ret;

	ret = stat(MMFS_DEVICE_PATH, &sb);

	if (ret == 0)
		return;

	ret = mknod(MMFS_DEVICE_PATH, S_IFCHR | S_IRUSR | S_IWUSR,
		    makedev(MMFS_DEVICE_MAJOR, 0));

	if (ret != 0)
		errx(1, "mknod()");
}


int main(int argc, char **argv)
{
	int fd;

	create_mmfs_device();

	fd = open(MMFS_DEVICE_PATH, O_RDWR);
	if (fd == -1)
		errx(1, "open()");

	close(fd);

	return 0;
}

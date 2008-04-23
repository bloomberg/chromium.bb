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
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include "mmfs.h"

static void
test_bad_unref(int fd)
{
	struct mmfs_unreference_args unref;
	int ret;

	printf("Testing error return on bad unreference ioctl.\n");

	unref.handle = 0x10101010;
	ret = ioctl(fd, MMFS_IOCTL_UNREFERENCE, &unref);

	assert(ret == -1 && errno == EINVAL);
}

static void
test_bad_ioctl(int fd)
{
	int ret;

	printf("Testing error return on bad ioctl.\n");

	ret = ioctl(fd, _IO(MMFS_IOCTL_BASE, 0xf0), 0);

	assert(ret == -1 && errno == EINVAL);
}

static void
test_alloc_unref(int fd)
{
	struct mmfs_alloc_args alloc;
	struct mmfs_unreference_args unref;
	int ret;

	printf("Testing allocating and unreferencing an object.\n");

	memset(&alloc, 0, sizeof(alloc));
	alloc.size = 16 * 1024;
	ret = ioctl(fd, MMFS_IOCTL_ALLOC, &alloc);
	assert(ret == 0);

	unref.handle = alloc.handle;
	ret = ioctl(fd, MMFS_IOCTL_UNREFERENCE, &unref);
}

static void
test_alloc_close(int fd)
{
	struct mmfs_alloc_args alloc;
	int ret;

	printf("Testing closing with an object allocated.\n");

	memset(&alloc, 0, sizeof(alloc));
	alloc.size = 16 * 1024;
	ret = ioctl(fd, MMFS_IOCTL_ALLOC, &alloc);
	assert(ret == 0);

	close(fd);
}

int main(int argc, char **argv)
{
	int fd;

	fd = open_mmfs_device();

	test_bad_ioctl(fd);
	test_bad_unref(fd);
	test_alloc_unref(fd);
	test_alloc_close(fd);

	close(fd);

	return 0;
}

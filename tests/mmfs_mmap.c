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

#define MMFS_BUFFER_SIZE 16384

int do_read(int fd, int handle, void *buf, int offset, int size)
{
	struct mmfs_pread_args read;

	/* Ensure that we don't have any convenient data in buf in case
	 * we fail.
	 */
	memset(buf, 0xd0, size);

	memset(&read, 0, sizeof(read));
	read.handle = handle;
	read.data = buf;
	read.size = size;
	read.offset = offset;

	return ioctl(fd, MMFS_IOCTL_PREAD, &read);
}

int do_write(int fd, int handle, void *buf, int offset, int size)
{
	struct mmfs_pwrite_args write;

	memset(&write, 0, sizeof(write));
	write.handle = handle;
	write.data = buf;
	write.size = size;
	write.offset = offset;

	return ioctl(fd, MMFS_IOCTL_PWRITE, &write);
}

int main(int argc, char **argv)
{
	int fd;
	struct mmfs_alloc_args alloc;
	struct mmfs_mmap_args mmap;
	struct mmfs_unreference_args unref;
	uint8_t expected[MMFS_BUFFER_SIZE];
	uint8_t buf[MMFS_BUFFER_SIZE];
	int ret;
	int handle;

	fd = open_mmfs_device();

	memset(&mmap, 0, sizeof(mmap));
	mmap.handle = 0x10101010;
	mmap.offset = 0;
	mmap.size = 4096;
	printf("Testing mmaping of bad object.\n");
	ret = ioctl(fd, MMFS_IOCTL_MMAP, &mmap);
	assert(ret == -1 && errno == EINVAL);

	memset(&alloc, 0, sizeof(alloc));
	alloc.size = MMFS_BUFFER_SIZE;
	ret = ioctl(fd, MMFS_IOCTL_ALLOC, &alloc);
	assert(ret == 0);
	handle = alloc.handle;

	printf("Testing mmaping of newly allocated object.\n");
	mmap.handle = handle;
	mmap.offset = 0;
	mmap.size = MMFS_BUFFER_SIZE;
	ret = ioctl(fd, MMFS_IOCTL_MMAP, &mmap);
	assert(ret == 0);

	printf("Testing contents of newly allocated object.\n");
	memset(expected, 0, sizeof(expected));
	assert(memcmp(mmap.addr, expected, sizeof(expected)) == 0);

	printf("Testing coherency of writes and mmap reads.\n");
	memset(buf, 0, sizeof(buf));
	memset(buf + 1024, 0x01, 1024);
	memset(expected + 1024, 0x01, 1024);
	ret = do_write(fd, handle, buf, 0, MMFS_BUFFER_SIZE);
	assert(ret == 0);
	assert(memcmp(buf, mmap.addr, sizeof(buf)) == 0);

	printf("Testing that mapping stays after unreference\n");
	unref.handle = handle;
	ret = ioctl(fd, MMFS_IOCTL_UNREFERENCE, &unref);
	assert(ret == 0);
	assert(memcmp(buf, mmap.addr, sizeof(buf)) == 0);

	printf("Testing unmapping\n");
	munmap(mmap.addr, MMFS_BUFFER_SIZE);

	close(fd);

	return 0;
}

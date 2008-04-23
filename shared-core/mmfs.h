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

#include <sys/ioctl.h>

/** @file mmfs.h
 * This file provides ioctl and ioctl argument definitions for using the
 * mmfs device.
 */

#ifdef __KERNEL__
#include "mmfs_drv.h"
#endif /* __KERNEL__ */

#define MMFS_DEVICE_PATH		"/dev/mmfs"
/* XXX: Choose non-experimental major */
#define MMFS_DEVICE_MAJOR		246

struct mmfs_alloc_args {
	/**
	 * Requested size for the object.
	 *
	 * The (page-aligned) allocated size for the object will be returned.
	 */
	uint32_t size;
	/** Returned handle for the object. */
	uint32_t handle;
};

struct mmfs_unreference_args {
	/** Handle of the object to be unreferenced. */
	uint32_t handle;
};

struct mmfs_link_args {
	/** Handle for the object being given a name. */
	uint32_t handle;
	/** Requested file name to export the object under. */
	char *name;
	/** Requested file mode to export the object under. */
	mode_t mode;
};

struct mmfs_pread_args {
	/** Handle for the object being read. */
	uint32_t handle;
	/** Offset into the object to read from */
	off_t offset;
	/** Length of data to read */
	size_t size;
	/** Pointer to write the data into. */
	void *data;
};

struct mmfs_pwrite_args {
	/** Handle for the object being written to. */
	uint32_t handle;
	/** Offset into the object to write to */
	off_t offset;
	/** Length of data to write */
	size_t size;
	/** Pointer to read the data from. */
	void *data;
};

struct mmfs_mmap_args {
	/** Handle for the object being mapped. */
	uint32_t handle;
	/** Offset in the object to map. */
	off_t offset;
	/**
	 * Length of data to map.
	 *
	 * The value will be page-aligned.
	 */
	size_t size;
	/** Returned pointer the data was mapped at */
	void *addr;
};

/**
 * \name Ioctls Definitions
 */
/* @{ */

#define MMFS_IOCTL_BASE			'm'
#define MMFS_IO(nr)			_IO(MMFS_IOCTL_BASE, nr)
#define MMFS_IOR(nr,type)		_IOR(MMFS_IOCTL_BASE, nr, type)
#define MMFS_IOW(nr,type)		_IOW(MMFS_IOCTL_BASE, nr, type)
#define MMFS_IOWR(nr,type)		_IOWR(MMFS_IOCTL_BASE, nr, type)

/** This ioctl allocates an object and returns a handle referencing it. */
#define MMFS_IOCTL_ALLOC		MMFS_IOWR(0x00, struct mmfs_alloc_args)

/**
 * This ioctl releases the reference on the handle returned from
 * MMFS_IOCTL_ALLOC.
 */
#define MMFS_IOCTL_UNREFERENCE		MMFS_IOR(0x01, struct mmfs_unreference_args)

/**
 * This ioctl creates a file in the mmfs filesystem representing an object.
 *
 * XXX: Need a way to get handle from fd or name.
 */
#define MMFS_IOCTL_LINK			MMFS_IOWR(0x02, struct mmfs_link_args)

/** This ioctl copies data from an object into a user address. */
#define MMFS_IOCTL_PREAD		MMFS_IOWR(0x03, struct mmfs_pread_args)

/** This ioctl copies data from a user address into an object. */
#define MMFS_IOCTL_PWRITE		MMFS_IOWR(0x04, struct mmfs_pwrite_args)

/** This ioctl maps data from the object into the user address space. */
#define MMFS_IOCTL_MMAP			MMFS_IOWR(0x05, struct mmfs_mmap_args)

/* }@ */

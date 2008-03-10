/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Dave Airlie
 * Copyright 2007 Alex Deucher
 * All Rights Reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * Authors:
 *    Jérôme Glisse <glisse@freedesktop.org>
 */
#ifndef __RADEON_MS_DRM_H__
#define __RADEON_MS_DRM_H__

/* Fence
 * We have only one fence class as we submit command through th
 * same fifo so there is no need to synchronize buffer btw different
 * cmd stream.
 *
 * Set DRM_RADEON_FENCE_FLAG_FLUSHED if you want a flush with
 * emission of the fence
 *
 * For fence type we have the native DRM EXE type and the radeon RW
 * type.
 */
#define DRM_RADEON_FENCE_CLASS_ACCEL		0
#define DRM_RADEON_FENCE_TYPE_RW		2
#define DRM_RADEON_FENCE_FLAG_FLUSHED		0x01000000

/* radeon ms ioctl */
#define DRM_RADEON_EXECBUFFER			0x00

struct drm_radeon_execbuffer_arg {
	uint64_t    next;
	uint32_t    reloc_offset;
	union {
		struct drm_bo_op_req    req;
		struct drm_bo_arg_rep   rep;
	} d;
};

struct drm_radeon_execbuffer {
	uint32_t args_count;
	uint64_t args;
	uint32_t cmd_size;
	struct drm_fence_arg fence_arg;
};

#endif

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
#ifndef __AMD_DRM_H__
#define __AMD_DRM_H__

/* Fence
 *
 * Set DRM_AND_FENCE_FLAG_FLUSH if you want a flush with
 * emission of the fence
 *
 * For fence type we have the native DRM EXE type and the amd R & W type.
 */
#define DRM_AMD_FENCE_CLASS_2D                  0
#define DRM_AMD_FENCE_TYPE_R                    (1 << 1)
#define DRM_AMD_FENCE_TYPE_W                    (1 << 2)
#define DRM_AMD_FENCE_FLAG_FLUSH                0x01000000

/* ioctl */
#define DRM_AMD_CMD                             0x00
#define DRM_AMD_RESETCP                         0x01

/* cmd ioctl */

#define DRM_AMD_CMD_BO_TYPE_INVALID             0
#define DRM_AMD_CMD_BO_TYPE_CMD_RING            (1 << 0)
#define DRM_AMD_CMD_BO_TYPE_CMD_INDIRECT        (1 << 1)
#define DRM_AMD_CMD_BO_TYPE_DATA                (1 << 2)

struct drm_amd_cmd_bo_offset
{
	uint64_t                next;
	uint64_t                offset;
	uint32_t		cs_id;
};

struct drm_amd_cmd_bo
{
	uint32_t                type;
	uint64_t                next;
	uint64_t                offset;
	struct drm_bo_op_req    op_req;
	struct drm_bo_arg_rep   op_rep;
};

struct drm_amd_cmd
{
	uint32_t                cdw_count;
	uint32_t                bo_count;
	uint64_t                bo;
	struct drm_fence_arg    fence_arg;
};

#endif

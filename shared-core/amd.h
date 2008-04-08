/*
 * Copyright 2007 Jérôme Glisse
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
 *
 * Authors:
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#ifndef __AMD_H__
#define __AMD_H__

/* struct amd_cbuffer are for command buffer, this is the structure passed
 * around during command validation (ie check that user have the right to
 * execute the given command).
 */
struct amd_cbuffer_arg
{
	struct list_head         list;
	struct drm_buffer_object *buffer;
	int32_t                  dw_id;
};

struct amd_cbuffer
{
	uint32_t                 *cbuffer;
	uint32_t                 cbuffer_dw_count;
	struct amd_cbuffer_arg   arg_unused;
	struct amd_cbuffer_arg   arg_used;
	struct amd_cbuffer_arg   *args;
	void			 *driver;
};

struct amd_cbuffer_checker
{
	uint32_t numof_p0_checkers;
	uint32_t numof_p3_checkers;
	int (*check)(struct drm_device *dev, struct amd_cbuffer *cbuffer);
	int (**check_p0)(struct drm_device *dev, struct amd_cbuffer *cbuffer,
			 int dw_id, int reg);
	int (**check_p3)(struct drm_device *dev, struct amd_cbuffer *cbuffer,
			 int dw_id, int op, int count);
};

struct amd_cbuffer_arg *
amd_cbuffer_arg_from_dw_id(struct amd_cbuffer_arg *head, uint32_t dw_id);
int amd_cbuffer_check(struct drm_device *dev, struct amd_cbuffer *cbuffer);


/* struct amd_fb amd is for storing amd framebuffer informations
 */
struct amd_fb
{
	struct drm_device       *dev;
	struct drm_crtc         *crtc;
	struct drm_display_mode *fb_mode;
	struct drm_framebuffer  *fb;
};

#endif

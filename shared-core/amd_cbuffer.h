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
#ifndef __AMD_CBUFFER_H__
#define __AMD_CBUFFER_H__

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
};

#endif

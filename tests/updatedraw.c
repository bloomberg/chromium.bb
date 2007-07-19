/*
 * Copyright © 2007 Intel Corporation
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

#include "drmtest.h"

static void
set_draw_cliprects_empty(int fd, int drawable)
{
	int ret;
	struct drm_update_draw update;

	update.handle = drawable;
	update.type = DRM_DRAWABLE_CLIPRECTS;
	update.num = 0;
	update.data = 0;

	ret = ioctl(fd, DRM_IOCTL_UPDATE_DRAW, &update);
	assert(ret == 0);
}

static void
set_draw_cliprects_empty_fail(int fd, int drawable)
{
	int ret;
	struct drm_update_draw update;

	update.handle = drawable;
	update.type = DRM_DRAWABLE_CLIPRECTS;
	update.num = 0;
	update.data = 0;

	ret = ioctl(fd, DRM_IOCTL_UPDATE_DRAW, &update);
	assert(ret == -1 && errno == EINVAL);
}

static void
set_draw_cliprects_2(int fd, int drawable)
{
	int ret;
	struct drm_update_draw update;
	drm_clip_rect_t rects[2];

	rects[0].x1 = 0;
	rects[0].y1 = 0;
	rects[0].x2 = 10;
	rects[0].y2 = 10;

	rects[1].x1 = 10;
	rects[1].y1 = 10;
	rects[1].x2 = 20;
	rects[1].y2 = 20;

	update.handle = drawable;
	update.type = DRM_DRAWABLE_CLIPRECTS;
	update.num = 2;
	update.data = (unsigned long long)(uintptr_t)&rects;

	ret = ioctl(fd, DRM_IOCTL_UPDATE_DRAW, &update);
	assert(ret == 0);
}

/**
 * Tests drawable management: adding, removing, and updating the cliprects of
 * drawables.
 */
int main(int argc, char **argv)
{
	drm_draw_t drawarg;
	int fd, ret, drawable;

	fd = drm_open_any_master();

	/* Create a drawable.
	 * IOCTL_ADD_DRAW is RDWR, though it should really just be RD
	 */
	drawarg.handle = 0;
	ret = ioctl(fd, DRM_IOCTL_ADD_DRAW, &drawarg);
	assert(ret == 0);
	drawable = drawarg.handle;

	/* Do a series of cliprect updates */
	set_draw_cliprects_empty(fd, drawable);
	set_draw_cliprects_2(fd, drawable);
	set_draw_cliprects_empty(fd, drawable);

	/* Remove our drawable */
	drawarg.handle = drawable;
	ret = ioctl(fd, DRM_IOCTL_RM_DRAW, &drawarg);
	assert(ret == 0);
	drawable = drawarg.handle;

	/* Check that removing an unknown drawable returns error */
	drawarg.handle = 0x7fffffff;
	ret = ioctl(fd, DRM_IOCTL_RM_DRAW, &drawarg);
	assert(ret == -1 && errno == EINVAL);
	drawable = drawarg.handle;

	/* Attempt to set cliprects on a nonexistent drawable */
	set_draw_cliprects_empty_fail(fd, drawable);

	close(fd);
	return 0;
}

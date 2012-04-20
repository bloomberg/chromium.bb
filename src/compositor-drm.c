/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <gbm.h>
#include <libbacklight.h>

#include "compositor.h"
#include "evdev.h"
#include "launcher-util.h"

struct drm_compositor {
	struct weston_compositor base;

	struct udev *udev;
	struct wl_event_source *drm_source;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_drm_source;

	struct {
		int id;
		int fd;
	} drm;
	struct gbm_device *gbm;
	uint32_t *crtcs;
	int num_crtcs;
	uint32_t crtc_allocator;
	uint32_t connector_allocator;
	struct tty *tty;

	struct gbm_surface *dummy_surface;
	EGLSurface dummy_egl_surface;

	struct wl_list sprite_list;
	int sprites_are_broken;

	uint32_t prev_state;
};

struct drm_mode {
	struct weston_mode base;
	drmModeModeInfo mode_info;
};

struct drm_output {
	struct weston_output   base;

	uint32_t crtc_id;
	uint32_t connector_id;
	drmModeCrtcPtr original_crtc;

	struct gbm_surface *surface;
	EGLSurface egl_surface;
	uint32_t current_fb_id;
	uint32_t next_fb_id;
	struct gbm_bo *current_bo;
	struct gbm_bo *next_bo;

	struct wl_buffer *scanout_buffer;
	struct wl_listener scanout_buffer_destroy_listener;
	struct wl_buffer *pending_scanout_buffer;
	struct wl_listener pending_scanout_buffer_destroy_listener;
	struct backlight *backlight;
};

/*
 * An output has a primary display plane plus zero or more sprites for
 * blending display contents.
 */
struct drm_sprite {
	struct wl_list link;

	uint32_t fb_id;
	uint32_t pending_fb_id;
	struct weston_surface *surface;
	struct weston_surface *pending_surface;

	struct drm_compositor *compositor;

	struct wl_listener destroy_listener;
	struct wl_listener pending_destroy_listener;

	uint32_t possible_crtcs;
	uint32_t plane_id;
	uint32_t count_formats;

	int32_t src_x, src_y;
	uint32_t src_w, src_h;
	uint32_t dest_x, dest_y;
	uint32_t dest_w, dest_h;

	uint32_t formats[];
};

static int
surface_is_primary(struct weston_compositor *ec, struct weston_surface *es)
{
	struct weston_surface *primary;

	primary = container_of(ec->surface_list.next, struct weston_surface,
			       link);
	if (es == primary)
		return -1;
	return 0;
}

static int
drm_sprite_crtc_supported(struct weston_output *output_base, uint32_t supported)
{
	struct weston_compositor *ec = output_base->compositor;
	struct drm_compositor *c =(struct drm_compositor *) ec;
	struct drm_output *output = (struct drm_output *) output_base;
	int crtc;

	for (crtc = 0; crtc < c->num_crtcs; crtc++) {
		if (c->crtcs[crtc] != output->crtc_id)
			continue;

		if (supported & (1 << crtc))
			return -1;
	}

	return 0;
}

static int
drm_output_prepare_scanout_surface(struct drm_output *output)
{
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	struct weston_surface *es;

	es = container_of(c->base.surface_list.next,
			  struct weston_surface, link);

	/* Need to verify output->region contained in surface opaque
	 * region.  Or maybe just that format doesn't have alpha. */
	return -1;

	if (es->geometry.x != output->base.x ||
	    es->geometry.y != output->base.y ||
	    es->geometry.width != output->base.current->width ||
	    es->geometry.height != output->base.current->height ||
	    es->transform.enabled ||
	    es->image == EGL_NO_IMAGE_KHR)
		return -1;

	output->next_bo =
		gbm_bo_create_from_egl_image(c->gbm,
					     c->base.display, es->image,
					     es->geometry.width,
					     es->geometry.height,
					     GBM_BO_USE_SCANOUT);

	/* assert output->pending_scanout_buffer == NULL */
	output->pending_scanout_buffer = es->buffer;
	output->pending_scanout_buffer->busy_count++;

	wl_signal_add(&output->pending_scanout_buffer->resource.destroy_signal,
		      &output->pending_scanout_buffer_destroy_listener);

	pixman_region32_fini(&es->damage);
	pixman_region32_init(&es->damage);

	return 0;
}

static void
drm_output_render(struct drm_output *output, pixman_region32_t *damage)
{
	struct drm_compositor *compositor =
		(struct drm_compositor *) output->base.compositor;
	struct weston_surface *surface;

	if (!eglMakeCurrent(compositor->base.display, output->egl_surface,
			    output->egl_surface, compositor->base.context)) {
		fprintf(stderr, "failed to make current\n");
		return;
	}

	wl_list_for_each_reverse(surface, &compositor->base.surface_list, link)
		weston_surface_draw(surface, &output->base, damage);

	weston_output_do_read_pixels(&output->base);

	eglSwapBuffers(compositor->base.display, output->egl_surface);
	output->next_bo = gbm_surface_lock_front_buffer(output->surface);
	if (!output->next_bo) {
		fprintf(stderr, "failed to lock front buffer: %m\n");
		return;
	}
}

static void
drm_output_repaint(struct weston_output *output_base,
		   pixman_region32_t *damage)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_compositor *compositor =
		(struct drm_compositor *) output->base.compositor;
	struct drm_sprite *s;
	struct drm_mode *mode;
	uint32_t stride, handle;
	int ret = 0;

	drm_output_prepare_scanout_surface(output);
	if (!output->next_bo)
		drm_output_render(output, damage);

	stride = gbm_bo_get_pitch(output->next_bo);
	handle = gbm_bo_get_handle(output->next_bo).u32;

	/* Destroy and set to NULL now so we don't try to
	 * gbm_surface_release_buffer() a client buffer in the
	 * page_flip_handler. */
	if (output->pending_scanout_buffer) {
		gbm_bo_destroy(output->next_bo);
		output->next_bo = NULL;
	}

	ret = drmModeAddFB(compositor->drm.fd,
			   output->base.current->width,
			   output->base.current->height,
			   24, 32, stride, handle, &output->next_fb_id);
	if (ret) {
		fprintf(stderr, "failed to create kms fb: %m\n");
		gbm_surface_release_buffer(output->surface, output->next_bo);
		output->next_bo = NULL;
		return;
	}

	mode = container_of(output->base.current, struct drm_mode, base);
	if (output->current_fb_id == 0) {
		ret = drmModeSetCrtc(compositor->drm.fd, output->crtc_id,
				     output->next_fb_id, 0, 0,
				     &output->connector_id, 1,
				     &mode->mode_info);
		if (ret) {
			fprintf(stderr, "set mode failed: %m\n");
			return;
		}
	}

	if (drmModePageFlip(compositor->drm.fd, output->crtc_id,
			    output->next_fb_id,
			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
		fprintf(stderr, "queueing pageflip failed: %m\n");
		return;
	}

	/*
	 * Now, update all the sprite surfaces
	 */
	wl_list_for_each(s, &compositor->sprite_list, link) {
		uint32_t flags = 0;
		drmVBlank vbl = {
			.request.type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT,
			.request.sequence = 1,
		};

		if (!drm_sprite_crtc_supported(output_base, s->possible_crtcs))
			continue;

		ret = drmModeSetPlane(compositor->drm.fd, s->plane_id,
				      output->crtc_id, s->pending_fb_id, flags,
				      s->dest_x, s->dest_y,
				      s->dest_w, s->dest_h,
				      s->src_x, s->src_y,
				      s->src_w, s->src_h);
		if (ret)
			fprintf(stderr, "setplane failed: %d: %s\n",
				ret, strerror(errno));

		/*
		 * Queue a vblank signal so we know when the surface
		 * becomes active on the display or has been replaced.
		 */
		vbl.request.signal = (unsigned long)s;
		ret = drmWaitVBlank(compositor->drm.fd, &vbl);
		if (ret) {
			fprintf(stderr, "vblank event request failed: %d: %s\n",
				ret, strerror(errno));
		}
	}

	return;
}

static void
vblank_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec,
	       void *data)
{
	struct drm_sprite *s = (struct drm_sprite *)data;
	struct drm_compositor *c = s->compositor;

	if (s->surface) {
		weston_buffer_post_release(s->surface->buffer);
		wl_list_remove(&s->destroy_listener.link);
		s->surface = NULL;
		drmModeRmFB(c->drm.fd, s->fb_id);
		s->fb_id = 0;
	}

	if (s->pending_surface) {
		wl_list_remove(&s->pending_destroy_listener.link);
		wl_signal_add(&s->pending_surface->buffer->resource.destroy_signal,
			      &s->destroy_listener);
		s->surface = s->pending_surface;
		s->pending_surface = NULL;
		s->fb_id = s->pending_fb_id;
		s->pending_fb_id = 0;
	}
}

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct drm_output *output = (struct drm_output *) data;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	uint32_t msecs;

	if (output->current_fb_id)
		drmModeRmFB(c->drm.fd, output->current_fb_id);
	output->current_fb_id = output->next_fb_id;
	output->next_fb_id = 0;

	if (output->scanout_buffer) {
		weston_buffer_post_release(output->scanout_buffer);
		wl_list_remove(&output->scanout_buffer_destroy_listener.link);
		output->scanout_buffer = NULL;
	} else if (output->current_bo) {
		gbm_surface_release_buffer(output->surface,
					   output->current_bo);
	}

	output->current_bo = output->next_bo;
	output->next_bo = NULL;

	if (output->pending_scanout_buffer) {
		output->scanout_buffer = output->pending_scanout_buffer;
		wl_list_remove(&output->pending_scanout_buffer_destroy_listener.link);
		wl_signal_add(&output->scanout_buffer->resource.destroy_signal,
			      &output->scanout_buffer_destroy_listener);
		output->pending_scanout_buffer = NULL;
	}
	msecs = sec * 1000 + usec / 1000;
	weston_output_finish_frame(&output->base, msecs);
}

static int
drm_surface_format_supported(struct drm_sprite *s, uint32_t format)
{
	uint32_t i;

	for (i = 0; i < s->count_formats; i++)
		if (s->formats[i] == format)
			return 1;

	return 0;
}

static int
drm_surface_transform_supported(struct weston_surface *es)
{
	if (es->transform.enabled)
		return 0;

	return 1;
}

static int
drm_surface_overlap_supported(struct weston_output *output_base,
			      pixman_region32_t *overlap)
{
	/* We could potentially use a color key here if the surface left
	 * to display has rectangular regions
	 */
	if (pixman_region32_not_empty(overlap))
		return 0;
	return 1;
}

static void
drm_disable_unused_sprites(struct weston_output *output_base)
{
	struct weston_compositor *ec = output_base->compositor;
	struct drm_compositor *c =(struct drm_compositor *) ec;
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_sprite *s;
	int ret;

	wl_list_for_each(s, &c->sprite_list, link) {
		if (s->pending_fb_id)
			continue;

		ret = drmModeSetPlane(c->drm.fd, s->plane_id,
				      output->crtc_id, 0, 0,
				      0, 0, 0, 0, 0, 0, 0, 0);
		if (ret)
			fprintf(stderr,
				"failed to disable plane: %d: %s\n",
				ret, strerror(errno));
		drmModeRmFB(c->drm.fd, s->fb_id);
		s->surface = NULL;
		s->pending_surface = NULL;
		s->fb_id = 0;
		s->pending_fb_id = 0;
	}
}

/*
 * This function must take care to damage any previously assigned surface
 * if the sprite ends up binding to a different surface than in the
 * previous frame.
 */
static int
drm_output_prepare_overlay_surface(struct weston_output *output_base,
				   struct weston_surface *es,
				   pixman_region32_t *overlap)
{
	struct weston_compositor *ec = output_base->compositor;
	struct drm_compositor *c =(struct drm_compositor *) ec;
	struct drm_sprite *s;
	int found = 0;
	EGLint handle, stride;
	struct gbm_bo *bo;
	uint32_t fb_id = 0;
	uint32_t handles[4], pitches[4], offsets[4];
	int ret = 0;
	pixman_region32_t dest_rect, src_rect;
	pixman_box32_t *box;
	uint32_t format;

	if (c->sprites_are_broken)
		return -1;

	if (surface_is_primary(ec, es))
		return -1;

	if (es->image == EGL_NO_IMAGE_KHR)
		return -1;

	if (!drm_surface_transform_supported(es))
		return -1;

	if (!drm_surface_overlap_supported(output_base, overlap))
		return -1;

	wl_list_for_each(s, &c->sprite_list, link) {
		if (!drm_sprite_crtc_supported(output_base, s->possible_crtcs))
			continue;

		if (!s->pending_fb_id) {
			found = 1;
			break;
		}
	}

	/* No sprites available */
	if (!found)
		return -1;

	bo = gbm_bo_create_from_egl_image(c->gbm, c->base.display, es->image,
					  es->geometry.width, es->geometry.height,
					  GBM_BO_USE_SCANOUT);
	format = gbm_bo_get_format(bo);
	handle = gbm_bo_get_handle(bo).s32;
	stride = gbm_bo_get_pitch(bo);

	gbm_bo_destroy(bo);

	if (!drm_surface_format_supported(s, format))
		return -1;

	if (!handle)
		return -1;

	handles[0] = handle;
	pitches[0] = stride;
	offsets[0] = 0;

	ret = drmModeAddFB2(c->drm.fd, es->geometry.width, es->geometry.height,
			    format, handles, pitches, offsets,
			    &fb_id, 0);
	if (ret) {
		fprintf(stderr, "addfb2 failed: %d\n", ret);
		c->sprites_are_broken = 1;
		return -1;
	}

	if (s->surface && s->surface != es) {
		struct weston_surface *old_surf = s->surface;
		pixman_region32_fini(&old_surf->damage);
		pixman_region32_init_rect(&old_surf->damage,
					  old_surf->geometry.x, old_surf->geometry.y,
					  old_surf->geometry.width, old_surf->geometry.height);
	}

	s->pending_fb_id = fb_id;
	s->pending_surface = es;
	es->buffer->busy_count++;

	/*
	 * Calculate the source & dest rects properly based on actual
	 * postion (note the caller has called weston_surface_update_transform()
	 * for us already).
	 */
	pixman_region32_init(&dest_rect);
	pixman_region32_intersect(&dest_rect, &es->transform.boundingbox,
				  &output_base->region);
	pixman_region32_translate(&dest_rect, -output_base->x, -output_base->y);
	box = pixman_region32_extents(&dest_rect);
	s->dest_x = box->x1;
	s->dest_y = box->y1;
	s->dest_w = box->x2 - box->x1;
	s->dest_h = box->y2 - box->y1;
	pixman_region32_fini(&dest_rect);

	pixman_region32_init(&src_rect);
	pixman_region32_intersect(&src_rect, &es->transform.boundingbox,
				  &output_base->region);
	pixman_region32_translate(&src_rect, -es->geometry.x, -es->geometry.y);
	box = pixman_region32_extents(&src_rect);
	s->src_x = box->x1 << 16;
	s->src_y = box->y1 << 16;
	s->src_w = (box->x2 - box->x1) << 16;
	s->src_h = (box->y2 - box->y1) << 16;
	pixman_region32_fini(&src_rect);

	wl_signal_add(&es->buffer->resource.destroy_signal,
		      &s->pending_destroy_listener);
	return 0;
}

static int
drm_output_set_cursor(struct weston_output *output_base,
		      struct weston_input_device *eid);

static void
weston_output_set_cursor(struct weston_output *output,
			 struct weston_input_device *device,
			 pixman_region32_t *overlap)
{
	pixman_region32_t cursor_region;
	int prior_was_hardware;

	if (device->sprite == NULL)
		return;

	pixman_region32_init(&cursor_region);
	pixman_region32_intersect(&cursor_region,
				  &device->sprite->transform.boundingbox,
				  &output->region);

	if (!pixman_region32_not_empty(&cursor_region)) {
		drm_output_set_cursor(output, NULL);
		goto out;
	}

	prior_was_hardware = device->hw_cursor;
	if (pixman_region32_not_empty(overlap) ||
	    drm_output_set_cursor(output, device) < 0) {
		if (prior_was_hardware) {
			weston_surface_damage(device->sprite);
			drm_output_set_cursor(output, NULL);
		}
		device->hw_cursor = 0;
	} else {
		if (!prior_was_hardware)
			weston_surface_damage_below(device->sprite);
		pixman_region32_fini(&device->sprite->damage);
		pixman_region32_init(&device->sprite->damage);
		device->hw_cursor = 1;
	}

out:
	pixman_region32_fini(&cursor_region);
}

static void
drm_assign_planes(struct weston_output *output)
{
	struct weston_compositor *ec = output->compositor;
	struct weston_surface *es;
	pixman_region32_t overlap, surface_overlap;
	struct weston_input_device *device;

	/*
	 * Find a surface for each sprite in the output using some heuristics:
	 * 1) size
	 * 2) frequency of update
	 * 3) opacity (though some hw might support alpha blending)
	 * 4) clipping (this can be fixed with color keys)
	 *
	 * The idea is to save on blitting since this should save power.
	 * If we can get a large video surface on the sprite for example,
	 * the main display surface may not need to update at all, and
	 * the client buffer can be used directly for the sprite surface
	 * as we do for flipping full screen surfaces.
	 */
	pixman_region32_init(&overlap);
	wl_list_for_each(es, &ec->surface_list, link) {
		/*
		 * FIXME: try to assign hw cursors here too, they're just
		 * special overlays
		 */
		pixman_region32_init(&surface_overlap);
		pixman_region32_intersect(&surface_overlap, &overlap,
					  &es->transform.boundingbox);

		device = (struct weston_input_device *) ec->input_device;
		if (es == device->sprite) {
			weston_output_set_cursor(output, device,
						 &surface_overlap);

			if (!device->hw_cursor)
				pixman_region32_union(&overlap, &overlap,
						      &es->transform.boundingbox);
		} else if (!drm_output_prepare_overlay_surface(output, es,
							       &surface_overlap)) {
			pixman_region32_fini(&es->damage);
			pixman_region32_init(&es->damage);
		} else {
			pixman_region32_union(&overlap, &overlap,
					      &es->transform.boundingbox);
		}
		pixman_region32_fini(&surface_overlap);
	}
	pixman_region32_fini(&overlap);

	drm_disable_unused_sprites(output);
}

static int
drm_output_set_cursor(struct weston_output *output_base,
		      struct weston_input_device *eid)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	EGLint handle, stride;
	int ret = -1;
	struct gbm_bo *bo;

	if (eid == NULL) {
		drmModeSetCursor(c->drm.fd, output->crtc_id, 0, 0, 0);
		return 0;
	}

	if (eid->sprite->image == EGL_NO_IMAGE_KHR)
		goto out;

	if (eid->sprite->geometry.width > 64 ||
	    eid->sprite->geometry.height > 64)
		goto out;

	bo = gbm_bo_create_from_egl_image(c->gbm,
					  c->base.display,
					  eid->sprite->image, 64, 64,
					  GBM_BO_USE_CURSOR_64X64);
	/* Not suitable for hw cursor, fall back */
	if (bo == NULL)
		goto out;

	handle = gbm_bo_get_handle(bo).s32;
	stride = gbm_bo_get_pitch(bo);
	gbm_bo_destroy(bo);

	/* gbm_bo_create_from_egl_image() didn't always validate the usage
	 * flags, and in that case we might end up with a bad stride. */
	if (stride != 64 * 4)
		goto out;

	ret = drmModeSetCursor(c->drm.fd, output->crtc_id, handle, 64, 64);
	if (ret) {
		fprintf(stderr, "failed to set cursor: %s\n", strerror(-ret));
		goto out;
	}

	ret = drmModeMoveCursor(c->drm.fd, output->crtc_id,
				eid->sprite->geometry.x - output->base.x,
				eid->sprite->geometry.y - output->base.y);
	if (ret) {
		fprintf(stderr, "failed to move cursor: %s\n", strerror(-ret));
		goto out;
	}

out:
	if (ret)
		drmModeSetCursor(c->drm.fd, output->crtc_id, 0, 0, 0);
	return ret;
}

static void
drm_output_destroy(struct weston_output *output_base)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	drmModeCrtcPtr origcrtc = output->original_crtc;

	if (output->backlight)
		backlight_destroy(output->backlight);

	/* Turn off hardware cursor */
	drm_output_set_cursor(&output->base, NULL);

	/* Restore original CRTC state */
	drmModeSetCrtc(c->drm.fd, origcrtc->crtc_id, origcrtc->buffer_id,
		       origcrtc->x, origcrtc->y,
		       &output->connector_id, 1, &origcrtc->mode);
	drmModeFreeCrtc(origcrtc);

	c->crtc_allocator &= ~(1 << output->crtc_id);
	c->connector_allocator &= ~(1 << output->connector_id);

	eglDestroySurface(c->base.display, output->egl_surface);
	gbm_surface_destroy(output->surface);

	weston_output_destroy(&output->base);
	wl_list_remove(&output->base.link);

	free(output);
}

static struct drm_mode *
choose_mode (struct drm_output *output, struct weston_mode *target_mode)
{
	struct drm_mode *tmp_mode = NULL, *mode;

	if (output->base.current->width == target_mode->width && 
	    output->base.current->height == target_mode->height &&
	    (output->base.current->refresh == target_mode->refresh ||
	     target_mode->refresh == 0))
		return (struct drm_mode *)output->base.current;

	wl_list_for_each(mode, &output->base.mode_list, base.link) {
		if (mode->mode_info.hdisplay == target_mode->width &&
		    mode->mode_info.vdisplay == target_mode->height) {
			if (mode->mode_info.vrefresh == target_mode->refresh || 
          		    target_mode->refresh == 0) {
				return mode;
			} else if (!tmp_mode) 
				tmp_mode = mode;
		}
	}

	return tmp_mode;
}

static int
drm_output_switch_mode(struct weston_output *output_base, struct weston_mode *mode)
{
	struct drm_output *output;
	struct drm_mode *drm_mode;
	int ret;
	struct drm_compositor *ec;
	struct gbm_surface *surface;
	EGLSurface egl_surface;

	if (output_base == NULL) {
		fprintf(stderr, "output is NULL.\n");
		return -1;
	}

	if (mode == NULL) {
		fprintf(stderr, "mode is NULL.\n");
		return -1;
	}

	ec = (struct drm_compositor *)output_base->compositor;
	output = (struct drm_output *)output_base;
	drm_mode  = choose_mode (output, mode);

	if (!drm_mode) {
		printf("%s, invalid resolution:%dx%d\n", __func__, mode->width, mode->height);
		return -1;
	} else if (&drm_mode->base == output->base.current) {
		return 0;
	} else if (drm_mode->base.width == output->base.current->width &&
	           drm_mode->base.height == output->base.current->height) {
		/* only change refresh value */
		ret = drmModeSetCrtc(ec->drm.fd,
				     output->crtc_id,
				     output->current_fb_id, 0, 0,
				     &output->connector_id, 1, &drm_mode->mode_info);

		if (ret) {
			fprintf(stderr, "failed to set mode (%dx%d) %u Hz\n",
				drm_mode->base.width,
				drm_mode->base.height,
				drm_mode->base.refresh);
			ret = -1;
		} else {
			output->base.current->flags = 0;
			output->base.current = &drm_mode->base;
			drm_mode->base.flags = 
				WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
			ret = 0;
		}

		return ret;
	}

	drm_mode->base.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	surface = gbm_surface_create(ec->gbm,
			         drm_mode->base.width,
			         drm_mode->base.height,
				 GBM_FORMAT_XRGB8888,
				 GBM_BO_USE_SCANOUT |
				 GBM_BO_USE_RENDERING);
	if (!surface) {
		fprintf(stderr, "failed to create gbm surface\n");
		return -1;
	}

	egl_surface =
		eglCreateWindowSurface(ec->base.display,
				       ec->base.config,
				       surface, NULL);

	if (egl_surface == EGL_NO_SURFACE) {
		fprintf(stderr, "failed to create egl surface\n");
		goto err;
	}

	ret = drmModeSetCrtc(ec->drm.fd,
			     output->crtc_id,
			     output->current_fb_id, 0, 0,
			     &output->connector_id, 1, &drm_mode->mode_info);
	if (ret) {
		fprintf(stderr, "failed to set mode\n");
		goto err;
	}

	/* reset rendering stuff. */
	if (output->current_fb_id)
		drmModeRmFB(ec->drm.fd, output->current_fb_id);
	output->current_fb_id = 0;

	if (output->next_fb_id)
		drmModeRmFB(ec->drm.fd, output->next_fb_id);
	output->next_fb_id = 0;

	if (output->current_bo)
		gbm_surface_release_buffer(output->surface,
					   output->current_bo);
		output->current_bo = NULL;

	if (output->next_bo)
		gbm_surface_release_buffer(output->surface,
					   output->next_bo);
	output->next_bo = NULL;

	eglDestroySurface(ec->base.display, output->egl_surface);
	gbm_surface_destroy(output->surface);
	output->egl_surface = egl_surface;
	output->surface = surface;

	/*update output*/
	output->base.current = &drm_mode->base;
	output->base.dirty = 1;
	weston_output_move(&output->base, output->base.x, output->base.y);
	return 0;

err:
	eglDestroySurface(ec->base.display, egl_surface);
	gbm_surface_destroy(surface);
	return -1;
}

static int
on_drm_input(int fd, uint32_t mask, void *data)
{
	drmEventContext evctx;

	memset(&evctx, 0, sizeof evctx);
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.page_flip_handler = page_flip_handler;
	evctx.vblank_handler = vblank_handler;
	drmHandleEvent(fd, &evctx);

	return 1;
}

static int
init_egl(struct drm_compositor *ec, struct udev_device *device)
{
	EGLint major, minor, n;
	const char *filename, *sysnum;
	int fd;
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	sysnum = udev_device_get_sysnum(device);
	if (sysnum)
		ec->drm.id = atoi(sysnum);
	if (!sysnum || ec->drm.id < 0) {
		fprintf(stderr, "cannot get device sysnum\n");
		return -1;
	}

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	filename = udev_device_get_devnode(device);
	fd = open(filename, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		/* Probably permissions error */
		fprintf(stderr, "couldn't open %s, skipping\n",
			udev_device_get_devnode(device));
		return -1;
	}

	ec->drm.fd = fd;
	ec->gbm = gbm_create_device(ec->drm.fd);
	ec->base.display = eglGetDisplay(ec->gbm);
	if (ec->base.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	if (!eglInitialize(ec->base.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!eglChooseConfig(ec->base.display, config_attribs,
			     &ec->base.config, 1, &n) || n != 1) {
		fprintf(stderr, "failed to choose config: %d\n", n);
		return -1;
	}

	ec->base.context = eglCreateContext(ec->base.display, ec->base.config,
					    EGL_NO_CONTEXT, context_attribs);
	if (ec->base.context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	ec->dummy_surface = gbm_surface_create(ec->gbm, 10, 10,
					       GBM_FORMAT_XRGB8888,
					       GBM_BO_USE_RENDERING);
	if (!ec->dummy_surface) {
		fprintf(stderr, "failed to create dummy gbm surface\n");
		return -1;
	}

	ec->dummy_egl_surface =
		eglCreateWindowSurface(ec->base.display, ec->base.config,
				       ec->dummy_surface, NULL);
	if (ec->dummy_egl_surface == EGL_NO_SURFACE) {
		fprintf(stderr, "failed to create egl surface\n");
		return -1;
	}

	if (!eglMakeCurrent(ec->base.display, ec->dummy_egl_surface,
			    ec->dummy_egl_surface, ec->base.context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

	return 0;
}

static drmModeModeInfo builtin_1024x768 = {
	63500,			/* clock */
	1024, 1072, 1176, 1328, 0,
	768, 771, 775, 798, 0,
	59920,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
	0,
	"1024x768"
};


static int
drm_output_add_mode(struct drm_output *output, drmModeModeInfo *info)
{
	struct drm_mode *mode;

	mode = malloc(sizeof *mode);
	if (mode == NULL)
		return -1;

	mode->base.flags = 0;
	mode->base.width = info->hdisplay;
	mode->base.height = info->vdisplay;
	mode->base.refresh = info->vrefresh;
	mode->mode_info = *info;
	wl_list_insert(output->base.mode_list.prev, &mode->base.link);

	return 0;
}

static int
drm_subpixel_to_wayland(int drm_value)
{
	switch (drm_value) {
	default:
	case DRM_MODE_SUBPIXEL_UNKNOWN:
		return WL_OUTPUT_SUBPIXEL_UNKNOWN;
	case DRM_MODE_SUBPIXEL_NONE:
		return WL_OUTPUT_SUBPIXEL_NONE;
	case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
		return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
	case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
		return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
	case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
		return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
	case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
		return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
	}
}

static void
output_handle_scanout_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct drm_output *output =
		container_of(listener, struct drm_output,
			     scanout_buffer_destroy_listener);

	output->scanout_buffer = NULL;

	if (!output->pending_scanout_buffer)
		weston_compositor_schedule_repaint(output->base.compositor);
}

static void
output_handle_pending_scanout_buffer_destroy(struct wl_listener *listener,
					     void *data)
{
	struct drm_output *output =
		container_of(listener, struct drm_output,
			     pending_scanout_buffer_destroy_listener);

	output->pending_scanout_buffer = NULL;

	weston_compositor_schedule_repaint(output->base.compositor);
}

static void
sprite_handle_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct drm_sprite *sprite =
		container_of(listener, struct drm_sprite,
			     destroy_listener);

	sprite->surface = NULL;
}

static void
sprite_handle_pending_buffer_destroy(struct wl_listener *listener, void *data)
{
	struct drm_sprite *sprite =
		container_of(listener, struct drm_sprite,
			     pending_destroy_listener);

	sprite->pending_surface = NULL;
}

/* returns a value between 0-255 range, where higher is brighter */
static uint32_t
drm_get_backlight(struct drm_output *output)
{
	long brightness, max_brightness, norm;

	brightness = backlight_get_brightness(output->backlight);
	max_brightness = backlight_get_max_brightness(output->backlight);

	/* convert it on a scale of 0 to 255 */
	norm = (brightness * 255)/(max_brightness);

	return (uint32_t) norm;
}

/* values accepted are between 0-255 range */
static void
drm_set_backlight(struct weston_output *output_base, uint32_t value)
{
	struct drm_output *output = (struct drm_output *) output_base;
	long max_brightness, new_brightness;

	if (!output->backlight)
		return;

	if (value > 255)
		return;

	max_brightness = backlight_get_max_brightness(output->backlight);

	/* get denormalized value */
	new_brightness = (value * max_brightness) / 255;

	backlight_set_brightness(output->backlight, new_brightness);
}

static drmModePropertyPtr
drm_get_prop(int fd, drmModeConnectorPtr connector, const char *name)
{
	drmModePropertyPtr props;
	int i;

	for (i = 0; i < connector->count_props; i++) {
		props = drmModeGetProperty(fd, connector->props[i]);
		if (!props)
			continue;

		if (!strcmp(props->name, name))
			return props;

		drmModeFreeProperty(props);
	}

	return NULL;
}

static void
drm_set_dpms(struct weston_output *output_base, enum dpms_enum level)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct weston_compositor *ec = output_base->compositor;
	struct drm_compositor *c = (struct drm_compositor *) ec;
	drmModeConnectorPtr connector;
	drmModePropertyPtr prop;

	connector = drmModeGetConnector(c->drm.fd, output->connector_id);
	if (!connector)
		return;

	prop = drm_get_prop(c->drm.fd, connector, "DPMS");
	if (!prop) {
		drmModeFreeConnector(connector);
		return;
	}

	drmModeConnectorSetProperty(c->drm.fd, connector->connector_id,
				    prop->prop_id, level);
	drmModeFreeProperty(prop);
	drmModeFreeConnector(connector);
}

static int
create_output_for_connector(struct drm_compositor *ec,
			    drmModeRes *resources,
			    drmModeConnector *connector,
			    int x, int y, struct udev_device *drm_device)
{
	struct drm_output *output;
	struct drm_mode *drm_mode, *next;
	drmModeEncoder *encoder;
	int i, ret;

	encoder = drmModeGetEncoder(ec->drm.fd, connector->encoders[0]);
	if (encoder == NULL) {
		fprintf(stderr, "No encoder for connector.\n");
		return -1;
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1 << i) &&
		    !(ec->crtc_allocator & (1 << resources->crtcs[i])))
			break;
	}
	if (i == resources->count_crtcs) {
		fprintf(stderr, "No usable crtc for encoder.\n");
		drmModeFreeEncoder(encoder);
		return -1;
	}

	output = malloc(sizeof *output);
	if (output == NULL) {
		drmModeFreeEncoder(encoder);
		return -1;
	}

	memset(output, 0, sizeof *output);
	output->base.subpixel = drm_subpixel_to_wayland(connector->subpixel);
	output->base.make = "unknown";
	output->base.model = "unknown";
	wl_list_init(&output->base.mode_list);

	output->crtc_id = resources->crtcs[i];
	ec->crtc_allocator |= (1 << output->crtc_id);
	output->connector_id = connector->connector_id;
	ec->connector_allocator |= (1 << output->connector_id);

	output->original_crtc = drmModeGetCrtc(ec->drm.fd, output->crtc_id);
	drmModeFreeEncoder(encoder);

	for (i = 0; i < connector->count_modes; i++) {
		ret = drm_output_add_mode(output, &connector->modes[i]);
		if (ret)
			goto err_free;
	}

	if (connector->count_modes == 0) {
		ret = drm_output_add_mode(output, &builtin_1024x768);
		if (ret)
			goto err_free;
	}

	drm_mode = container_of(output->base.mode_list.next,
				struct drm_mode, base.link);
	output->base.current = &drm_mode->base;
	drm_mode->base.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	output->surface = gbm_surface_create(ec->gbm,
					     output->base.current->width,
					     output->base.current->height,
					     GBM_FORMAT_XRGB8888,
					     GBM_BO_USE_SCANOUT |
					     GBM_BO_USE_RENDERING);
	if (!output->surface) {
		fprintf(stderr, "failed to create gbm surface\n");
		goto err_free;
	}

	output->egl_surface =
		eglCreateWindowSurface(ec->base.display, ec->base.config,
				       output->surface, NULL);
	if (output->egl_surface == EGL_NO_SURFACE) {
		fprintf(stderr, "failed to create egl surface\n");
		goto err_surface;
	}

	output->backlight = backlight_init(drm_device,
					   connector->connector_type);
	if (output->backlight) {
		output->base.set_backlight = drm_set_backlight;
		output->base.backlight_current = drm_get_backlight(output);
	}

	weston_output_init(&output->base, &ec->base, x, y,
			   connector->mmWidth, connector->mmHeight,
			   WL_OUTPUT_FLIPPED);

	wl_list_insert(ec->base.output_list.prev, &output->base.link);

	output->scanout_buffer_destroy_listener.notify =
		output_handle_scanout_buffer_destroy;
	output->pending_scanout_buffer_destroy_listener.notify =
		output_handle_pending_scanout_buffer_destroy;

	output->next_fb_id = 0;
	output->base.origin = output->base.current;
	output->base.repaint = drm_output_repaint;
	output->base.destroy = drm_output_destroy;
	output->base.assign_planes = drm_assign_planes;
	output->base.set_dpms = drm_set_dpms;
	output->base.switch_mode = drm_output_switch_mode;

	return 0;

err_surface:
	gbm_surface_destroy(output->surface);
err_free:
	wl_list_for_each_safe(drm_mode, next, &output->base.mode_list,
							base.link) {
		wl_list_remove(&drm_mode->base.link);
		free(drm_mode);
	}

	drmModeFreeCrtc(output->original_crtc);
	ec->crtc_allocator &= ~(1 << output->crtc_id);
	ec->connector_allocator &= ~(1 << output->connector_id);
	free(output);

	return -1;
}

static void
create_sprites(struct drm_compositor *ec)
{
	struct drm_sprite *sprite;
	drmModePlaneRes *plane_res;
	drmModePlane *plane;
	uint32_t i;

	plane_res = drmModeGetPlaneResources(ec->drm.fd);
	if (!plane_res) {
		fprintf(stderr, "failed to get plane resources: %s\n",
			strerror(errno));
		return;
	}

	for (i = 0; i < plane_res->count_planes; i++) {
		plane = drmModeGetPlane(ec->drm.fd, plane_res->planes[i]);
		if (!plane)
			continue;

		sprite = malloc(sizeof(*sprite) + ((sizeof(uint32_t)) *
						   plane->count_formats));
		if (!sprite) {
			fprintf(stderr, "%s: out of memory\n",
				__func__);
			free(plane);
			continue;
		}

		memset(sprite, 0, sizeof *sprite);

		sprite->possible_crtcs = plane->possible_crtcs;
		sprite->plane_id = plane->plane_id;
		sprite->surface = NULL;
		sprite->pending_surface = NULL;
		sprite->fb_id = 0;
		sprite->pending_fb_id = 0;
		sprite->destroy_listener.notify = sprite_handle_buffer_destroy;
		sprite->pending_destroy_listener.notify =
			sprite_handle_pending_buffer_destroy;
		sprite->compositor = ec;
		sprite->count_formats = plane->count_formats;
		memcpy(sprite->formats, plane->formats,
		       plane->count_formats * sizeof(plane->formats[0]));
		drmModeFreePlane(plane);

		wl_list_insert(&ec->sprite_list, &sprite->link);
	}

	free(plane_res->planes);
	free(plane_res);
}

static void
destroy_sprites(struct drm_compositor *compositor)
{
	struct drm_sprite *sprite, *next;
	struct drm_output *output;

	output = container_of(compositor->base.output_list.next,
			      struct drm_output, base.link);

	wl_list_for_each_safe(sprite, next, &compositor->sprite_list, link) {
		drmModeSetPlane(compositor->drm.fd,
				sprite->plane_id,
				output->crtc_id, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0);
		drmModeRmFB(compositor->drm.fd, sprite->fb_id);
		free(sprite);
	}
}

static int
create_outputs(struct drm_compositor *ec, uint32_t option_connector,
	       struct udev_device *drm_device)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	int i;
	int x = 0, y = 0;

	resources = drmModeGetResources(ec->drm.fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return -1;
	}

	ec->crtcs = calloc(resources->count_crtcs, sizeof(uint32_t));
	if (!ec->crtcs) {
		drmModeFreeResources(resources);
		return -1;
	}

	ec->num_crtcs = resources->count_crtcs;
	memcpy(ec->crtcs, resources->crtcs, sizeof(uint32_t) * ec->num_crtcs);

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(ec->drm.fd,
						resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    (option_connector == 0 ||
		     connector->connector_id == option_connector)) {
			if (create_output_for_connector(ec, resources,
							connector, x, y,
							drm_device) < 0) {
				drmModeFreeConnector(connector);
				continue;
			}

			x += container_of(ec->base.output_list.prev,
					  struct weston_output,
					  link)->current->width;
		}

		drmModeFreeConnector(connector);
	}

	if (wl_list_empty(&ec->base.output_list)) {
		fprintf(stderr, "No currently active connector found.\n");
		drmModeFreeResources(resources);
		return -1;
	}

	drmModeFreeResources(resources);

	return 0;
}

static void
update_outputs(struct drm_compositor *ec, struct udev_device *drm_device)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	struct drm_output *output, *next;
	int x = 0, y = 0;
	int x_offset = 0, y_offset = 0;
	uint32_t connected = 0, disconnects = 0;
	int i;

	resources = drmModeGetResources(ec->drm.fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return;
	}

	/* collect new connects */
	for (i = 0; i < resources->count_connectors; i++) {
		int connector_id = resources->connectors[i];

		connector = drmModeGetConnector(ec->drm.fd, connector_id);
		if (connector == NULL)
			continue;

		if (connector->connection != DRM_MODE_CONNECTED) {
			drmModeFreeConnector(connector);
			continue;
		}

		connected |= (1 << connector_id);

		if (!(ec->connector_allocator & (1 << connector_id))) {
			struct weston_output *last =
				container_of(ec->base.output_list.prev,
					     struct weston_output, link);

			/* XXX: not yet needed, we die with 0 outputs */
			if (!wl_list_empty(&ec->base.output_list))
				x = last->x + last->current->width;
			else
				x = 0;
			y = 0;
			create_output_for_connector(ec, resources,
						    connector, x, y,
						    drm_device);
			printf("connector %d connected\n", connector_id);

		}
		drmModeFreeConnector(connector);
	}
	drmModeFreeResources(resources);

	disconnects = ec->connector_allocator & ~connected;
	if (disconnects) {
		wl_list_for_each_safe(output, next, &ec->base.output_list,
				      base.link) {
			if (x_offset != 0 || y_offset != 0) {
				weston_output_move(&output->base,
						 output->base.x - x_offset,
						 output->base.y - y_offset);
			}

			if (disconnects & (1 << output->connector_id)) {
				disconnects &= ~(1 << output->connector_id);
				printf("connector %d disconnected\n",
				       output->connector_id);
				x_offset += output->base.current->width;
				drm_output_destroy(&output->base);
			}
		}
	}

	/* FIXME: handle zero outputs, without terminating */	
	if (ec->connector_allocator == 0)
		wl_display_terminate(ec->base.wl_display);
}

static int
udev_event_is_hotplug(struct drm_compositor *ec, struct udev_device *device)
{
	const char *sysnum;
	const char *val;

	sysnum = udev_device_get_sysnum(device);
	if (!sysnum || atoi(sysnum) != ec->drm.id)
		return 0;

	val = udev_device_get_property_value(device, "HOTPLUG");
	if (!val)
		return 0;

	return strcmp(val, "1") == 0;
}

static int
udev_drm_event(int fd, uint32_t mask, void *data)
{
	struct drm_compositor *ec = data;
	struct udev_device *event;

	event = udev_monitor_receive_device(ec->udev_monitor);

	if (udev_event_is_hotplug(ec, event))
		update_outputs(ec, event);

	udev_device_unref(event);

	return 1;
}

static void
drm_destroy(struct weston_compositor *ec)
{
	struct drm_compositor *d = (struct drm_compositor *) ec;
	struct weston_input_device *input, *next;

	wl_list_for_each_safe(input, next, &ec->input_device_list, link)
		evdev_input_destroy(input);

	wl_event_source_remove(d->udev_drm_source);
	wl_event_source_remove(d->drm_source);

	weston_compositor_shutdown(ec);

	gbm_device_destroy(d->gbm);
	destroy_sprites(d);
	if (weston_launcher_drm_set_master(&d->base, d->drm.fd, 0) < 0)
		fprintf(stderr, "failed to drop master: %m\n");
	tty_destroy(d->tty);

	free(d);
}

static void
drm_compositor_set_modes(struct drm_compositor *compositor)
{
	struct drm_output *output;
	struct drm_mode *drm_mode;
	int ret;

	wl_list_for_each(output, &compositor->base.output_list, base.link) {
		drm_mode = (struct drm_mode *) output->base.current;
		ret = drmModeSetCrtc(compositor->drm.fd, output->crtc_id,
				     output->current_fb_id, 0, 0,
				     &output->connector_id, 1,
				     &drm_mode->mode_info);
		if (ret < 0) {
			fprintf(stderr,
				"failed to set mode %dx%d for output at %d,%d: %m\n",
				drm_mode->base.width, drm_mode->base.height, 
				output->base.x, output->base.y);
		}
	}
}

static void
vt_func(struct weston_compositor *compositor, int event)
{
	struct drm_compositor *ec = (struct drm_compositor *) compositor;
	struct weston_output *output;
	struct weston_input_device *input;
	struct drm_sprite *sprite;
	struct drm_output *drm_output;

	switch (event) {
	case TTY_ENTER_VT:
		compositor->focus = 1;
		if (weston_launcher_drm_set_master(&ec->base, ec->drm.fd, 1)) {
			fprintf(stderr, "failed to set master: %m\n");
			wl_display_terminate(compositor->wl_display);
		}
		compositor->state = ec->prev_state;
		drm_compositor_set_modes(ec);
		weston_compositor_damage_all(compositor);
		wl_list_for_each(input, &compositor->input_device_list, link) {
			evdev_add_devices(ec->udev, input);
			evdev_enable_udev_monitor(ec->udev, input);
		}
		break;
	case TTY_LEAVE_VT:
		wl_list_for_each(input, &compositor->input_device_list, link) {
			evdev_disable_udev_monitor(input);
			evdev_remove_devices(input);
		}

		compositor->focus = 0;
		ec->prev_state = compositor->state;
		compositor->state = WESTON_COMPOSITOR_SLEEPING;

		/* If we have a repaint scheduled (either from a
		 * pending pageflip or the idle handler), make sure we
		 * cancel that so we don't try to pageflip when we're
		 * vt switched away.  The SLEEPING state will prevent
		 * further attemps at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output, &ec->base.output_list, link) {
			output->repaint_needed = 0;
			drm_output_set_cursor(output, NULL);
		}

		drm_output = container_of(ec->base.output_list.next,
					  struct drm_output, base.link);

		wl_list_for_each(sprite, &ec->sprite_list, link)
			drmModeSetPlane(ec->drm.fd,
					sprite->plane_id,
					drm_output->crtc_id, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0);

		if (weston_launcher_drm_set_master(&ec->base, ec->drm.fd, 0) < 0)
			fprintf(stderr, "failed to drop master: %m\n");

		break;
	};
}

static void
switch_vt_binding(struct wl_input_device *device, uint32_t time,
		  uint32_t key, uint32_t button, uint32_t axis, int32_t state, void *data)
{
	struct drm_compositor *ec = data;

	if (state)
		tty_activate_vt(ec->tty, key - KEY_F1 + 1);
}

static const char default_seat[] = "seat0";

static struct weston_compositor *
drm_compositor_create(struct wl_display *display,
		      int connector, const char *seat, int tty)
{
	struct drm_compositor *ec;
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device, *drm_device;
	const char *path, *device_seat;
	struct wl_event_loop *loop;
	uint32_t key;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	memset(ec, 0, sizeof *ec);
	ec->udev = udev_new();
	if (ec->udev == NULL) {
		fprintf(stderr, "failed to initialize udev context\n");
		return NULL;
	}

	ec->base.wl_display = display;
	ec->tty = tty_create(&ec->base, vt_func, tty);
	if (!ec->tty) {
		fprintf(stderr, "failed to initialize tty\n");
		free(ec);
		return NULL;
	}

	e = udev_enumerate_new(ec->udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_sysname(e, "card[0-9]*");

	udev_enumerate_scan_devices(e);
	drm_device = NULL;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(ec->udev, path);
		device_seat =
			udev_device_get_property_value(device, "ID_SEAT");
		if (!device_seat)
			device_seat = default_seat;
		if (strcmp(device_seat, seat) == 0) {
			drm_device = device;
			break;
		}
		udev_device_unref(device);
	}

	if (drm_device == NULL) {
		fprintf(stderr, "no drm device found\n");
		return NULL;
	}

	if (init_egl(ec, drm_device) < 0) {
		fprintf(stderr, "failed to initialize egl\n");
		return NULL;
	}

	ec->base.destroy = drm_destroy;

	ec->base.focus = 1;

	ec->prev_state = WESTON_COMPOSITOR_ACTIVE;

	/* Can't init base class until we have a current egl context */
	if (weston_compositor_init(&ec->base, display) < 0)
		return NULL;

	for (key = KEY_F1; key < KEY_F9; key++)
		weston_compositor_add_binding(&ec->base, key, 0, 0,
					      MODIFIER_CTRL | MODIFIER_ALT,
					      switch_vt_binding, ec);

	wl_list_init(&ec->sprite_list);
	create_sprites(ec);

	if (create_outputs(ec, connector, drm_device) < 0) {
		fprintf(stderr, "failed to create output for %s\n", path);
		return NULL;
	}

	udev_device_unref(drm_device);
	udev_enumerate_unref(e);
	path = NULL;

	evdev_input_create(&ec->base, ec->udev, seat);

	loop = wl_display_get_event_loop(ec->base.wl_display);
	ec->drm_source =
		wl_event_loop_add_fd(loop, ec->drm.fd,
				     WL_EVENT_READABLE, on_drm_input, ec);

	ec->udev_monitor = udev_monitor_new_from_netlink(ec->udev, "udev");
	if (ec->udev_monitor == NULL) {
		fprintf(stderr, "failed to intialize udev monitor\n");
		return NULL;
	}
	udev_monitor_filter_add_match_subsystem_devtype(ec->udev_monitor,
							"drm", NULL);
	ec->udev_drm_source =
		wl_event_loop_add_fd(loop,
				     udev_monitor_get_fd(ec->udev_monitor),
				     WL_EVENT_READABLE, udev_drm_event, ec);

	if (udev_monitor_enable_receiving(ec->udev_monitor) < 0) {
		fprintf(stderr, "failed to enable udev-monitor receiving\n");
		return NULL;
	}

	return &ec->base;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[])
{
	int connector = 0, tty = 0;
	const char *seat = default_seat;

	const struct weston_option drm_options[] = {
		{ WESTON_OPTION_INTEGER, "connector", 0, &connector },
		{ WESTON_OPTION_STRING, "seat", 0, &seat },
		{ WESTON_OPTION_INTEGER, "tty", 0, &tty },
	};

	parse_options(drm_options, ARRAY_LENGTH(drm_options), argc, argv);

	return drm_compositor_create(display, connector, seat, tty);
}

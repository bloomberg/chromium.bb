/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <assert.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <time.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <gbm.h>
#include <libudev.h>

#include "shared/helpers.h"
#include "shared/timespec-util.h"
#include "libbacklight.h"
#include "compositor.h"
#include "gl-renderer.h"
#include "pixman-renderer.h"
#include "libinput-seat.h"
#include "launcher-util.h"
#include "vaapi-recorder.h"
#include "presentation_timing-server-protocol.h"
#include "linux-dmabuf.h"

#ifndef DRM_CAP_TIMESTAMP_MONOTONIC
#define DRM_CAP_TIMESTAMP_MONOTONIC 0x6
#endif

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

#ifndef GBM_BO_USE_CURSOR
#define GBM_BO_USE_CURSOR GBM_BO_USE_CURSOR_64X64
#endif

static int option_current_mode = 0;

enum output_config {
	OUTPUT_CONFIG_INVALID = 0,
	OUTPUT_CONFIG_OFF,
	OUTPUT_CONFIG_PREFERRED,
	OUTPUT_CONFIG_CURRENT,
	OUTPUT_CONFIG_MODE,
	OUTPUT_CONFIG_MODELINE
};

struct drm_backend {
	struct weston_backend base;
	struct weston_compositor *compositor;

	struct udev *udev;
	struct wl_event_source *drm_source;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_drm_source;

	struct {
		int id;
		int fd;
		char *filename;
	} drm;
	struct gbm_device *gbm;
	uint32_t *crtcs;
	int num_crtcs;
	uint32_t crtc_allocator;
	uint32_t connector_allocator;
	struct wl_listener session_listener;
	uint32_t format;

	/* we need these parameters in order to not fail drmModeAddFB2()
	 * due to out of bounds dimensions, and then mistakenly set
	 * sprites_are_broken:
	 */
	uint32_t min_width, max_width;
	uint32_t min_height, max_height;
	int no_addfb2;

	struct wl_list sprite_list;
	int sprites_are_broken;
	int sprites_hidden;

	int cursors_are_broken;

	int use_pixman;

	uint32_t prev_state;

	struct udev_input input;

	int32_t cursor_width;
	int32_t cursor_height;
};

struct drm_mode {
	struct weston_mode base;
	drmModeModeInfo mode_info;
};

struct drm_output;

struct drm_fb {
	struct drm_output *output;
	uint32_t fb_id, stride, handle, size;
	int fd;
	int is_client_buffer;
	struct weston_buffer_reference buffer_ref;

	/* Used by gbm fbs */
	struct gbm_bo *bo;

	/* Used by dumb fbs */
	void *map;
};

struct drm_edid {
	char eisa_id[13];
	char monitor_name[13];
	char pnp_id[5];
	char serial_number[13];
};

struct drm_output {
	struct weston_output   base;

	uint32_t crtc_id;
	int pipe;
	uint32_t connector_id;
	drmModeCrtcPtr original_crtc;
	struct drm_edid edid;
	drmModePropertyPtr dpms_prop;
	uint32_t format;

	enum dpms_enum dpms;

	int vblank_pending;
	int page_flip_pending;
	int destroy_pending;

	struct gbm_surface *surface;
	struct gbm_bo *cursor_bo[2];
	struct weston_plane cursor_plane;
	struct weston_plane fb_plane;
	struct weston_view *cursor_view;
	int current_cursor;
	struct drm_fb *current, *next;
	struct backlight *backlight;

	struct drm_fb *dumb[2];
	pixman_image_t *image[2];
	int current_image;
	pixman_region32_t previous_damage;

	struct vaapi_recorder *recorder;
	struct wl_listener recorder_frame_listener;
};

/*
 * An output has a primary display plane plus zero or more sprites for
 * blending display contents.
 */
struct drm_sprite {
	struct wl_list link;

	struct weston_plane plane;

	struct drm_fb *current, *next;
	struct drm_output *output;
	struct drm_backend *backend;

	uint32_t possible_crtcs;
	uint32_t plane_id;
	uint32_t count_formats;

	int32_t src_x, src_y;
	uint32_t src_w, src_h;
	uint32_t dest_x, dest_y;
	uint32_t dest_w, dest_h;

	uint32_t formats[];
};

struct drm_parameters {
	int connector;
	int tty;
	int use_pixman;
	const char *seat_id;
};

static struct gl_renderer_interface *gl_renderer;

static const char default_seat[] = "seat0";

static void
drm_output_set_cursor(struct drm_output *output);

static void
drm_output_update_msc(struct drm_output *output, unsigned int seq);

static int
drm_sprite_crtc_supported(struct drm_output *output, uint32_t supported)
{
	struct weston_compositor *ec = output->base.compositor;
	struct drm_backend *b =(struct drm_backend *)ec->backend;
	int crtc;

	for (crtc = 0; crtc < b->num_crtcs; crtc++) {
		if (b->crtcs[crtc] != output->crtc_id)
			continue;

		if (supported & (1 << crtc))
			return -1;
	}

	return 0;
}

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = data;
	struct gbm_device *gbm = gbm_bo_get_device(bo);

	if (fb->fb_id)
		drmModeRmFB(gbm_device_get_fd(gbm), fb->fb_id);

	weston_buffer_reference(&fb->buffer_ref, NULL);

	free(data);
}

static struct drm_fb *
drm_fb_create_dumb(struct drm_backend *b, unsigned width, unsigned height)
{
	struct drm_fb *fb;
	int ret;

	struct drm_mode_create_dumb create_arg;
	struct drm_mode_destroy_dumb destroy_arg;
	struct drm_mode_map_dumb map_arg;

	fb = zalloc(sizeof *fb);
	if (!fb)
		return NULL;

	memset(&create_arg, 0, sizeof create_arg);
	create_arg.bpp = 32;
	create_arg.width = width;
	create_arg.height = height;

	ret = drmIoctl(b->drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
	if (ret)
		goto err_fb;

	fb->handle = create_arg.handle;
	fb->stride = create_arg.pitch;
	fb->size = create_arg.size;
	fb->fd = b->drm.fd;

	ret = drmModeAddFB(b->drm.fd, width, height, 24, 32,
			   fb->stride, fb->handle, &fb->fb_id);
	if (ret)
		goto err_bo;

	memset(&map_arg, 0, sizeof map_arg);
	map_arg.handle = fb->handle;
	ret = drmIoctl(fb->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg);
	if (ret)
		goto err_add_fb;

	fb->map = mmap(0, fb->size, PROT_WRITE,
		       MAP_SHARED, b->drm.fd, map_arg.offset);
	if (fb->map == MAP_FAILED)
		goto err_add_fb;

	return fb;

err_add_fb:
	drmModeRmFB(b->drm.fd, fb->fb_id);
err_bo:
	memset(&destroy_arg, 0, sizeof(destroy_arg));
	destroy_arg.handle = create_arg.handle;
	drmIoctl(b->drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
err_fb:
	free(fb);
	return NULL;
}

static void
drm_fb_destroy_dumb(struct drm_fb *fb)
{
	struct drm_mode_destroy_dumb destroy_arg;

	if (!fb->map)
		return;

	if (fb->fb_id)
		drmModeRmFB(fb->fd, fb->fb_id);

	weston_buffer_reference(&fb->buffer_ref, NULL);

	munmap(fb->map, fb->size);

	memset(&destroy_arg, 0, sizeof(destroy_arg));
	destroy_arg.handle = fb->handle;
	drmIoctl(fb->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);

	free(fb);
}

static struct drm_fb *
drm_fb_get_from_bo(struct gbm_bo *bo,
		   struct drm_backend *backend, uint32_t format)
{
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	uint32_t width, height;
	uint32_t handles[4], pitches[4], offsets[4];
	int ret;

	if (fb)
		return fb;

	fb = zalloc(sizeof *fb);
	if (fb == NULL)
		return NULL;

	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	fb->stride = gbm_bo_get_stride(bo);
	fb->handle = gbm_bo_get_handle(bo).u32;
	fb->size = fb->stride * height;
	fb->fd = backend->drm.fd;

	if (backend->min_width > width || width > backend->max_width ||
	    backend->min_height > height ||
	    height > backend->max_height) {
		weston_log("bo geometry out of bounds\n");
		goto err_free;
	}

	ret = -1;

	if (format && !backend->no_addfb2) {
		handles[0] = fb->handle;
		pitches[0] = fb->stride;
		offsets[0] = 0;

		ret = drmModeAddFB2(backend->drm.fd, width, height,
				    format, handles, pitches, offsets,
				    &fb->fb_id, 0);
		if (ret) {
			weston_log("addfb2 failed: %m\n");
			backend->no_addfb2 = 1;
			backend->sprites_are_broken = 1;
		}
	}

	if (ret)
		ret = drmModeAddFB(backend->drm.fd, width, height, 24, 32,
				   fb->stride, fb->handle, &fb->fb_id);

	if (ret) {
		weston_log("failed to create kms fb: %m\n");
		goto err_free;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;

err_free:
	free(fb);
	return NULL;
}

static void
drm_fb_set_buffer(struct drm_fb *fb, struct weston_buffer *buffer)
{
	assert(fb->buffer_ref.buffer == NULL);

	fb->is_client_buffer = 1;

	weston_buffer_reference(&fb->buffer_ref, buffer);
}

static void
drm_output_release_fb(struct drm_output *output, struct drm_fb *fb)
{
	if (!fb)
		return;

	if (fb->map &&
            (fb != output->dumb[0] && fb != output->dumb[1])) {
		drm_fb_destroy_dumb(fb);
	} else if (fb->bo) {
		if (fb->is_client_buffer)
			gbm_bo_destroy(fb->bo);
		else
			gbm_surface_release_buffer(output->surface,
						   fb->bo);
	}
}

static uint32_t
drm_output_check_scanout_format(struct drm_output *output,
				struct weston_surface *es, struct gbm_bo *bo)
{
	uint32_t format;
	pixman_region32_t r;

	format = gbm_bo_get_format(bo);

	if (format == GBM_FORMAT_ARGB8888) {
		/* We can scanout an ARGB buffer if the surface's
		 * opaque region covers the whole output, but we have
		 * to use XRGB as the KMS format code. */
		pixman_region32_init_rect(&r, 0, 0,
					  output->base.width,
					  output->base.height);
		pixman_region32_subtract(&r, &r, &es->opaque);

		if (!pixman_region32_not_empty(&r))
			format = GBM_FORMAT_XRGB8888;

		pixman_region32_fini(&r);
	}

	if (output->format == format)
		return format;

	return 0;
}

static struct weston_plane *
drm_output_prepare_scanout_view(struct drm_output *output,
				struct weston_view *ev)
{
	struct drm_backend *b =
		(struct drm_backend *)output->base.compositor->backend;
	struct weston_buffer *buffer = ev->surface->buffer_ref.buffer;
	struct weston_buffer_viewport *viewport = &ev->surface->buffer_viewport;
	struct gbm_bo *bo;
	uint32_t format;

	if (ev->geometry.x != output->base.x ||
	    ev->geometry.y != output->base.y ||
	    buffer == NULL || b->gbm == NULL ||
	    buffer->width != output->base.current_mode->width ||
	    buffer->height != output->base.current_mode->height ||
	    output->base.transform != viewport->buffer.transform ||
	    ev->transform.enabled)
		return NULL;

	if (ev->geometry.scissor_enabled)
		return NULL;

	bo = gbm_bo_import(b->gbm, GBM_BO_IMPORT_WL_BUFFER,
			   buffer->resource, GBM_BO_USE_SCANOUT);

	/* Unable to use the buffer for scanout */
	if (!bo)
		return NULL;

	format = drm_output_check_scanout_format(output, ev->surface, bo);
	if (format == 0) {
		gbm_bo_destroy(bo);
		return NULL;
	}

	output->next = drm_fb_get_from_bo(bo, b, format);
	if (!output->next) {
		gbm_bo_destroy(bo);
		return NULL;
	}

	drm_fb_set_buffer(output->next, buffer);

	return &output->fb_plane;
}

static void
drm_output_render_gl(struct drm_output *output, pixman_region32_t *damage)
{
	struct drm_backend *b =
		(struct drm_backend *)output->base.compositor->backend;
	struct gbm_bo *bo;

	output->base.compositor->renderer->repaint_output(&output->base,
							  damage);

	bo = gbm_surface_lock_front_buffer(output->surface);
	if (!bo) {
		weston_log("failed to lock front buffer: %m\n");
		return;
	}

	output->next = drm_fb_get_from_bo(bo, b, output->format);
	if (!output->next) {
		weston_log("failed to get drm_fb for bo\n");
		gbm_surface_release_buffer(output->surface, bo);
		return;
	}
}

static void
drm_output_render_pixman(struct drm_output *output, pixman_region32_t *damage)
{
	struct weston_compositor *ec = output->base.compositor;
	pixman_region32_t total_damage, previous_damage;

	pixman_region32_init(&total_damage);
	pixman_region32_init(&previous_damage);

	pixman_region32_copy(&previous_damage, damage);

	pixman_region32_union(&total_damage, damage, &output->previous_damage);
	pixman_region32_copy(&output->previous_damage, &previous_damage);

	output->current_image ^= 1;

	output->next = output->dumb[output->current_image];
	pixman_renderer_output_set_buffer(&output->base,
					  output->image[output->current_image]);

	ec->renderer->repaint_output(&output->base, &total_damage);

	pixman_region32_fini(&total_damage);
	pixman_region32_fini(&previous_damage);
}

static void
drm_output_render(struct drm_output *output, pixman_region32_t *damage)
{
	struct weston_compositor *c = output->base.compositor;
	struct drm_backend *b = (struct drm_backend *)c->backend;

	if (b->use_pixman)
		drm_output_render_pixman(output, damage);
	else
		drm_output_render_gl(output, damage);

	pixman_region32_subtract(&c->primary_plane.damage,
				 &c->primary_plane.damage, damage);
}

static void
drm_output_set_gamma(struct weston_output *output_base,
		     uint16_t size, uint16_t *r, uint16_t *g, uint16_t *b)
{
	int rc;
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_backend *backend =
		(struct drm_backend *) output->base.compositor->backend;

	/* check */
	if (output_base->gamma_size != size)
		return;
	if (!output->original_crtc)
		return;

	rc = drmModeCrtcSetGamma(backend->drm.fd,
				 output->crtc_id,
				 size, r, g, b);
	if (rc)
		weston_log("set gamma failed: %m\n");
}

/* Determine the type of vblank synchronization to use for the output.
 * 
 * The pipe parameter indicates which CRTC is in use.  Knowing this, we
 * can determine which vblank sequence type to use for it.  Traditional
 * cards had only two CRTCs, with CRTC 0 using no special flags, and
 * CRTC 1 using DRM_VBLANK_SECONDARY.  The first bit of the pipe
 * parameter indicates this.
 * 
 * Bits 1-5 of the pipe parameter are 5 bit wide pipe number between
 * 0-31.  If this is non-zero it indicates we're dealing with a
 * multi-gpu situation and we need to calculate the vblank sync
 * using DRM_BLANK_HIGH_CRTC_MASK.
 */
static unsigned int
drm_waitvblank_pipe(struct drm_output *output)
{
	if (output->pipe > 1)
		return (output->pipe << DRM_VBLANK_HIGH_CRTC_SHIFT) &
				DRM_VBLANK_HIGH_CRTC_MASK;
	else if (output->pipe > 0)
		return DRM_VBLANK_SECONDARY;
	else
		return 0;
}

static int
drm_output_repaint(struct weston_output *output_base,
		   pixman_region32_t *damage)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_backend *backend =
		(struct drm_backend *)output->base.compositor->backend;
	struct drm_sprite *s;
	struct drm_mode *mode;
	int ret = 0;

	if (output->destroy_pending)
		return -1;

	if (!output->next)
		drm_output_render(output, damage);
	if (!output->next)
		return -1;

	mode = container_of(output->base.current_mode, struct drm_mode, base);
	if (!output->current ||
	    output->current->stride != output->next->stride) {
		ret = drmModeSetCrtc(backend->drm.fd, output->crtc_id,
				     output->next->fb_id, 0, 0,
				     &output->connector_id, 1,
				     &mode->mode_info);
		if (ret) {
			weston_log("set mode failed: %m\n");
			goto err_pageflip;
		}
		output_base->set_dpms(output_base, WESTON_DPMS_ON);
	}

	if (drmModePageFlip(backend->drm.fd, output->crtc_id,
			    output->next->fb_id,
			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
		weston_log("queueing pageflip failed: %m\n");
		goto err_pageflip;
	}

	output->page_flip_pending = 1;

	drm_output_set_cursor(output);

	/*
	 * Now, update all the sprite surfaces
	 */
	wl_list_for_each(s, &backend->sprite_list, link) {
		uint32_t flags = 0, fb_id = 0;
		drmVBlank vbl = {
			.request.type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT,
			.request.sequence = 1,
		};

		if ((!s->current && !s->next) ||
		    !drm_sprite_crtc_supported(output, s->possible_crtcs))
			continue;

		if (s->next && !backend->sprites_hidden)
			fb_id = s->next->fb_id;

		ret = drmModeSetPlane(backend->drm.fd, s->plane_id,
				      output->crtc_id, fb_id, flags,
				      s->dest_x, s->dest_y,
				      s->dest_w, s->dest_h,
				      s->src_x, s->src_y,
				      s->src_w, s->src_h);
		if (ret)
			weston_log("setplane failed: %d: %s\n",
				ret, strerror(errno));

		vbl.request.type |= drm_waitvblank_pipe(output);

		/*
		 * Queue a vblank signal so we know when the surface
		 * becomes active on the display or has been replaced.
		 */
		vbl.request.signal = (unsigned long)s;
		ret = drmWaitVBlank(backend->drm.fd, &vbl);
		if (ret) {
			weston_log("vblank event request failed: %d: %s\n",
				ret, strerror(errno));
		}

		s->output = output;
		output->vblank_pending = 1;
	}

	return 0;

err_pageflip:
	output->cursor_view = NULL;
	if (output->next) {
		drm_output_release_fb(output, output->next);
		output->next = NULL;
	}

	return -1;
}

static void
drm_output_start_repaint_loop(struct weston_output *output_base)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_backend *backend = (struct drm_backend *)
		output_base->compositor->backend;
	uint32_t fb_id;
	struct timespec ts, tnow;
	struct timespec vbl2now;
	int64_t refresh_nsec;
	int ret;
	drmVBlank vbl = {
		.request.type = DRM_VBLANK_RELATIVE,
		.request.sequence = 0,
		.request.signal = 0,
	};

	if (output->destroy_pending)
		return;

	if (!output->current) {
		/* We can't page flip if there's no mode set */
		goto finish_frame;
	}

	/* Try to get current msc and timestamp via instant query */
	vbl.request.type |= drm_waitvblank_pipe(output);
	ret = drmWaitVBlank(backend->drm.fd, &vbl);

	/* Error ret or zero timestamp means failure to get valid timestamp */
	if ((ret == 0) && (vbl.reply.tval_sec > 0 || vbl.reply.tval_usec > 0)) {
		ts.tv_sec = vbl.reply.tval_sec;
		ts.tv_nsec = vbl.reply.tval_usec * 1000;

		/* Valid timestamp for most recent vblank - not stale?
		 * Stale ts could happen on Linux 3.17+, so make sure it
		 * is not older than 1 refresh duration since now.
		 */
		weston_compositor_read_presentation_clock(backend->compositor,
							  &tnow);
		timespec_sub(&vbl2now, &tnow, &ts);
		refresh_nsec =
			millihz_to_nsec(output->base.current_mode->refresh);
		if (timespec_to_nsec(&vbl2now) < refresh_nsec) {
			drm_output_update_msc(output, vbl.reply.sequence);
			weston_output_finish_frame(output_base, &ts,
						PRESENTATION_FEEDBACK_INVALID);
			return;
		}
	}

	/* Immediate query didn't provide valid timestamp.
	 * Use pageflip fallback.
	 */
	fb_id = output->current->fb_id;

	if (drmModePageFlip(backend->drm.fd, output->crtc_id, fb_id,
			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
		weston_log("queueing pageflip failed: %m\n");
		goto finish_frame;
	}

	return;

finish_frame:
	/* if we cannot page-flip, immediately finish frame */
	weston_compositor_read_presentation_clock(output_base->compositor, &ts);
	weston_output_finish_frame(output_base, &ts,
				   PRESENTATION_FEEDBACK_INVALID);
}

static void
drm_output_update_msc(struct drm_output *output, unsigned int seq)
{
	uint64_t msc_hi = output->base.msc >> 32;

	if (seq < (output->base.msc & 0xffffffff))
		msc_hi++;

	output->base.msc = (msc_hi << 32) + seq;
}

static void
vblank_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec,
	       void *data)
{
	struct drm_sprite *s = (struct drm_sprite *)data;
	struct drm_output *output = s->output;
	struct timespec ts;
	uint32_t flags = PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	drm_output_update_msc(output, frame);
	output->vblank_pending = 0;

	drm_output_release_fb(output, s->current);
	s->current = s->next;
	s->next = NULL;

	if (!output->page_flip_pending) {
		ts.tv_sec = sec;
		ts.tv_nsec = usec * 1000;
		weston_output_finish_frame(&output->base, &ts, flags);
	}
}

static void
drm_output_destroy(struct weston_output *output_base);

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct drm_output *output = (struct drm_output *) data;
	struct timespec ts;
	uint32_t flags = PRESENTATION_FEEDBACK_KIND_VSYNC |
			 PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	drm_output_update_msc(output, frame);

	/* We don't set page_flip_pending on start_repaint_loop, in that case
	 * we just want to page flip to the current buffer to get an accurate
	 * timestamp */
	if (output->page_flip_pending) {
		drm_output_release_fb(output, output->current);
		output->current = output->next;
		output->next = NULL;
	}

	output->page_flip_pending = 0;

	if (output->destroy_pending)
		drm_output_destroy(&output->base);
	else if (!output->vblank_pending) {
		ts.tv_sec = sec;
		ts.tv_nsec = usec * 1000;
		weston_output_finish_frame(&output->base, &ts, flags);

		/* We can't call this from frame_notify, because the output's
		 * repaint needed flag is cleared just after that */
		if (output->recorder)
			weston_output_schedule_repaint(&output->base);
	}
}

static uint32_t
drm_output_check_sprite_format(struct drm_sprite *s,
			       struct weston_view *ev, struct gbm_bo *bo)
{
	uint32_t i, format;

	format = gbm_bo_get_format(bo);

	if (format == GBM_FORMAT_ARGB8888) {
		pixman_region32_t r;

		pixman_region32_init_rect(&r, 0, 0,
					  ev->surface->width,
					  ev->surface->height);
		pixman_region32_subtract(&r, &r, &ev->surface->opaque);

		if (!pixman_region32_not_empty(&r))
			format = GBM_FORMAT_XRGB8888;

		pixman_region32_fini(&r);
	}

	for (i = 0; i < s->count_formats; i++)
		if (s->formats[i] == format)
			return format;

	return 0;
}

static int
drm_view_transform_supported(struct weston_view *ev)
{
	return !ev->transform.enabled ||
		(ev->transform.matrix.type < WESTON_MATRIX_TRANSFORM_ROTATE);
}

static struct weston_plane *
drm_output_prepare_overlay_view(struct drm_output *output,
				struct weston_view *ev)
{
	struct weston_compositor *ec = output->base.compositor;
	struct drm_backend *b = (struct drm_backend *)ec->backend;
	struct weston_buffer_viewport *viewport = &ev->surface->buffer_viewport;
	struct wl_resource *buffer_resource;
	struct drm_sprite *s;
	struct linux_dmabuf_buffer *dmabuf;
	int found = 0;
	struct gbm_bo *bo;
	pixman_region32_t dest_rect, src_rect;
	pixman_box32_t *box, tbox;
	uint32_t format;
	wl_fixed_t sx1, sy1, sx2, sy2;

	if (b->gbm == NULL)
		return NULL;

	if (viewport->buffer.transform != output->base.transform)
		return NULL;

	if (viewport->buffer.scale != output->base.current_scale)
		return NULL;

	if (b->sprites_are_broken)
		return NULL;

	if (ev->output_mask != (1u << output->base.id))
		return NULL;

	if (ev->surface->buffer_ref.buffer == NULL)
		return NULL;
	buffer_resource = ev->surface->buffer_ref.buffer->resource;

	if (ev->alpha != 1.0f)
		return NULL;

	if (wl_shm_buffer_get(buffer_resource))
		return NULL;

	if (!drm_view_transform_supported(ev))
		return NULL;

	wl_list_for_each(s, &b->sprite_list, link) {
		if (!drm_sprite_crtc_supported(output, s->possible_crtcs))
			continue;

		if (!s->next) {
			found = 1;
			break;
		}
	}

	/* No sprites available */
	if (!found)
		return NULL;

	if ((dmabuf = linux_dmabuf_buffer_get(buffer_resource))) {
#ifdef HAVE_GBM_FD_IMPORT
		/* XXX: TODO:
		 *
		 * Use AddFB2 directly, do not go via GBM.
		 * Add support for multiplanar formats.
		 * Both require refactoring in the DRM-backend to
		 * support a mix of gbm_bos and drmfbs.
		 */
		struct gbm_import_fd_data gbm_dmabuf = {
			.fd     = dmabuf->dmabuf_fd[0],
			.width  = dmabuf->width,
			.height = dmabuf->height,
			.stride = dmabuf->stride[0],
			.format = dmabuf->format
		};

		if (dmabuf->n_planes != 1 || dmabuf->offset[0] != 0)
			return NULL;

		bo = gbm_bo_import(b->gbm, GBM_BO_IMPORT_FD, &gbm_dmabuf,
				   GBM_BO_USE_SCANOUT);
#else
		return NULL;
#endif
	} else {
		bo = gbm_bo_import(b->gbm, GBM_BO_IMPORT_WL_BUFFER,
				   buffer_resource, GBM_BO_USE_SCANOUT);
	}
	if (!bo)
		return NULL;

	format = drm_output_check_sprite_format(s, ev, bo);
	if (format == 0) {
		gbm_bo_destroy(bo);
		return NULL;
	}

	s->next = drm_fb_get_from_bo(bo, b, format);
	if (!s->next) {
		gbm_bo_destroy(bo);
		return NULL;
	}

	drm_fb_set_buffer(s->next, ev->surface->buffer_ref.buffer);

	box = pixman_region32_extents(&ev->transform.boundingbox);
	s->plane.x = box->x1;
	s->plane.y = box->y1;

	/*
	 * Calculate the source & dest rects properly based on actual
	 * position (note the caller has called weston_view_update_transform()
	 * for us already).
	 */
	pixman_region32_init(&dest_rect);
	pixman_region32_intersect(&dest_rect, &ev->transform.boundingbox,
				  &output->base.region);
	pixman_region32_translate(&dest_rect, -output->base.x, -output->base.y);
	box = pixman_region32_extents(&dest_rect);
	tbox = weston_transformed_rect(output->base.width,
				       output->base.height,
				       output->base.transform,
				       output->base.current_scale,
				       *box);
	s->dest_x = tbox.x1;
	s->dest_y = tbox.y1;
	s->dest_w = tbox.x2 - tbox.x1;
	s->dest_h = tbox.y2 - tbox.y1;
	pixman_region32_fini(&dest_rect);

	pixman_region32_init(&src_rect);
	pixman_region32_intersect(&src_rect, &ev->transform.boundingbox,
				  &output->base.region);
	box = pixman_region32_extents(&src_rect);

	weston_view_from_global_fixed(ev,
				      wl_fixed_from_int(box->x1),
				      wl_fixed_from_int(box->y1),
				      &sx1, &sy1);
	weston_view_from_global_fixed(ev,
				      wl_fixed_from_int(box->x2),
				      wl_fixed_from_int(box->y2),
				      &sx2, &sy2);

	if (sx1 < 0)
		sx1 = 0;
	if (sy1 < 0)
		sy1 = 0;
	if (sx2 > wl_fixed_from_int(ev->surface->width))
		sx2 = wl_fixed_from_int(ev->surface->width);
	if (sy2 > wl_fixed_from_int(ev->surface->height))
		sy2 = wl_fixed_from_int(ev->surface->height);

	tbox.x1 = sx1;
	tbox.y1 = sy1;
	tbox.x2 = sx2;
	tbox.y2 = sy2;

	tbox = weston_transformed_rect(wl_fixed_from_int(ev->surface->width),
				       wl_fixed_from_int(ev->surface->height),
				       viewport->buffer.transform,
				       viewport->buffer.scale,
				       tbox);

	s->src_x = tbox.x1 << 8;
	s->src_y = tbox.y1 << 8;
	s->src_w = (tbox.x2 - tbox.x1) << 8;
	s->src_h = (tbox.y2 - tbox.y1) << 8;
	pixman_region32_fini(&src_rect);

	return &s->plane;
}

static struct weston_plane *
drm_output_prepare_cursor_view(struct drm_output *output,
			       struct weston_view *ev)
{
	struct drm_backend *b =
		(struct drm_backend *)output->base.compositor->backend;
	struct weston_buffer_viewport *viewport = &ev->surface->buffer_viewport;

	if (b->gbm == NULL)
		return NULL;
	if (output->base.transform != WL_OUTPUT_TRANSFORM_NORMAL)
		return NULL;
	if (viewport->buffer.scale != output->base.current_scale)
		return NULL;
	if (output->cursor_view)
		return NULL;
	if (ev->output_mask != (1u << output->base.id))
		return NULL;
	if (b->cursors_are_broken)
		return NULL;
	if (ev->geometry.scissor_enabled)
		return NULL;
	if (ev->surface->buffer_ref.buffer == NULL ||
	    !wl_shm_buffer_get(ev->surface->buffer_ref.buffer->resource) ||
	    ev->surface->width > b->cursor_width ||
	    ev->surface->height > b->cursor_height)
		return NULL;

	output->cursor_view = ev;

	return &output->cursor_plane;
}

/**
 * Update the image for the current cursor surface
 *
 * @param b DRM backend structure
 * @param bo GBM buffer object to write into
 * @param ev View to use for cursor image
 */
static void
cursor_bo_update(struct drm_backend *b, struct gbm_bo *bo,
		 struct weston_view *ev)
{
	struct weston_buffer *buffer = ev->surface->buffer_ref.buffer;
	uint32_t buf[b->cursor_width * b->cursor_height];
	int32_t stride;
	uint8_t *s;
	int i;

	assert(buffer && buffer->shm_buffer);
	assert(buffer->shm_buffer == wl_shm_buffer_get(buffer->resource));
	assert(ev->surface->width <= b->cursor_width);
	assert(ev->surface->height <= b->cursor_height);

	memset(buf, 0, sizeof buf);
	stride = wl_shm_buffer_get_stride(buffer->shm_buffer);
	s = wl_shm_buffer_get_data(buffer->shm_buffer);

	wl_shm_buffer_begin_access(buffer->shm_buffer);
	for (i = 0; i < ev->surface->height; i++)
		memcpy(buf + i * b->cursor_width,
		       s + i * stride,
		       ev->surface->width * 4);
	wl_shm_buffer_end_access(buffer->shm_buffer);

	if (gbm_bo_write(bo, buf, sizeof buf) < 0)
		weston_log("failed update cursor: %m\n");
}

static void
drm_output_set_cursor(struct drm_output *output)
{
	struct weston_view *ev = output->cursor_view;
	struct weston_buffer *buffer;
	struct drm_backend *b =
		(struct drm_backend *) output->base.compositor->backend;
	EGLint handle;
	struct gbm_bo *bo;
	int x, y;

	output->cursor_view = NULL;
	if (ev == NULL) {
		drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
		return;
	}

	buffer = ev->surface->buffer_ref.buffer;

	if (buffer &&
	    pixman_region32_not_empty(&output->cursor_plane.damage)) {
		pixman_region32_fini(&output->cursor_plane.damage);
		pixman_region32_init(&output->cursor_plane.damage);
		output->current_cursor ^= 1;
		bo = output->cursor_bo[output->current_cursor];

		cursor_bo_update(b, bo, ev);
		handle = gbm_bo_get_handle(bo).s32;
		if (drmModeSetCursor(b->drm.fd, output->crtc_id, handle,
				b->cursor_width, b->cursor_height)) {
			weston_log("failed to set cursor: %m\n");
			b->cursors_are_broken = 1;
		}
	}

	x = (ev->geometry.x - output->base.x) * output->base.current_scale;
	y = (ev->geometry.y - output->base.y) * output->base.current_scale;
	if (output->cursor_plane.x != x || output->cursor_plane.y != y) {
		if (drmModeMoveCursor(b->drm.fd, output->crtc_id, x, y)) {
			weston_log("failed to move cursor: %m\n");
			b->cursors_are_broken = 1;
		}

		output->cursor_plane.x = x;
		output->cursor_plane.y = y;
	}
}

static void
drm_assign_planes(struct weston_output *output_base)
{
	struct drm_backend *b =
		(struct drm_backend *)output_base->compositor->backend;
	struct drm_output *output = (struct drm_output *)output_base;
	struct weston_view *ev, *next;
	pixman_region32_t overlap, surface_overlap;
	struct weston_plane *primary, *next_plane;

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
	primary = &output_base->compositor->primary_plane;

	wl_list_for_each_safe(ev, next, &output_base->compositor->view_list, link) {
		struct weston_surface *es = ev->surface;

		/* Test whether this buffer can ever go into a plane:
		 * non-shm, or small enough to be a cursor.
		 *
		 * Also, keep a reference when using the pixman renderer.
		 * That makes it possible to do a seamless switch to the GL
		 * renderer and since the pixman renderer keeps a reference
		 * to the buffer anyway, there is no side effects.
		 */
		if (b->use_pixman ||
		    (es->buffer_ref.buffer &&
		    (!wl_shm_buffer_get(es->buffer_ref.buffer->resource) ||
		     (ev->surface->width <= 64 && ev->surface->height <= 64))))
			es->keep_buffer = true;
		else
			es->keep_buffer = false;

		pixman_region32_init(&surface_overlap);
		pixman_region32_intersect(&surface_overlap, &overlap,
					  &ev->transform.boundingbox);

		next_plane = NULL;
		if (pixman_region32_not_empty(&surface_overlap))
			next_plane = primary;
		if (next_plane == NULL)
			next_plane = drm_output_prepare_cursor_view(output, ev);
		if (next_plane == NULL)
			next_plane = drm_output_prepare_scanout_view(output, ev);
		if (next_plane == NULL)
			next_plane = drm_output_prepare_overlay_view(output, ev);
		if (next_plane == NULL)
			next_plane = primary;

		weston_view_move_to_plane(ev, next_plane);

		if (next_plane == primary)
			pixman_region32_union(&overlap, &overlap,
					      &ev->transform.boundingbox);

		if (next_plane == primary ||
		    next_plane == &output->cursor_plane) {
			/* cursor plane involves a copy */
			ev->psf_flags = 0;
		} else {
			/* All other planes are a direct scanout of a
			 * single client buffer.
			 */
			ev->psf_flags = PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
		}

		pixman_region32_fini(&surface_overlap);
	}
	pixman_region32_fini(&overlap);
}

static void
drm_output_fini_pixman(struct drm_output *output);

static void
drm_output_destroy(struct weston_output *output_base)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_backend *b =
		(struct drm_backend *)output->base.compositor->backend;
	drmModeCrtcPtr origcrtc = output->original_crtc;

	if (output->page_flip_pending) {
		output->destroy_pending = 1;
		weston_log("destroy output while page flip pending\n");
		return;
	}

	if (output->backlight)
		backlight_destroy(output->backlight);

	drmModeFreeProperty(output->dpms_prop);

	/* Turn off hardware cursor */
	drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);

	/* Restore original CRTC state */
	drmModeSetCrtc(b->drm.fd, origcrtc->crtc_id, origcrtc->buffer_id,
		       origcrtc->x, origcrtc->y,
		       &output->connector_id, 1, &origcrtc->mode);
	drmModeFreeCrtc(origcrtc);

	b->crtc_allocator &= ~(1 << output->crtc_id);
	b->connector_allocator &= ~(1 << output->connector_id);

	if (b->use_pixman) {
		drm_output_fini_pixman(output);
	} else {
		gl_renderer->output_destroy(output_base);
		gbm_surface_destroy(output->surface);
	}

	weston_plane_release(&output->fb_plane);
	weston_plane_release(&output->cursor_plane);

	weston_output_destroy(&output->base);

	free(output);
}

/**
 * Find the closest-matching mode for a given target
 *
 * Given a target mode, find the most suitable mode amongst the output's
 * current mode list to use, preferring the current mode if possible, to
 * avoid an expensive mode switch.
 *
 * @param output DRM output
 * @param target_mode Mode to attempt to match
 * @returns Pointer to a mode from the output's mode list
 */
static struct drm_mode *
choose_mode (struct drm_output *output, struct weston_mode *target_mode)
{
	struct drm_mode *tmp_mode = NULL, *mode;

	if (output->base.current_mode->width == target_mode->width &&
	    output->base.current_mode->height == target_mode->height &&
	    (output->base.current_mode->refresh == target_mode->refresh ||
	     target_mode->refresh == 0))
		return (struct drm_mode *)output->base.current_mode;

	wl_list_for_each(mode, &output->base.mode_list, base.link) {
		if (mode->mode_info.hdisplay == target_mode->width &&
		    mode->mode_info.vdisplay == target_mode->height) {
			if (mode->base.refresh == target_mode->refresh ||
			    target_mode->refresh == 0) {
				return mode;
			} else if (!tmp_mode)
				tmp_mode = mode;
		}
	}

	return tmp_mode;
}

static int
drm_output_init_egl(struct drm_output *output, struct drm_backend *b);
static int
drm_output_init_pixman(struct drm_output *output, struct drm_backend *b);

static int
drm_output_switch_mode(struct weston_output *output_base, struct weston_mode *mode)
{
	struct drm_output *output;
	struct drm_mode *drm_mode;
	struct drm_backend *b;

	if (output_base == NULL) {
		weston_log("output is NULL.\n");
		return -1;
	}

	if (mode == NULL) {
		weston_log("mode is NULL.\n");
		return -1;
	}

	b = (struct drm_backend *)output_base->compositor->backend;
	output = (struct drm_output *)output_base;
	drm_mode  = choose_mode (output, mode);

	if (!drm_mode) {
		weston_log("%s, invalid resolution:%dx%d\n", __func__, mode->width, mode->height);
		return -1;
	}

	if (&drm_mode->base == output->base.current_mode)
		return 0;

	output->base.current_mode->flags = 0;

	output->base.current_mode = &drm_mode->base;
	output->base.current_mode->flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	/* reset rendering stuff. */
	drm_output_release_fb(output, output->current);
	drm_output_release_fb(output, output->next);
	output->current = output->next = NULL;

	if (b->use_pixman) {
		drm_output_fini_pixman(output);
		if (drm_output_init_pixman(output, b) < 0) {
			weston_log("failed to init output pixman state with "
				   "new mode\n");
			return -1;
		}
	} else {
		gl_renderer->output_destroy(&output->base);
		gbm_surface_destroy(output->surface);

		if (drm_output_init_egl(output, b) < 0) {
			weston_log("failed to init output egl state with "
				   "new mode");
			return -1;
		}
	}

	return 0;
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
init_drm(struct drm_backend *b, struct udev_device *device)
{
	const char *filename, *sysnum;
	uint64_t cap;
	int fd, ret;
	clockid_t clk_id;

	sysnum = udev_device_get_sysnum(device);
	if (sysnum)
		b->drm.id = atoi(sysnum);
	if (!sysnum || b->drm.id < 0) {
		weston_log("cannot get device sysnum\n");
		return -1;
	}

	filename = udev_device_get_devnode(device);
	fd = weston_launcher_open(b->compositor->launcher, filename, O_RDWR);
	if (fd < 0) {
		/* Probably permissions error */
		weston_log("couldn't open %s, skipping\n",
			udev_device_get_devnode(device));
		return -1;
	}

	weston_log("using %s\n", filename);

	b->drm.fd = fd;
	b->drm.filename = strdup(filename);

	ret = drmGetCap(fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap);
	if (ret == 0 && cap == 1)
		clk_id = CLOCK_MONOTONIC;
	else
		clk_id = CLOCK_REALTIME;

	if (weston_compositor_set_presentation_clock(b->compositor, clk_id) < 0) {
		weston_log("Error: failed to set presentation clock %d.\n",
			   clk_id);
		return -1;
	}

	ret = drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &cap);
	if (ret == 0)
		b->cursor_width = cap;
	else
		b->cursor_width = 64;

	ret = drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &cap);
	if (ret == 0)
		b->cursor_height = cap;
	else
		b->cursor_height = 64;

	return 0;
}

static struct gbm_device *
create_gbm_device(int fd)
{
	struct gbm_device *gbm;

	gl_renderer = weston_load_module("gl-renderer.so",
					 "gl_renderer_interface");
	if (!gl_renderer)
		return NULL;

	/* GBM will load a dri driver, but even though they need symbols from
	 * libglapi, in some version of Mesa they are not linked to it. Since
	 * only the gl-renderer module links to it, the call above won't make
	 * these symbols globally available, and loading the DRI driver fails.
	 * Workaround this by dlopen()'ing libglapi with RTLD_GLOBAL. */
	dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL);

	gbm = gbm_create_device(fd);

	return gbm;
}

/* When initializing EGL, if the preferred buffer format isn't available
 * we may be able to susbstitute an ARGB format for an XRGB one.
 *
 * This returns 0 if substitution isn't possible, but 0 might be a
 * legitimate format for other EGL platforms, so the caller is
 * responsible for checking for 0 before calling gl_renderer->create().
 *
 * This works around https://bugs.freedesktop.org/show_bug.cgi?id=89689
 * but it's entirely possible we'll see this again on other implementations.
 */
static int
fallback_format_for(uint32_t format)
{
	switch (format) {
	case GBM_FORMAT_XRGB8888:
		return GBM_FORMAT_ARGB8888;
	case GBM_FORMAT_XRGB2101010:
		return GBM_FORMAT_ARGB2101010;
	default:
		return 0;
	}
}

static int
drm_backend_create_gl_renderer(struct drm_backend *b)
{
	EGLint format[2] = {
		b->format,
		fallback_format_for(b->format),
	};
	int n_formats = 1;

	if (format[1])
		n_formats = 2;
	if (gl_renderer->create(b->compositor,
				EGL_PLATFORM_GBM_KHR,
				(void *)b->gbm,
				gl_renderer->opaque_attribs,
				format,
				n_formats) < 0) {
		return -1;
	}

	return 0;
}

static int
init_egl(struct drm_backend *b)
{
	b->gbm = create_gbm_device(b->drm.fd);

	if (!b->gbm)
		return -1;

	if (drm_backend_create_gl_renderer(b) < 0) {
		gbm_device_destroy(b->gbm);
		return -1;
	}

	return 0;
}

static int
init_pixman(struct drm_backend *b)
{
	return pixman_renderer_init(b->compositor);
}

/**
 * Add a mode to output's mode list
 *
 * Copy the supplied DRM mode into a Weston mode structure, and add it to the
 * output's mode list.
 *
 * @param output DRM output to add mode to
 * @param info DRM mode structure to add
 * @returns Newly-allocated Weston/DRM mode structure
 */
static struct drm_mode *
drm_output_add_mode(struct drm_output *output, const drmModeModeInfo *info)
{
	struct drm_mode *mode;
	uint64_t refresh;

	mode = malloc(sizeof *mode);
	if (mode == NULL)
		return NULL;

	mode->base.flags = 0;
	mode->base.width = info->hdisplay;
	mode->base.height = info->vdisplay;

	/* Calculate higher precision (mHz) refresh rate */
	refresh = (info->clock * 1000000LL / info->htotal +
		   info->vtotal / 2) / info->vtotal;

	if (info->flags & DRM_MODE_FLAG_INTERLACE)
		refresh *= 2;
	if (info->flags & DRM_MODE_FLAG_DBLSCAN)
		refresh /= 2;
	if (info->vscan > 1)
	    refresh /= info->vscan;

	mode->base.refresh = refresh;
	mode->mode_info = *info;

	if (info->type & DRM_MODE_TYPE_PREFERRED)
		mode->base.flags |= WL_OUTPUT_MODE_PREFERRED;

	wl_list_insert(output->base.mode_list.prev, &mode->base.link);

	return mode;
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
	struct drm_backend *b = (struct drm_backend *)ec->backend;
	int ret;

	if (!output->dpms_prop)
		return;

	ret = drmModeConnectorSetProperty(b->drm.fd, output->connector_id,
				 	  output->dpms_prop->prop_id, level);
	if (ret) {
		weston_log("DRM: DPMS: failed property set for %s\n",
			   output->base.name);
		return;
	}

	output->dpms = level;
}

static const char * const connector_type_names[] = {
	[DRM_MODE_CONNECTOR_Unknown]     = "Unknown",
	[DRM_MODE_CONNECTOR_VGA]         = "VGA",
	[DRM_MODE_CONNECTOR_DVII]        = "DVI-I",
	[DRM_MODE_CONNECTOR_DVID]        = "DVI-D",
	[DRM_MODE_CONNECTOR_DVIA]        = "DVI-A",
	[DRM_MODE_CONNECTOR_Composite]   = "Composite",
	[DRM_MODE_CONNECTOR_SVIDEO]      = "SVIDEO",
	[DRM_MODE_CONNECTOR_LVDS]        = "LVDS",
	[DRM_MODE_CONNECTOR_Component]   = "Component",
	[DRM_MODE_CONNECTOR_9PinDIN]     = "DIN",
	[DRM_MODE_CONNECTOR_DisplayPort] = "DP",
	[DRM_MODE_CONNECTOR_HDMIA]       = "HDMI-A",
	[DRM_MODE_CONNECTOR_HDMIB]       = "HDMI-B",
	[DRM_MODE_CONNECTOR_TV]          = "TV",
	[DRM_MODE_CONNECTOR_eDP]         = "eDP",
#ifdef DRM_MODE_CONNECTOR_DSI
	[DRM_MODE_CONNECTOR_VIRTUAL]     = "Virtual",
	[DRM_MODE_CONNECTOR_DSI]         = "DSI",
#endif
};

static char *
make_connector_name(const drmModeConnector *con)
{
	char name[32];
	const char *type_name = NULL;

	if (con->connector_type < ARRAY_LENGTH(connector_type_names))
		type_name = connector_type_names[con->connector_type];

	if (!type_name)
		type_name = "UNNAMED";

	snprintf(name, sizeof name, "%s-%d", type_name, con->connector_type_id);

	return strdup(name);
}

static int
find_crtc_for_connector(struct drm_backend *b,
			drmModeRes *resources, drmModeConnector *connector)
{
	drmModeEncoder *encoder;
	uint32_t possible_crtcs;
	int i, j;

	for (j = 0; j < connector->count_encoders; j++) {
		encoder = drmModeGetEncoder(b->drm.fd, connector->encoders[j]);
		if (encoder == NULL) {
			weston_log("Failed to get encoder.\n");
			return -1;
		}
		possible_crtcs = encoder->possible_crtcs;
		drmModeFreeEncoder(encoder);

		for (i = 0; i < resources->count_crtcs; i++) {
			if (possible_crtcs & (1 << i) &&
			    !(b->crtc_allocator & (1 << resources->crtcs[i])))
				return i;
		}
	}

	return -1;
}

/* Init output state that depends on gl or gbm */
static int
drm_output_init_egl(struct drm_output *output, struct drm_backend *b)
{
	EGLint format[2] = {
		output->format,
		fallback_format_for(output->format),
	};
	int i, flags, n_formats = 1;

	output->surface = gbm_surface_create(b->gbm,
					     output->base.current_mode->width,
					     output->base.current_mode->height,
					     format[0],
					     GBM_BO_USE_SCANOUT |
					     GBM_BO_USE_RENDERING);
	if (!output->surface) {
		weston_log("failed to create gbm surface\n");
		return -1;
	}

	if (format[1])
		n_formats = 2;
	if (gl_renderer->output_create(&output->base,
				       (EGLNativeWindowType)output->surface,
				       output->surface,
				       gl_renderer->opaque_attribs,
				       format,
				       n_formats) < 0) {
		weston_log("failed to create gl renderer output state\n");
		gbm_surface_destroy(output->surface);
		return -1;
	}

	flags = GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE;

	for (i = 0; i < 2; i++) {
		if (output->cursor_bo[i])
			continue;

		output->cursor_bo[i] =
			gbm_bo_create(b->gbm, b->cursor_width, b->cursor_height,
				GBM_FORMAT_ARGB8888, flags);
	}

	if (output->cursor_bo[0] == NULL || output->cursor_bo[1] == NULL) {
		weston_log("cursor buffers unavailable, using gl cursors\n");
		b->cursors_are_broken = 1;
	}

	return 0;
}

static int
drm_output_init_pixman(struct drm_output *output, struct drm_backend *b)
{
	int w = output->base.current_mode->width;
	int h = output->base.current_mode->height;
	unsigned int i;

	/* FIXME error checking */

	for (i = 0; i < ARRAY_LENGTH(output->dumb); i++) {
		output->dumb[i] = drm_fb_create_dumb(b, w, h);
		if (!output->dumb[i])
			goto err;

		output->image[i] =
			pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h,
						 output->dumb[i]->map,
						 output->dumb[i]->stride);
		if (!output->image[i])
			goto err;
	}

	if (pixman_renderer_output_create(&output->base) < 0)
		goto err;

	pixman_region32_init_rect(&output->previous_damage,
				  output->base.x, output->base.y, output->base.width, output->base.height);

	return 0;

err:
	for (i = 0; i < ARRAY_LENGTH(output->dumb); i++) {
		if (output->dumb[i])
			drm_fb_destroy_dumb(output->dumb[i]);
		if (output->image[i])
			pixman_image_unref(output->image[i]);

		output->dumb[i] = NULL;
		output->image[i] = NULL;
	}

	return -1;
}

static void
drm_output_fini_pixman(struct drm_output *output)
{
	unsigned int i;

	pixman_renderer_output_destroy(&output->base);
	pixman_region32_fini(&output->previous_damage);

	for (i = 0; i < ARRAY_LENGTH(output->dumb); i++) {
		drm_fb_destroy_dumb(output->dumb[i]);
		pixman_image_unref(output->image[i]);
		output->dumb[i] = NULL;
		output->image[i] = NULL;
	}
}

static void
edid_parse_string(const uint8_t *data, char text[])
{
	int i;
	int replaced = 0;

	/* this is always 12 bytes, but we can't guarantee it's null
	 * terminated or not junk. */
	strncpy(text, (const char *) data, 12);

	/* guarantee our new string is null-terminated */
	text[12] = '\0';

	/* remove insane chars */
	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == '\n' ||
		    text[i] == '\r') {
			text[i] = '\0';
			break;
		}
	}

	/* ensure string is printable */
	for (i = 0; text[i] != '\0'; i++) {
		if (!isprint(text[i])) {
			text[i] = '-';
			replaced++;
		}
	}

	/* if the string is random junk, ignore the string */
	if (replaced > 4)
		text[0] = '\0';
}

#define EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING	0xfe
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME		0xfc
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER	0xff
#define EDID_OFFSET_DATA_BLOCKS				0x36
#define EDID_OFFSET_LAST_BLOCK				0x6c
#define EDID_OFFSET_PNPID				0x08
#define EDID_OFFSET_SERIAL				0x0c

static int
edid_parse(struct drm_edid *edid, const uint8_t *data, size_t length)
{
	int i;
	uint32_t serial_number;

	/* check header */
	if (length < 128)
		return -1;
	if (data[0] != 0x00 || data[1] != 0xff)
		return -1;

	/* decode the PNP ID from three 5 bit words packed into 2 bytes
	 * /--08--\/--09--\
	 * 7654321076543210
	 * |\---/\---/\---/
	 * R  C1   C2   C3 */
	edid->pnp_id[0] = 'A' + ((data[EDID_OFFSET_PNPID + 0] & 0x7c) / 4) - 1;
	edid->pnp_id[1] = 'A' + ((data[EDID_OFFSET_PNPID + 0] & 0x3) * 8) + ((data[EDID_OFFSET_PNPID + 1] & 0xe0) / 32) - 1;
	edid->pnp_id[2] = 'A' + (data[EDID_OFFSET_PNPID + 1] & 0x1f) - 1;
	edid->pnp_id[3] = '\0';

	/* maybe there isn't a ASCII serial number descriptor, so use this instead */
	serial_number = (uint32_t) data[EDID_OFFSET_SERIAL + 0];
	serial_number += (uint32_t) data[EDID_OFFSET_SERIAL + 1] * 0x100;
	serial_number += (uint32_t) data[EDID_OFFSET_SERIAL + 2] * 0x10000;
	serial_number += (uint32_t) data[EDID_OFFSET_SERIAL + 3] * 0x1000000;
	if (serial_number > 0)
		sprintf(edid->serial_number, "%lu", (unsigned long) serial_number);

	/* parse EDID data */
	for (i = EDID_OFFSET_DATA_BLOCKS;
	     i <= EDID_OFFSET_LAST_BLOCK;
	     i += 18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
			edid_parse_string(&data[i+5],
					  edid->monitor_name);
		} else if (data[i+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
			edid_parse_string(&data[i+5],
					  edid->serial_number);
		} else if (data[i+3] == EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
			edid_parse_string(&data[i+5],
					  edid->eisa_id);
		}
	}
	return 0;
}

static void
find_and_parse_output_edid(struct drm_backend *b,
			   struct drm_output *output,
			   drmModeConnector *connector)
{
	drmModePropertyBlobPtr edid_blob = NULL;
	drmModePropertyPtr property;
	int i;
	int rc;

	for (i = 0; i < connector->count_props && !edid_blob; i++) {
		property = drmModeGetProperty(b->drm.fd, connector->props[i]);
		if (!property)
			continue;
		if ((property->flags & DRM_MODE_PROP_BLOB) &&
		    !strcmp(property->name, "EDID")) {
			edid_blob = drmModeGetPropertyBlob(b->drm.fd,
							   connector->prop_values[i]);
		}
		drmModeFreeProperty(property);
	}
	if (!edid_blob)
		return;

	rc = edid_parse(&output->edid,
			edid_blob->data,
			edid_blob->length);
	if (!rc) {
		weston_log("EDID data '%s', '%s', '%s'\n",
			   output->edid.pnp_id,
			   output->edid.monitor_name,
			   output->edid.serial_number);
		if (output->edid.pnp_id[0] != '\0')
			output->base.make = output->edid.pnp_id;
		if (output->edid.monitor_name[0] != '\0')
			output->base.model = output->edid.monitor_name;
		if (output->edid.serial_number[0] != '\0')
			output->base.serial_number = output->edid.serial_number;
	}
	drmModeFreePropertyBlob(edid_blob);
}



static int
parse_modeline(const char *s, drmModeModeInfo *mode)
{
	char hsync[16];
	char vsync[16];
	float fclock;

	mode->type = DRM_MODE_TYPE_USERDEF;
	mode->hskew = 0;
	mode->vscan = 0;
	mode->vrefresh = 0;
	mode->flags = 0;

	if (sscanf(s, "%f %hd %hd %hd %hd %hd %hd %hd %hd %15s %15s",
		   &fclock,
		   &mode->hdisplay,
		   &mode->hsync_start,
		   &mode->hsync_end,
		   &mode->htotal,
		   &mode->vdisplay,
		   &mode->vsync_start,
		   &mode->vsync_end,
		   &mode->vtotal, hsync, vsync) != 11)
		return -1;

	mode->clock = fclock * 1000;
	if (strcmp(hsync, "+hsync") == 0)
		mode->flags |= DRM_MODE_FLAG_PHSYNC;
	else if (strcmp(hsync, "-hsync") == 0)
		mode->flags |= DRM_MODE_FLAG_NHSYNC;
	else
		return -1;

	if (strcmp(vsync, "+vsync") == 0)
		mode->flags |= DRM_MODE_FLAG_PVSYNC;
	else if (strcmp(vsync, "-vsync") == 0)
		mode->flags |= DRM_MODE_FLAG_NVSYNC;
	else
		return -1;

	return 0;
}

static void
setup_output_seat_constraint(struct drm_backend *b,
			     struct weston_output *output,
			     const char *s)
{
	if (strcmp(s, "") != 0) {
		struct weston_pointer *pointer;
		struct udev_seat *seat;

		seat = udev_seat_get_named(&b->input, s);
		if (!seat)
			return;

		seat->base.output = output;

		pointer = weston_seat_get_pointer(&seat->base);
		if (pointer)
			weston_pointer_clamp(pointer,
					     &pointer->x,
					     &pointer->y);
	}
}

static int
get_gbm_format_from_section(struct weston_config_section *section,
			    uint32_t default_value,
			    uint32_t *format)
{
	char *s;
	int ret = 0;

	weston_config_section_get_string(section,
					 "gbm-format", &s, NULL);

	if (s == NULL)
		*format = default_value;
	else if (strcmp(s, "xrgb8888") == 0)
		*format = GBM_FORMAT_XRGB8888;
	else if (strcmp(s, "rgb565") == 0)
		*format = GBM_FORMAT_RGB565;
	else if (strcmp(s, "xrgb2101010") == 0)
		*format = GBM_FORMAT_XRGB2101010;
	else {
		weston_log("fatal: unrecognized pixel format: %s\n", s);
		ret = -1;
	}

	free(s);

	return ret;
}

/**
 * Choose suitable mode for an output
 *
 * Find the most suitable mode to use for initial setup (or reconfiguration on
 * hotplug etc) for a DRM output.
 *
 * @param output DRM output to choose mode for
 * @param kind Strategy and preference to use when choosing mode
 * @param width Desired width for this output
 * @param height Desired height for this output
 * @param current_mode Mode currently being displayed on this output
 * @param modeline Manually-entered mode (may be NULL)
 * @returns A mode from the output's mode list, or NULL if none available
 */
static struct drm_mode *
drm_output_choose_initial_mode(struct drm_output *output,
			       enum output_config kind,
			       int width, int height,
			       const drmModeModeInfo *current_mode,
			       const drmModeModeInfo *modeline)
{
	struct drm_mode *preferred = NULL;
	struct drm_mode *current = NULL;
	struct drm_mode *configured = NULL;
	struct drm_mode *best = NULL;
	struct drm_mode *drm_mode;

	wl_list_for_each_reverse(drm_mode, &output->base.mode_list, base.link) {
		if (kind == OUTPUT_CONFIG_MODE &&
		    width == drm_mode->base.width &&
		    height == drm_mode->base.height)
			configured = drm_mode;

		if (memcmp(&current_mode, &drm_mode->mode_info,
			   sizeof *current_mode) == 0)
			current = drm_mode;

		if (drm_mode->base.flags & WL_OUTPUT_MODE_PREFERRED)
			preferred = drm_mode;

		best = drm_mode;
	}

	if (kind == OUTPUT_CONFIG_MODELINE) {
		configured = drm_output_add_mode(output, modeline);
		if (!configured)
			return NULL;
	}

	if (current == NULL && current_mode->clock != 0) {
		current = drm_output_add_mode(output, current_mode);
		if (!current)
			return NULL;
	}

	if (kind == OUTPUT_CONFIG_CURRENT)
		configured = current;

	if (option_current_mode && current)
		return current;

	if (configured)
		return configured;

	if (preferred)
		return preferred;

	if (current)
		return current;

	if (best)
		return best;

	weston_log("no available modes for %s\n", output->base.name);
	return NULL;
}

static int
connector_get_current_mode(drmModeConnector *connector, int drm_fd,
			   drmModeModeInfo *mode)
{
	drmModeEncoder *encoder;
	drmModeCrtc *crtc;

	/* Get the current mode on the crtc that's currently driving
	 * this connector. */
	encoder = drmModeGetEncoder(drm_fd, connector->encoder_id);
	memset(mode, 0, sizeof *mode);
	if (encoder != NULL) {
		crtc = drmModeGetCrtc(drm_fd, encoder->crtc_id);
		drmModeFreeEncoder(encoder);
		if (crtc == NULL)
			return -1;
		if (crtc->mode_valid)
			*mode = crtc->mode;
		drmModeFreeCrtc(crtc);
	}

	return 0;
}

/**
 * Create and configure a Weston output structure
 *
 * Given a DRM connector, create a matching drm_output structure and add it
 * to Weston's output list.
 *
 * @param b Weston backend structure structure
 * @param resources DRM resources for this device
 * @param connector DRM connector to use for this new output
 * @param x Horizontal offset to use into global co-ordinate space
 * @param y Vertical offset to use into global co-ordinate space
 * @param drm_device udev device pointer
 * @returns 0 on success, or -1 on failure
 */
static int
create_output_for_connector(struct drm_backend *b,
			    drmModeRes *resources,
			    drmModeConnector *connector,
			    int x, int y, struct udev_device *drm_device)
{
	struct drm_output *output;
	struct drm_mode *drm_mode, *next, *current;
	struct weston_mode *m;
	struct weston_config_section *section;
	drmModeModeInfo crtc_mode, modeline;
	int i, width, height, scale;
	char *s;
	enum output_config config;
	uint32_t transform;

	i = find_crtc_for_connector(b, resources, connector);
	if (i < 0) {
		weston_log("No usable crtc/encoder pair for connector.\n");
		return -1;
	}

	output = zalloc(sizeof *output);
	if (output == NULL)
		return -1;

	output->base.subpixel = drm_subpixel_to_wayland(connector->subpixel);
	output->base.name = make_connector_name(connector);
	output->base.make = "unknown";
	output->base.model = "unknown";
	output->base.serial_number = "unknown";
	wl_list_init(&output->base.mode_list);

	section = weston_config_get_section(b->compositor->config, "output", "name",
					    output->base.name);
	weston_config_section_get_string(section, "mode", &s, "preferred");
	if (strcmp(s, "off") == 0)
		config = OUTPUT_CONFIG_OFF;
	else if (strcmp(s, "preferred") == 0)
		config = OUTPUT_CONFIG_PREFERRED;
	else if (strcmp(s, "current") == 0)
		config = OUTPUT_CONFIG_CURRENT;
	else if (sscanf(s, "%dx%d", &width, &height) == 2)
		config = OUTPUT_CONFIG_MODE;
	else if (parse_modeline(s, &modeline) == 0)
		config = OUTPUT_CONFIG_MODELINE;
	else {
		weston_log("Invalid mode \"%s\" for output %s\n",
			   s, output->base.name);
		config = OUTPUT_CONFIG_PREFERRED;
	}
	free(s);

	weston_config_section_get_int(section, "scale", &scale, 1);
	weston_config_section_get_string(section, "transform", &s, "normal");
	if (weston_parse_transform(s, &transform) < 0)
		weston_log("Invalid transform \"%s\" for output %s\n",
			   s, output->base.name);

	free(s);

	if (get_gbm_format_from_section(section,
					b->format,
					&output->format) == -1)
		output->format = b->format;

	weston_config_section_get_string(section, "seat", &s, "");
	setup_output_seat_constraint(b, &output->base, s);
	free(s);

	output->crtc_id = resources->crtcs[i];
	output->pipe = i;
	b->crtc_allocator |= (1 << output->crtc_id);
	output->connector_id = connector->connector_id;
	b->connector_allocator |= (1 << output->connector_id);

	output->original_crtc = drmModeGetCrtc(b->drm.fd, output->crtc_id);
	output->dpms_prop = drm_get_prop(b->drm.fd, connector, "DPMS");

	if (connector_get_current_mode(connector, b->drm.fd, &crtc_mode) < 0)
		goto err_free;

	for (i = 0; i < connector->count_modes; i++) {
		drm_mode = drm_output_add_mode(output, &connector->modes[i]);
		if (!drm_mode)
			goto err_free;
	}

	if (config == OUTPUT_CONFIG_OFF) {
		weston_log("Disabling output %s\n", output->base.name);
		drmModeSetCrtc(b->drm.fd, output->crtc_id,
			       0, 0, 0, 0, 0, NULL);
		goto err_free;
	}

	current = drm_output_choose_initial_mode(output, config,
						 width, height,
						 &crtc_mode, &modeline);
	if (!current)
		goto err_free;
	output->base.current_mode = &current->base;
	output->base.current_mode->flags |= WL_OUTPUT_MODE_CURRENT;

	weston_output_init(&output->base, b->compositor, x, y,
			   connector->mmWidth, connector->mmHeight,
			   transform, scale);

	if (b->use_pixman) {
		if (drm_output_init_pixman(output, b) < 0) {
			weston_log("Failed to init output pixman state\n");
			goto err_output;
		}
	} else if (drm_output_init_egl(output, b) < 0) {
		weston_log("Failed to init output gl state\n");
		goto err_output;
	}

	output->backlight = backlight_init(drm_device,
					   connector->connector_type);
	if (output->backlight) {
		weston_log("Initialized backlight, device %s\n",
			   output->backlight->path);
		output->base.set_backlight = drm_set_backlight;
		output->base.backlight_current = drm_get_backlight(output);
	} else {
		weston_log("Failed to initialize backlight\n");
	}

	weston_compositor_add_output(b->compositor, &output->base);

	find_and_parse_output_edid(b, output, connector);
	if (connector->connector_type == DRM_MODE_CONNECTOR_LVDS)
		output->base.connection_internal = 1;

	output->base.start_repaint_loop = drm_output_start_repaint_loop;
	output->base.repaint = drm_output_repaint;
	output->base.destroy = drm_output_destroy;
	output->base.assign_planes = drm_assign_planes;
	output->base.set_dpms = drm_set_dpms;
	output->base.switch_mode = drm_output_switch_mode;

	output->base.gamma_size = output->original_crtc->gamma_size;
	output->base.set_gamma = drm_output_set_gamma;

	weston_plane_init(&output->cursor_plane, b->compositor, 0, 0);
	weston_plane_init(&output->fb_plane, b->compositor, 0, 0);

	weston_compositor_stack_plane(b->compositor, &output->cursor_plane, NULL);
	weston_compositor_stack_plane(b->compositor, &output->fb_plane,
				      &b->compositor->primary_plane);

	weston_log("Output %s, (connector %d, crtc %d)\n",
		   output->base.name, output->connector_id, output->crtc_id);
	wl_list_for_each(m, &output->base.mode_list, link)
		weston_log_continue(STAMP_SPACE "mode %dx%d@%.1f%s%s%s\n",
				    m->width, m->height, m->refresh / 1000.0,
				    m->flags & WL_OUTPUT_MODE_PREFERRED ?
				    ", preferred" : "",
				    m->flags & WL_OUTPUT_MODE_CURRENT ?
				    ", current" : "",
				    connector->count_modes == 0 ?
				    ", built-in" : "");

	/* Set native_ fields, so weston_output_mode_switch_to_native() works */
	output->base.native_mode = output->base.current_mode;
	output->base.native_scale = output->base.current_scale;

	return 0;

err_output:
	weston_output_destroy(&output->base);
err_free:
	wl_list_for_each_safe(drm_mode, next, &output->base.mode_list,
							base.link) {
		wl_list_remove(&drm_mode->base.link);
		free(drm_mode);
	}

	drmModeFreeCrtc(output->original_crtc);
	b->crtc_allocator &= ~(1 << output->crtc_id);
	b->connector_allocator &= ~(1 << output->connector_id);
	free(output);

	return -1;
}

static void
create_sprites(struct drm_backend *b)
{
	struct drm_sprite *sprite;
	drmModePlaneRes *plane_res;
	drmModePlane *plane;
	uint32_t i;

	plane_res = drmModeGetPlaneResources(b->drm.fd);
	if (!plane_res) {
		weston_log("failed to get plane resources: %s\n",
			strerror(errno));
		return;
	}

	for (i = 0; i < plane_res->count_planes; i++) {
		plane = drmModeGetPlane(b->drm.fd, plane_res->planes[i]);
		if (!plane)
			continue;

		sprite = zalloc(sizeof(*sprite) + ((sizeof(uint32_t)) *
						   plane->count_formats));
		if (!sprite) {
			weston_log("%s: out of memory\n",
				__func__);
			drmModeFreePlane(plane);
			continue;
		}

		sprite->possible_crtcs = plane->possible_crtcs;
		sprite->plane_id = plane->plane_id;
		sprite->current = NULL;
		sprite->next = NULL;
		sprite->backend = b;
		sprite->count_formats = plane->count_formats;
		memcpy(sprite->formats, plane->formats,
		       plane->count_formats * sizeof(plane->formats[0]));
		drmModeFreePlane(plane);
		weston_plane_init(&sprite->plane, b->compositor, 0, 0);
		weston_compositor_stack_plane(b->compositor, &sprite->plane,
					      &b->compositor->primary_plane);

		wl_list_insert(&b->sprite_list, &sprite->link);
	}

	drmModeFreePlaneResources(plane_res);
}

static void
destroy_sprites(struct drm_backend *backend)
{
	struct drm_sprite *sprite, *next;
	struct drm_output *output;

	output = container_of(backend->compositor->output_list.next,
			      struct drm_output, base.link);

	wl_list_for_each_safe(sprite, next, &backend->sprite_list, link) {
		drmModeSetPlane(backend->drm.fd,
				sprite->plane_id,
				output->crtc_id, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0);
		drm_output_release_fb(output, sprite->current);
		drm_output_release_fb(output, sprite->next);
		weston_plane_release(&sprite->plane);
		free(sprite);
	}
}

static int
create_outputs(struct drm_backend *b, uint32_t option_connector,
	       struct udev_device *drm_device)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	int i;
	int x = 0, y = 0;

	resources = drmModeGetResources(b->drm.fd);
	if (!resources) {
		weston_log("drmModeGetResources failed\n");
		return -1;
	}

	b->crtcs = calloc(resources->count_crtcs, sizeof(uint32_t));
	if (!b->crtcs) {
		drmModeFreeResources(resources);
		return -1;
	}

	b->min_width  = resources->min_width;
	b->max_width  = resources->max_width;
	b->min_height = resources->min_height;
	b->max_height = resources->max_height;

	b->num_crtcs = resources->count_crtcs;
	memcpy(b->crtcs, resources->crtcs, sizeof(uint32_t) * b->num_crtcs);

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(b->drm.fd,
						resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    (option_connector == 0 ||
		     connector->connector_id == option_connector)) {
			if (create_output_for_connector(b, resources,
							connector, x, y,
							drm_device) < 0) {
				drmModeFreeConnector(connector);
				continue;
			}

			x += container_of(b->compositor->output_list.prev,
					  struct weston_output,
					  link)->width;
		}

		drmModeFreeConnector(connector);
	}

	if (wl_list_empty(&b->compositor->output_list)) {
		weston_log("No currently active connector found.\n");
		drmModeFreeResources(resources);
		return -1;
	}

	drmModeFreeResources(resources);

	return 0;
}

static void
update_outputs(struct drm_backend *b, struct udev_device *drm_device)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	struct drm_output *output, *next;
	int x = 0, y = 0;
	uint32_t connected = 0, disconnects = 0;
	int i;

	resources = drmModeGetResources(b->drm.fd);
	if (!resources) {
		weston_log("drmModeGetResources failed\n");
		return;
	}

	/* collect new connects */
	for (i = 0; i < resources->count_connectors; i++) {
		int connector_id = resources->connectors[i];

		connector = drmModeGetConnector(b->drm.fd, connector_id);
		if (connector == NULL)
			continue;

		if (connector->connection != DRM_MODE_CONNECTED) {
			drmModeFreeConnector(connector);
			continue;
		}

		connected |= (1 << connector_id);

		if (!(b->connector_allocator & (1 << connector_id))) {
			struct weston_output *last =
				container_of(b->compositor->output_list.prev,
					     struct weston_output, link);

			/* XXX: not yet needed, we die with 0 outputs */
			if (!wl_list_empty(&b->compositor->output_list))
				x = last->x + last->width;
			else
				x = 0;
			y = 0;
			create_output_for_connector(b, resources,
						    connector, x, y,
						    drm_device);
			weston_log("connector %d connected\n", connector_id);

		}
		drmModeFreeConnector(connector);
	}
	drmModeFreeResources(resources);

	disconnects = b->connector_allocator & ~connected;
	if (disconnects) {
		wl_list_for_each_safe(output, next, &b->compositor->output_list,
				      base.link) {
			if (disconnects & (1 << output->connector_id)) {
				disconnects &= ~(1 << output->connector_id);
				weston_log("connector %d disconnected\n",
				       output->connector_id);
				drm_output_destroy(&output->base);
			}
		}
	}

	/* FIXME: handle zero outputs, without terminating */
	if (b->connector_allocator == 0)
		weston_compositor_exit(b->compositor);
}

static int
udev_event_is_hotplug(struct drm_backend *b, struct udev_device *device)
{
	const char *sysnum;
	const char *val;

	sysnum = udev_device_get_sysnum(device);
	if (!sysnum || atoi(sysnum) != b->drm.id)
		return 0;

	val = udev_device_get_property_value(device, "HOTPLUG");
	if (!val)
		return 0;

	return strcmp(val, "1") == 0;
}

static int
udev_drm_event(int fd, uint32_t mask, void *data)
{
	struct drm_backend *b = data;
	struct udev_device *event;

	event = udev_monitor_receive_device(b->udev_monitor);

	if (udev_event_is_hotplug(b, event))
		update_outputs(b, event);

	udev_device_unref(event);

	return 1;
}

static void
drm_restore(struct weston_compositor *ec)
{
	weston_launcher_restore(ec->launcher);
}

static void
drm_destroy(struct weston_compositor *ec)
{
	struct drm_backend *b = (struct drm_backend *) ec->backend;

	udev_input_destroy(&b->input);

	wl_event_source_remove(b->udev_drm_source);
	wl_event_source_remove(b->drm_source);

	destroy_sprites(b);

	weston_compositor_shutdown(ec);

	if (b->gbm)
		gbm_device_destroy(b->gbm);

	weston_launcher_destroy(ec->launcher);

	close(b->drm.fd);

	free(b);
}

static void
drm_backend_set_modes(struct drm_backend *backend)
{
	struct drm_output *output;
	struct drm_mode *drm_mode;
	int ret;

	wl_list_for_each(output, &backend->compositor->output_list, base.link) {
		if (!output->current) {
			/* If something that would cause the output to
			 * switch mode happened while in another vt, we
			 * might not have a current drm_fb. In that case,
			 * schedule a repaint and let drm_output_repaint
			 * handle setting the mode. */
			weston_output_schedule_repaint(&output->base);
			continue;
		}

		drm_mode = (struct drm_mode *) output->base.current_mode;
		ret = drmModeSetCrtc(backend->drm.fd, output->crtc_id,
				     output->current->fb_id, 0, 0,
				     &output->connector_id, 1,
				     &drm_mode->mode_info);
		if (ret < 0) {
			weston_log(
				"failed to set mode %dx%d for output at %d,%d: %m\n",
				drm_mode->base.width, drm_mode->base.height,
				output->base.x, output->base.y);
		}
	}
}

static void
session_notify(struct wl_listener *listener, void *data)
{
	struct weston_compositor *compositor = data;
	struct drm_backend *b = (struct drm_backend *)compositor->backend;
	struct drm_sprite *sprite;
	struct drm_output *output;

	if (compositor->session_active) {
		weston_log("activating session\n");
		compositor->state = b->prev_state;
		drm_backend_set_modes(b);
		weston_compositor_damage_all(compositor);
		udev_input_enable(&b->input);
	} else {
		weston_log("deactivating session\n");
		udev_input_disable(&b->input);

		b->prev_state = compositor->state;
		weston_compositor_offscreen(compositor);

		/* If we have a repaint scheduled (either from a
		 * pending pageflip or the idle handler), make sure we
		 * cancel that so we don't try to pageflip when we're
		 * vt switched away.  The OFFSCREEN state will prevent
		 * further attemps at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output, &compositor->output_list, base.link) {
			output->base.repaint_needed = 0;
			drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
		}

		output = container_of(compositor->output_list.next,
				      struct drm_output, base.link);

		wl_list_for_each(sprite, &b->sprite_list, link)
			drmModeSetPlane(b->drm.fd,
					sprite->plane_id,
					output->crtc_id, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0);
	};
}

static void
switch_vt_binding(struct weston_keyboard *keyboard, uint32_t time,
		  uint32_t key, void *data)
{
	struct weston_compositor *compositor = data;

	weston_launcher_activate_vt(compositor->launcher, key - KEY_F1 + 1);
}

/*
 * Find primary GPU
 * Some systems may have multiple DRM devices attached to a single seat. This
 * function loops over all devices and tries to find a PCI device with the
 * boot_vga sysfs attribute set to 1.
 * If no such device is found, the first DRM device reported by udev is used.
 */
static struct udev_device*
find_primary_gpu(struct drm_backend *b, const char *seat)
{
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	const char *path, *device_seat, *id;
	struct udev_device *device, *drm_device, *pci;

	e = udev_enumerate_new(b->udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_sysname(e, "card[0-9]*");

	udev_enumerate_scan_devices(e);
	drm_device = NULL;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(b->udev, path);
		if (!device)
			continue;
		device_seat = udev_device_get_property_value(device, "ID_SEAT");
		if (!device_seat)
			device_seat = default_seat;
		if (strcmp(device_seat, seat)) {
			udev_device_unref(device);
			continue;
		}

		pci = udev_device_get_parent_with_subsystem_devtype(device,
								"pci", NULL);
		if (pci) {
			id = udev_device_get_sysattr_value(pci, "boot_vga");
			if (id && !strcmp(id, "1")) {
				if (drm_device)
					udev_device_unref(drm_device);
				drm_device = device;
				break;
			}
		}

		if (!drm_device)
			drm_device = device;
		else
			udev_device_unref(device);
	}

	udev_enumerate_unref(e);
	return drm_device;
}

static void
planes_binding(struct weston_keyboard *keyboard, uint32_t time, uint32_t key,
	       void *data)
{
	struct drm_backend *b = data;

	switch (key) {
	case KEY_C:
		b->cursors_are_broken ^= 1;
		break;
	case KEY_V:
		b->sprites_are_broken ^= 1;
		break;
	case KEY_O:
		b->sprites_hidden ^= 1;
		break;
	default:
		break;
	}
}

#ifdef BUILD_VAAPI_RECORDER
static void
recorder_destroy(struct drm_output *output)
{
	vaapi_recorder_destroy(output->recorder);
	output->recorder = NULL;

	output->base.disable_planes--;

	wl_list_remove(&output->recorder_frame_listener.link);
	weston_log("[libva recorder] done\n");
}

static void
recorder_frame_notify(struct wl_listener *listener, void *data)
{
	struct drm_output *output;
	struct drm_backend *b;
	int fd, ret;

	output = container_of(listener, struct drm_output,
			      recorder_frame_listener);
	b = (struct drm_backend *)output->base.compositor->backend;

	if (!output->recorder)
		return;

	ret = drmPrimeHandleToFD(b->drm.fd, output->current->handle,
				 DRM_CLOEXEC, &fd);
	if (ret) {
		weston_log("[libva recorder] "
			   "failed to create prime fd for front buffer\n");
		return;
	}

	ret = vaapi_recorder_frame(output->recorder, fd,
				   output->current->stride);
	if (ret < 0) {
		weston_log("[libva recorder] aborted: %m\n");
		recorder_destroy(output);
	}
}

static void *
create_recorder(struct drm_backend *b, int width, int height,
		const char *filename)
{
	int fd;
	drm_magic_t magic;

	fd = open(b->drm.filename, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return NULL;

	drmGetMagic(fd, &magic);
	drmAuthMagic(b->drm.fd, magic);

	return vaapi_recorder_create(fd, width, height, filename);
}

static void
recorder_binding(struct weston_keyboard *keyboard, uint32_t time, uint32_t key,
		 void *data)
{
	struct drm_backend *b = data;
	struct drm_output *output;
	int width, height;

	output = container_of(b->compositor->output_list.next,
			      struct drm_output, base.link);

	if (!output->recorder) {
		if (output->format != GBM_FORMAT_XRGB8888) {
			weston_log("failed to start vaapi recorder: "
				   "output format not supported\n");
			return;
		}

		width = output->base.current_mode->width;
		height = output->base.current_mode->height;

		output->recorder =
			create_recorder(b, width, height, "capture.h264");
		if (!output->recorder) {
			weston_log("failed to create vaapi recorder\n");
			return;
		}

		output->base.disable_planes++;

		output->recorder_frame_listener.notify = recorder_frame_notify;
		wl_signal_add(&output->base.frame_signal,
			      &output->recorder_frame_listener);

		weston_output_schedule_repaint(&output->base);

		weston_log("[libva recorder] initialized\n");
	} else {
		recorder_destroy(output);
	}
}
#else
static void
recorder_binding(struct weston_keyboard *keyboard, uint32_t time, uint32_t key,
		 void *data)
{
	weston_log("Compiled without libva support\n");
}
#endif

static void
switch_to_gl_renderer(struct drm_backend *b)
{
	struct drm_output *output;
	bool dmabuf_support_inited;

	if (!b->use_pixman)
		return;

	dmabuf_support_inited = !!b->compositor->renderer->import_dmabuf;

	weston_log("Switching to GL renderer\n");

	b->gbm = create_gbm_device(b->drm.fd);
	if (!b->gbm) {
		weston_log("Failed to create gbm device. "
			   "Aborting renderer switch\n");
		return;
	}

	wl_list_for_each(output, &b->compositor->output_list, base.link)
		pixman_renderer_output_destroy(&output->base);

	b->compositor->renderer->destroy(b->compositor);

	if (drm_backend_create_gl_renderer(b) < 0) {
		gbm_device_destroy(b->gbm);
		weston_log("Failed to create GL renderer. Quitting.\n");
		/* FIXME: we need a function to shutdown cleanly */
		assert(0);
	}

	wl_list_for_each(output, &b->compositor->output_list, base.link)
		drm_output_init_egl(output, b);

	b->use_pixman = 0;

	if (!dmabuf_support_inited && b->compositor->renderer->import_dmabuf) {
		if (linux_dmabuf_setup(b->compositor) < 0)
			weston_log("Error: initializing dmabuf "
				   "support failed.\n");
	}
}

static void
renderer_switch_binding(struct weston_keyboard *keyboard, uint32_t time,
			uint32_t key, void *data)
{
	struct drm_backend *b =
		(struct drm_backend *) keyboard->seat->compositor;

	switch_to_gl_renderer(b);
}

static struct drm_backend *
drm_backend_create(struct weston_compositor *compositor,
		      struct drm_parameters *param,
		      int *argc, char *argv[],
		      struct weston_config *config)
{
	struct drm_backend *b;
	struct weston_config_section *section;
	struct udev_device *drm_device;
	struct wl_event_loop *loop;
	const char *path;
	uint32_t key;

	weston_log("initializing drm backend\n");

	b = zalloc(sizeof *b);
	if (b == NULL)
		return NULL;

	/*
	 * KMS support for hardware planes cannot properly synchronize
	 * without nuclear page flip. Without nuclear/atomic, hw plane
	 * and cursor plane updates would either tear or cause extra
	 * waits for vblanks which means dropping the compositor framerate
	 * to a fraction. For cursors, it's not so bad, so they are
	 * enabled.
	 *
	 * These can be enabled again when nuclear/atomic support lands.
	 */
	b->sprites_are_broken = 1;
	b->compositor = compositor;

	section = weston_config_get_section(config, "core", NULL, NULL);
	if (get_gbm_format_from_section(section,
					GBM_FORMAT_XRGB8888,
					&b->format) == -1)
		goto err_base;

	b->use_pixman = param->use_pixman;

	/* Check if we run drm-backend using weston-launch */
	compositor->launcher = weston_launcher_connect(compositor, param->tty,
						       param->seat_id, true);
	if (compositor->launcher == NULL) {
		weston_log("fatal: drm backend should be run "
			   "using weston-launch binary or as root\n");
		goto err_compositor;
	}

	b->udev = udev_new();
	if (b->udev == NULL) {
		weston_log("failed to initialize udev context\n");
		goto err_launcher;
	}

	b->session_listener.notify = session_notify;
	wl_signal_add(&compositor->session_signal, &b->session_listener);

	drm_device = find_primary_gpu(b, param->seat_id);
	if (drm_device == NULL) {
		weston_log("no drm device found\n");
		goto err_udev;
	}
	path = udev_device_get_syspath(drm_device);

	if (init_drm(b, drm_device) < 0) {
		weston_log("failed to initialize kms\n");
		goto err_udev_dev;
	}

	if (b->use_pixman) {
		if (init_pixman(b) < 0) {
			weston_log("failed to initialize pixman renderer\n");
			goto err_udev_dev;
		}
	} else {
		if (init_egl(b) < 0) {
			weston_log("failed to initialize egl\n");
			goto err_udev_dev;
		}
	}

	b->base.destroy = drm_destroy;
	b->base.restore = drm_restore;

	b->prev_state = WESTON_COMPOSITOR_ACTIVE;

	for (key = KEY_F1; key < KEY_F9; key++)
		weston_compositor_add_key_binding(compositor, key,
						  MODIFIER_CTRL | MODIFIER_ALT,
						  switch_vt_binding, compositor);

	wl_list_init(&b->sprite_list);
	create_sprites(b);

	if (udev_input_init(&b->input,
			    compositor, b->udev, param->seat_id) < 0) {
		weston_log("failed to create input devices\n");
		goto err_sprite;
	}

	if (create_outputs(b, param->connector, drm_device) < 0) {
		weston_log("failed to create output for %s\n", path);
		goto err_udev_input;
	}

	/* A this point we have some idea of whether or not we have a working
	 * cursor plane. */
	if (!b->cursors_are_broken)
		compositor->capabilities |= WESTON_CAP_CURSOR_PLANE;

	path = NULL;

	loop = wl_display_get_event_loop(compositor->wl_display);
	b->drm_source =
		wl_event_loop_add_fd(loop, b->drm.fd,
				     WL_EVENT_READABLE, on_drm_input, b);

	b->udev_monitor = udev_monitor_new_from_netlink(b->udev, "udev");
	if (b->udev_monitor == NULL) {
		weston_log("failed to intialize udev monitor\n");
		goto err_drm_source;
	}
	udev_monitor_filter_add_match_subsystem_devtype(b->udev_monitor,
							"drm", NULL);
	b->udev_drm_source =
		wl_event_loop_add_fd(loop,
				     udev_monitor_get_fd(b->udev_monitor),
				     WL_EVENT_READABLE, udev_drm_event, b);

	if (udev_monitor_enable_receiving(b->udev_monitor) < 0) {
		weston_log("failed to enable udev-monitor receiving\n");
		goto err_udev_monitor;
	}

	udev_device_unref(drm_device);

	weston_compositor_add_debug_binding(compositor, KEY_O,
					    planes_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_C,
					    planes_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_V,
					    planes_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_Q,
					    recorder_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_W,
					    renderer_switch_binding, b);

	if (compositor->renderer->import_dmabuf) {
		if (linux_dmabuf_setup(compositor) < 0)
			weston_log("Error: initializing dmabuf "
				   "support failed.\n");
	}

	compositor->backend = &b->base;

	return b;

err_udev_monitor:
	wl_event_source_remove(b->udev_drm_source);
	udev_monitor_unref(b->udev_monitor);
err_drm_source:
	wl_event_source_remove(b->drm_source);
err_udev_input:
	udev_input_destroy(&b->input);
err_sprite:
	gbm_device_destroy(b->gbm);
	destroy_sprites(b);
err_udev_dev:
	udev_device_unref(drm_device);
err_launcher:
	weston_launcher_destroy(compositor->launcher);
err_udev:
	udev_unref(b->udev);
err_compositor:
	weston_compositor_shutdown(compositor);
err_base:
	free(b);
	return NULL;
}

WL_EXPORT int
backend_init(struct weston_compositor *compositor, int *argc, char *argv[],
	     struct weston_config *config)
{
	struct drm_backend *b;
	struct drm_parameters param = { 0, };

	const struct weston_option drm_options[] = {
		{ WESTON_OPTION_INTEGER, "connector", 0, &param.connector },
		{ WESTON_OPTION_STRING, "seat", 0, &param.seat_id },
		{ WESTON_OPTION_INTEGER, "tty", 0, &param.tty },
		{ WESTON_OPTION_BOOLEAN, "current-mode", 0, &option_current_mode },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &param.use_pixman },
	};

	param.seat_id = default_seat;

	parse_options(drm_options, ARRAY_LENGTH(drm_options), argc, argv);

	b = drm_backend_create(compositor, &param, argc, argv, config);
	if (b == NULL)
		return -1;
	return 0;
}

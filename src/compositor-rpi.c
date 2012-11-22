/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2012 Raspberry Pi Foundation
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <libudev.h>

#include "config.h"

#ifdef HAVE_BCM_HOST
#  include <bcm_host.h>
#else
#  include "rpi-bcm-stubs.h"
#endif

#include "compositor.h"
#include "gl-renderer.h"
#include "evdev.h"

/*
 * Dispmanx API offers alpha-blended overlays for hardware compositing.
 * The final composite consists of dispmanx elements, and their contents:
 * the dispmanx resource assigned to the element. The elements may be
 * scanned out directly, or composited to a temporary surface, depending on
 * how the firmware decides to handle the scene. Updates to multiple elements
 * may be queued in a single dispmanx update object, resulting in atomic and
 * vblank synchronized display updates.
 *
 * To avoid tearing and display artifacts, the current dispmanx resource in a
 * dispmanx element must not be touched. Therefore each element must be
 * double-buffered, using two resources, the front and the back. The update
 * sequence is:
 * 0. the front resource is already in-use, the back resource is unused
 * 1. write data into the back resource
 * 2. submit an element update, back becomes in-use
 * 3. swap back and front pointers (both are in-use now)
 * 4. wait for update_submit completion, the new back resource becomes unused
 *
 * A resource may be destroyed only, when the update removing the element has
 * completed. Otherwise you risk showing an incomplete composition.
 *
 * The dispmanx element used as the native window for EGL does not need
 * manually allocated resources, EGL does double-buffering internally.
 * Unfortunately it also means, that we cannot alternate between two
 * buffers like the DRM backend does, since we have no control over what
 * resources EGL uses. We are forced to use EGL_BUFFER_PRESERVED as the
 * EGL_SWAP_BEHAVIOR to avoid repainting the whole output every frame.
 *
 * We also cannot bundle eglSwapBuffers into our own display update, which
 * means that Weston's primary plane updates and the overlay updates may
 * happen unsynchronized.
 */

#ifndef ELEMENT_CHANGE_LAYER
/* copied from interface/vmcs_host/vc_vchi_dispmanx.h of userland.git */
#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)
#endif

/* Enabling this debugging incurs a significant performance hit */
#if 0
#define DBG(...) \
	weston_log(__VA_ARGS__)
#else
#define DBG(...) do {} while (0)
#endif

/* If we had a fully featured vc_dispmanx_resource_write_data()... */
/*#define HAVE_RESOURCE_WRITE_DATA_RECT 1*/

struct rpi_compositor;
struct rpi_output;

struct rpi_resource {
	DISPMANX_RESOURCE_HANDLE_T handle;
	int width;
	int height; /* height of the image (valid pixel data) */
	int stride; /* bytes */
	int buffer_height; /* height of the buffer */
	VC_IMAGE_TYPE_T ifmt;
};

struct rpi_element {
	struct wl_list link;
	struct weston_plane plane;
	struct rpi_output *output;

	DISPMANX_ELEMENT_HANDLE_T handle;
	int layer;
	int need_swap;
	int single_buffer;

	struct rpi_resource resources[2];
	struct rpi_resource *front;
	struct rpi_resource *back;
	pixman_region32_t prev_damage;

	struct weston_surface *surface;
	struct wl_listener surface_destroy_listener;
};

struct rpi_flippipe {
	int readfd;
	int writefd;
	struct wl_event_source *source;
};

struct rpi_output {
	struct rpi_compositor *compositor;
	struct weston_output base;
	int single_buffer;

	struct weston_mode mode;
	struct rpi_flippipe flippipe;

	DISPMANX_DISPLAY_HANDLE_T display;
	EGL_DISPMANX_WINDOW_T egl_window;
	DISPMANX_ELEMENT_HANDLE_T egl_element;

	struct wl_list element_list; /* struct rpi_element */
	struct wl_list old_element_list; /* struct rpi_element */
};

struct rpi_seat {
	struct weston_seat base;
	struct wl_list devices_list;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_monitor_source;
	char *seat_id;
};

struct rpi_compositor {
	struct weston_compositor base;
	uint32_t prev_state;

	struct udev *udev;
	struct tty *tty;

	int max_planes; /* per output, really */
	int single_buffer;
};

static inline struct rpi_output *
to_rpi_output(struct weston_output *base)
{
	return container_of(base, struct rpi_output, base);
}

static inline struct rpi_seat *
to_rpi_seat(struct weston_seat *base)
{
	return container_of(base, struct rpi_seat, base);
}

static inline struct rpi_compositor *
to_rpi_compositor(struct weston_compositor *base)
{
	return container_of(base, struct rpi_compositor, base);
}

static const char *
egl_error_string(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
	switch (code) {
	MYERRCODE(EGL_SUCCESS)
	MYERRCODE(EGL_NOT_INITIALIZED)
	MYERRCODE(EGL_BAD_ACCESS)
	MYERRCODE(EGL_BAD_ALLOC)
	MYERRCODE(EGL_BAD_ATTRIBUTE)
	MYERRCODE(EGL_BAD_CONTEXT)
	MYERRCODE(EGL_BAD_CONFIG)
	MYERRCODE(EGL_BAD_CURRENT_SURFACE)
	MYERRCODE(EGL_BAD_DISPLAY)
	MYERRCODE(EGL_BAD_SURFACE)
	MYERRCODE(EGL_BAD_MATCH)
	MYERRCODE(EGL_BAD_PARAMETER)
	MYERRCODE(EGL_BAD_NATIVE_PIXMAP)
	MYERRCODE(EGL_BAD_NATIVE_WINDOW)
	MYERRCODE(EGL_CONTEXT_LOST)
	default:
		return "unknown";
	}
#undef MYERRCODE
}

static void
print_egl_error_state(void)
{
	EGLint code;

	code = eglGetError();
	weston_log("EGL error state: %s (0x%04lx)\n",
		   egl_error_string(code), (long)code);
}

static inline int
int_max(int a, int b)
{
	return a > b ? a : b;
}

static void
rpi_resource_init(struct rpi_resource *resource)
{
	resource->handle = DISPMANX_NO_HANDLE;
}

static void
rpi_resource_release(struct rpi_resource *resource)
{
	if (resource->handle == DISPMANX_NO_HANDLE)
		return;

	vc_dispmanx_resource_delete(resource->handle);
	DBG("resource %p release\n", resource);
	resource->handle = DISPMANX_NO_HANDLE;
}

static int
rpi_resource_realloc(struct rpi_resource *resource, VC_IMAGE_TYPE_T ifmt,
		     int width, int height, int stride, int buffer_height)
{
	uint32_t dummy;

	if (resource->handle != DISPMANX_NO_HANDLE &&
	    resource->width == width &&
	    resource->height == height &&
	    resource->stride == stride &&
	    resource->buffer_height == buffer_height &&
	    resource->ifmt == ifmt)
		return 0;

	rpi_resource_release(resource);

	/* NOTE: if stride is not a multiple of 16 pixels in bytes,
	 * the vc_image_* functions may break. Dispmanx elements
	 * should be fine, though. Buffer_height probably has similar
	 * constraints, too.
	 */
	resource->handle =
		vc_dispmanx_resource_create(ifmt,
					    width | (stride << 16),
					    height | (buffer_height << 16),
					    &dummy);
	if (resource->handle == DISPMANX_NO_HANDLE)
		return -1;

	resource->width = width;
	resource->height = height;
	resource->stride = stride;
	resource->buffer_height = buffer_height;
	resource->ifmt = ifmt;
	DBG("resource %p alloc\n", resource);
	return 0;
}

static VC_IMAGE_TYPE_T
shm_buffer_get_vc_format(struct wl_buffer *buffer)
{
	switch (wl_shm_buffer_get_format(buffer)) {
	case WL_SHM_FORMAT_XRGB8888:
		return VC_IMAGE_XRGB8888;
	case WL_SHM_FORMAT_ARGB8888:
		return VC_IMAGE_ARGB8888;
	default:
		/* invalid format */
		return VC_IMAGE_MIN;
	}
}

static int
rpi_resource_update(struct rpi_resource *resource, struct wl_buffer *buffer,
		    pixman_region32_t *region)
{
	pixman_region32_t write_region;
	pixman_box32_t *r;
	VC_RECT_T rect;
	VC_IMAGE_TYPE_T ifmt;
	uint32_t *pixels;
	int width;
	int height;
	int stride;
	int ret;
#ifdef HAVE_RESOURCE_WRITE_DATA_RECT
	int n;
#endif

	if (!buffer)
		return -1;

	ifmt = shm_buffer_get_vc_format(buffer);
	width = wl_shm_buffer_get_width(buffer);
	height = wl_shm_buffer_get_height(buffer);
	stride = wl_shm_buffer_get_stride(buffer);
	pixels = wl_shm_buffer_get_data(buffer);

	if (rpi_resource_realloc(resource, ifmt, width, height,
				 stride, height) < 0)
		return -1;

	pixman_region32_init(&write_region);
	pixman_region32_intersect_rect(&write_region, region,
				       0, 0, width, height);

#ifdef HAVE_RESOURCE_WRITE_DATA_RECT
	/* XXX: Can this do a format conversion, so that scanout does not have to? */
	r = pixman_region32_rectangles(&write_region, &n);
	while (n--) {
		vc_dispmanx_rect_set(&rect, r[n].x1, r[n].y1,
				     r[n].x2 - r[n].x1, r[n].y2 - r[n].y1);

		ret = vc_dispmanx_resource_write_data_rect(resource->handle,
							   ifmt, stride,
							   pixels, &rect,
							   rect.x, rect.y);
		DBG("%s: %p %ux%u@%u,%u, ret %d\n", __func__, resource,
		    rect.width, rect.height, rect.x, rect.y, ret);
		if (ret)
			break;
	}
#else
	/* vc_dispmanx_resource_write_data() ignores ifmt,
	 * rect.x, rect.width, and uses stride only for computing
	 * the size of the transfer as rect.height * stride.
	 * Therefore we can only write rows starting at x=0.
	 * To be able to write more than one scanline at a time,
	 * the resource must have been created with the same stride
	 * as used here, and we must write full scanlines.
	 */

	r = pixman_region32_extents(&write_region);
	vc_dispmanx_rect_set(&rect, 0, r->y1, width, r->y2 - r->y1);
	ret = vc_dispmanx_resource_write_data(resource->handle, ifmt,
					       stride, pixels, &rect);
	DBG("%s: %p %ux%u@%u,%u, ret %d\n", __func__, resource,
	    width, r->y2 - r->y1, 0, r->y1, ret);
#endif

	pixman_region32_fini(&write_region);

	return ret ? -1 : 0;
}

static void
rpi_element_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct rpi_element *element =
		container_of(listener, struct rpi_element,
			     surface_destroy_listener);

	element->surface = NULL;
}

static struct rpi_element *
rpi_element_create(struct rpi_output *output, struct weston_surface *surface)
{
	struct rpi_element *element;

	element = calloc(1, sizeof *element);
	if (!element)
		return NULL;

	element->output = output;
	element->single_buffer = output->single_buffer;
	element->handle = DISPMANX_NO_HANDLE;
	rpi_resource_init(&element->resources[0]);
	rpi_resource_init(&element->resources[1]);
	element->front = &element->resources[0];

	if (element->single_buffer) {
		element->back = element->front;
	} else {
		element->back = &element->resources[1];
	}

	pixman_region32_init(&element->prev_damage);

	weston_plane_init(&element->plane, floor(surface->geometry.x),
			  floor(surface->geometry.y));

	element->surface = surface;
	element->surface_destroy_listener.notify =
		rpi_element_handle_surface_destroy;
	wl_signal_add(&surface->surface.resource.destroy_signal,
		      &element->surface_destroy_listener);

	wl_list_insert(output->element_list.prev, &element->link);

	return element;
}

static void
rpi_element_destroy(struct rpi_element *element)
{
	struct weston_surface *surface = element->surface;

	if (surface) {
		if (surface->plane == &element->plane) {
			/* If a surface, that was on a plane, gets hidden,
			 * it will not appear in the repaint surface list,
			 * is never considered in rpi_output_assign_planes(),
			 * and hence can stay assigned to this element's plane.
			 * We need to reassign it here.
			 */
			DBG("surface %p (%dx%d@%.1f,%.1f) to primary plane*\n",
			    surface,
			    surface->geometry.width, surface->geometry.height,
			    surface->geometry.x, surface->geometry.y);
			weston_surface_move_to_plane(surface,
				&surface->compositor->primary_plane);
		}
		wl_list_remove(&element->surface_destroy_listener.link);
	}

	wl_list_remove(&element->link);
	weston_plane_release(&element->plane);

	if (element->handle != DISPMANX_NO_HANDLE)
		weston_log("ERROR rpi: destroying on-screen element\n");

	pixman_region32_fini(&element->prev_damage);
	rpi_resource_release(&element->resources[0]);
	rpi_resource_release(&element->resources[1]);
	DBG("element %p destroyed (%u)\n", element, element->handle);

	free(element);
}

static void
rpi_element_reuse(struct rpi_element *element)
{
	wl_list_remove(&element->link);
	wl_list_insert(element->output->element_list.prev, &element->link);
}

static void
rpi_element_schedule_destroy(struct rpi_element *element)
{
	wl_list_remove(&element->link);
	wl_list_insert(element->output->old_element_list.prev,
		       &element->link);
}

static int
rpi_element_damage(struct rpi_element *element, struct wl_buffer *buffer,
		   pixman_region32_t *damage)
{
	pixman_region32_t upload;
	int ret;

	if (!pixman_region32_not_empty(damage))
		return 0;

	DBG("element %p update resource %p\n", element, element->back);

	if (element->single_buffer) {
		ret = rpi_resource_update(element->back, buffer, damage);
	} else {
		pixman_region32_init(&upload);
		pixman_region32_union(&upload, &element->prev_damage, damage);
		ret = rpi_resource_update(element->back, buffer, &upload);
		pixman_region32_fini(&upload);
	}

	pixman_region32_copy(&element->prev_damage, damage);
	element->need_swap = 1;

	return ret;
}

static void
rpi_element_compute_rects(struct rpi_element *element,
			  VC_RECT_T *src_rect, VC_RECT_T *dst_rect)
{
	struct weston_output *output = &element->output->base;
	int src_x, src_y;
	int dst_x, dst_y;
	int width, height;

	/* assume element->plane.{x,y} == element->surface->geometry.{x,y} */
	src_x = 0;
	src_y = 0;
	width = element->surface->geometry.width;
	height = element->surface->geometry.height;

	dst_x = element->plane.x - output->x;
	dst_y = element->plane.y - output->y;

	if (dst_x < 0) {
		width += dst_x;
		src_x -= dst_x;
		dst_x = 0;
	}

	if (dst_y < 0) {
		height += dst_y;
		src_y -= dst_y;
		dst_y = 0;
	}

	width = int_max(width, 0);
	height = int_max(height, 0);

	/* src_rect is in 16.16, dst_rect is in 32.0 unsigned fixed point */
	vc_dispmanx_rect_set(src_rect, src_x << 16, src_y << 16,
			     width << 16, height << 16);
	vc_dispmanx_rect_set(dst_rect, dst_x, dst_y, width, height);
}

static void
rpi_element_dmx_add(struct rpi_element *element,
		    DISPMANX_UPDATE_HANDLE_T update, int layer)
{
	VC_DISPMANX_ALPHA_T alphasetup = {
		DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_PREMULT,
		255, /* opacity 0-255 */
		0 /* mask resource handle */
	};
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	rpi_element_compute_rects(element, &src_rect, &dst_rect);

	element->handle = vc_dispmanx_element_add(
		update,
		element->output->display,
		layer,
		&dst_rect,
		element->back->handle,
		&src_rect,
		DISPMANX_PROTECTION_NONE,
		&alphasetup,
		NULL /* clamp */,
		DISPMANX_NO_ROTATE);
	DBG("element %p add %u\n", element, element->handle);
}

static void
rpi_element_dmx_swap(struct rpi_element *element,
		     DISPMANX_UPDATE_HANDLE_T update)
{
	VC_RECT_T rect;
	pixman_box32_t *r;

	/* XXX: skip, iff resource was not reallocated, and single-buffering */
	vc_dispmanx_element_change_source(update, element->handle,
					  element->back->handle);

	/* This is current damage now, after rpi_assign_plane() */
	r = pixman_region32_extents(&element->prev_damage);

	vc_dispmanx_rect_set(&rect, r->x1, r->y1,
			     r->x2 - r->x1, r->y2 - r->y1);
	vc_dispmanx_element_modified(update, element->handle, &rect);
	DBG("element %p swap\n", element);
}

static void
rpi_element_dmx_move(struct rpi_element *element,
		     DISPMANX_UPDATE_HANDLE_T update, int layer)
{
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	/* XXX: return early, if all attributes stay the same */

	rpi_element_compute_rects(element, &src_rect, &dst_rect);

	vc_dispmanx_element_change_attributes(
		update,
		element->handle,
		ELEMENT_CHANGE_LAYER |
			ELEMENT_CHANGE_DEST_RECT |
			ELEMENT_CHANGE_SRC_RECT,
		layer,
		255,
		&dst_rect,
		&src_rect,
		DISPMANX_NO_HANDLE,
		DISPMANX_NO_ROTATE);
	DBG("element %p move\n", element);
}

static int
rpi_element_update(struct rpi_element *element,
		   DISPMANX_UPDATE_HANDLE_T update, int layer)
{
	struct rpi_resource *tmp;

	if (element->handle == DISPMANX_NO_HANDLE) {
		/* need_swap is already true, see rpi_assign_plane() */

		rpi_element_dmx_add(element, update, layer);
		if (element->handle == DISPMANX_NO_HANDLE)
			weston_log("ERROR rpi: element_add() failed.\n");
	} else {
		if (element->need_swap)
			rpi_element_dmx_swap(element, update);
		rpi_element_dmx_move(element, update, layer);
	}
	element->layer = layer;

	if (element->need_swap) {
		tmp = element->front;
		element->front = element->back;
		element->back = tmp;
		element->need_swap = 0;
		DBG("new back %p, new front %p\n",
		    element->back, element->front);
	}

	return 0;
}

static void
rpi_flippipe_update_complete(DISPMANX_UPDATE_HANDLE_T update, void *data)
{
	/* This function runs in a different thread. */
	struct rpi_flippipe *flippipe = data;
	struct timeval tv;
	uint64_t time;
	ssize_t ret;

	/* manufacture flip completion timestamp */
	/* XXX: use CLOCK_MONOTONIC instead? */
	gettimeofday(&tv, NULL);
	time = (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

	ret = write(flippipe->writefd, &time, sizeof time);
	if (ret != sizeof time)
		weston_log("ERROR: %s failed to write, ret %zd, errno %d\n",
			   __func__, ret, errno);
}

static int
rpi_dispmanx_update_submit(DISPMANX_UPDATE_HANDLE_T update,
			   struct rpi_output *output)
{
	/*
	 * The callback registered here will eventually be called
	 * in a different thread context. Therefore we cannot call
	 * the usual functions from rpi_flippipe_update_complete().
	 * Instead, we have a pipe for passing the message from the
	 * thread, waking up the Weston main event loop, calling
	 * rpi_flippipe_handler(), and then ending up in
	 * rpi_output_update_complete() in the main thread context,
	 * where we can do the frame finishing work.
	 */
	return vc_dispmanx_update_submit(update, rpi_flippipe_update_complete,
					 &output->flippipe);
}

static void
rpi_output_update_complete(struct rpi_output *output, uint64_t time);

static int
rpi_flippipe_handler(int fd, uint32_t mask, void *data)
{
	struct rpi_output *output = data;
	ssize_t ret;
	uint64_t time;

	if (mask != WL_EVENT_READABLE)
		weston_log("ERROR: unexpected mask 0x%x in %s\n",
			   mask, __func__);

	ret = read(fd, &time, sizeof time);
	if (ret != sizeof time) {
		weston_log("ERROR: %s failed to read, ret %zd, errno %d\n",
			   __func__, ret, errno);
	}

	rpi_output_update_complete(output, time);

	return 1;
}

static int
rpi_flippipe_init(struct rpi_flippipe *flippipe, struct rpi_output *output)
{
	struct wl_event_loop *loop;
	int fd[2];

	if (pipe2(fd, O_CLOEXEC) == -1)
		return -1;

	flippipe->readfd = fd[0];
	flippipe->writefd = fd[1];

	loop = wl_display_get_event_loop(output->compositor->base.wl_display);
	flippipe->source = wl_event_loop_add_fd(loop, flippipe->readfd,
						WL_EVENT_READABLE,
						rpi_flippipe_handler, output);

	if (!flippipe->source) {
		close(flippipe->readfd);
		close(flippipe->writefd);
		return -1;
	}

	return 0;
}

static void
rpi_flippipe_release(struct rpi_flippipe *flippipe)
{
	wl_event_source_remove(flippipe->source);
	close(flippipe->readfd);
	close(flippipe->writefd);
}

static struct rpi_element *
find_rpi_element_from_surface(struct weston_surface *surface)
{
	struct wl_listener *listener;
	struct rpi_element *element;

	listener = wl_signal_get(&surface->surface.resource.destroy_signal,
				 rpi_element_handle_surface_destroy);
	if (!listener)
		return NULL;

	element = container_of(listener, struct rpi_element,
			       surface_destroy_listener);

	if (element->surface != surface)
		weston_log("ERROR rpi: sanity check failure in %s.\n",
			   __func__);

	return element;
}

static struct rpi_element *
rpi_assign_plane(struct weston_surface *surface, struct rpi_output *output)
{
	struct rpi_element *element;

	/* dispmanx elements cannot transform */
	if (surface->transform.enabled) {
		/* XXX: inspect the transformation matrix, we might still
		 * be able to put it into an element; scaling, additional
		 * translation (window titlebar context menus?)
		 */
		DBG("surface %p rejected: transform\n", surface);
		return NULL;
	}

	/* only shm surfaces supported */
	if (surface->buffer && !wl_buffer_is_shm(surface->buffer)) {
		DBG("surface %p rejected: not shm\n", surface);
		return NULL;
	}

	/* check if this surface previously belonged to an element */
	element = find_rpi_element_from_surface(surface);

	if (element) {
		rpi_element_reuse(element);
		element->plane.x = floor(surface->geometry.x);
		element->plane.y = floor(surface->geometry.y);
		DBG("surface %p reuse element %p\n", surface, element);
	} else {
		if (!surface->buffer) {
			DBG("surface %p rejected: no buffer\n", surface);
			return NULL;
		}

		element = rpi_element_create(output, surface);
		DBG("element %p created\n", element);
	}

	if (!element) {
		DBG("surface %p rejected: no element\n", surface);
		return NULL;
	}

	return element;
}

static void
rpi_output_assign_planes(struct weston_output *base)
{
	struct rpi_output *output = to_rpi_output(base);
	struct rpi_compositor *compositor = output->compositor;
	struct weston_surface *surface;
	pixman_region32_t overlap;
	pixman_region32_t surface_overlap;
	struct rpi_element *element;
	int n = 0;

	/* Construct the list of rpi_elements to be used into
	 * output->element_list, which is empty right now.
	 * Re-used elements are moved from old_element_list to
	 * element_list. */

	DBG("%s\n", __func__);

	pixman_region32_init(&overlap);
	wl_list_for_each(surface, &compositor->base.surface_list, link) {
		pixman_region32_init(&surface_overlap);
		pixman_region32_intersect(&surface_overlap, &overlap,
					  &surface->transform.boundingbox);

		element = NULL;
		if (!pixman_region32_not_empty(&surface_overlap) &&
		    n < compositor->max_planes)
			element = rpi_assign_plane(surface, output);

		if (element) {
			weston_surface_move_to_plane(surface, &element->plane);
			DBG("surface %p (%dx%d@%.1f,%.1f) to element %p\n",
			    surface,
			    surface->geometry.width, surface->geometry.height,
			    surface->geometry.x, surface->geometry.y, element);

			/* weston_surface_move_to_plane() does full-surface
			 * damage, if the plane is new, so no need to force
			 * initial resource update.
			 */
			if (rpi_element_damage(element, surface->buffer,
					       &surface->damage) < 0) {
				rpi_element_schedule_destroy(element);
				DBG("surface %p rejected: resource update failed\n",
				    surface);
				element = NULL;
			} else {
				n++;
			}
		}

		if (!element) {
			weston_surface_move_to_plane(surface,
					&compositor->base.primary_plane);
			DBG("surface %p (%dx%d@%.1f,%.1f) to primary plane\n",
			    surface,
			    surface->geometry.width, surface->geometry.height,
			    surface->geometry.x, surface->geometry.y);
			pixman_region32_union(&overlap, &overlap,
					      &surface->transform.boundingbox);
		}

		pixman_region32_fini(&surface_overlap);
	}
	pixman_region32_fini(&overlap);
}

static void
rpi_remove_elements(struct wl_list *element_list,
		    DISPMANX_UPDATE_HANDLE_T update)
{
	struct rpi_element *element;

	wl_list_for_each(element, element_list, link) {
		if (element->handle == DISPMANX_NO_HANDLE)
			continue;

		vc_dispmanx_element_remove(update, element->handle);
		DBG("element %p remove %u\n", element, element->handle);
		element->handle = DISPMANX_NO_HANDLE;
	}
}

static void
rpi_output_destroy_old_elements(struct rpi_output *output)
{
	struct rpi_element *element, *tmp;

	wl_list_for_each_safe(element, tmp, &output->old_element_list, link) {
		if (element->handle != DISPMANX_NO_HANDLE)
			continue;

		rpi_element_destroy(element);
	}
}

static void
rpi_output_repaint(struct weston_output *base, pixman_region32_t *damage)
{
	struct rpi_output *output = to_rpi_output(base);
	struct rpi_compositor *compositor = output->compositor;
	struct weston_plane *primary_plane = &compositor->base.primary_plane;
	struct rpi_element *element;
	DISPMANX_UPDATE_HANDLE_T update;
	int layer = 10000;

	DBG("%s\n", __func__);

	update = vc_dispmanx_update_start(0);

	/* update all live elements */
	wl_list_for_each(element, &output->element_list, link) {
		if (rpi_element_update(element, update, layer--) < 0)
			weston_log("ERROR rpi: element update failed.\n");
	}

	/* remove all unused elements */
	rpi_remove_elements(&output->old_element_list, update);

	/* schedule callback to rpi_output_update_complete() */
	rpi_dispmanx_update_submit(update, output);

	/* XXX: if there is anything to composite in GL,
	 * framerate seems to suffer */
	/* XXX: optimise the renderer for the case of nothing to render */
	/* XXX: if nothing to render, remove the element...
	 * but how, is destroying the EGLSurface a bad performance hit?
	 */
	compositor->base.renderer->repaint_output(&output->base, damage);

	pixman_region32_subtract(&primary_plane->damage,
				 &primary_plane->damage, damage);

	/* Move the list of elements into the old_element_list. */
	wl_list_insert_list(&output->old_element_list, &output->element_list);
	wl_list_init(&output->element_list);
}

static void
rpi_output_update_complete(struct rpi_output *output, uint64_t time)
{
	rpi_output_destroy_old_elements(output);
	weston_output_finish_frame(&output->base, time);
}

static void
rpi_output_destroy(struct weston_output *base)
{
	struct rpi_output *output = to_rpi_output(base);
	DISPMANX_UPDATE_HANDLE_T update;
	struct rpi_element *element, *tmp;

	DBG("%s\n", __func__);

	rpi_flippipe_release(&output->flippipe);

	update = vc_dispmanx_update_start(0);
	rpi_remove_elements(&output->element_list, update);
	rpi_remove_elements(&output->old_element_list, update);
	vc_dispmanx_element_remove(update, output->egl_element);
	vc_dispmanx_update_submit_sync(update);

	gl_renderer_output_destroy(base);

	wl_list_for_each_safe(element, tmp, &output->element_list, link)
		rpi_element_destroy(element);

	wl_list_for_each_safe(element, tmp, &output->old_element_list, link)
		rpi_element_destroy(element);

	wl_list_remove(&output->base.link);
	weston_output_destroy(&output->base);

	vc_dispmanx_display_close(output->display);

	free(output);
}

static int
rpi_output_create(struct rpi_compositor *compositor)
{
	struct rpi_output *output;
	DISPMANX_MODEINFO_T modeinfo;
	DISPMANX_UPDATE_HANDLE_T update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	int ret;
	float mm_width, mm_height;
	VC_DISPMANX_ALPHA_T alphasetup = {
		DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,
		255, /* opacity 0-255 */
		0 /* mask resource handle */
	};

	output = calloc(1, sizeof *output);
	if (!output)
		return -1;

	output->compositor = compositor;
	output->single_buffer = compositor->single_buffer;
	wl_list_init(&output->element_list);
	wl_list_init(&output->old_element_list);

	if (rpi_flippipe_init(&output->flippipe, output) < 0) {
		weston_log("Creating message pipe failed.\n");
		goto out_free;
	}

	output->display = vc_dispmanx_display_open(DISPMANX_ID_HDMI);
	if (!output->display) {
		weston_log("Failed to open dispmanx HDMI display.\n");
		goto out_pipe;
	}

	ret = vc_dispmanx_display_get_info(output->display, &modeinfo);
	if (ret < 0) {
		weston_log("Failed to get display mode information.\n");
		goto out_dmx_close;
	}

	vc_dispmanx_rect_set(&dst_rect, 0, 0, modeinfo.width, modeinfo.height);
	vc_dispmanx_rect_set(&src_rect, 0, 0,
			     modeinfo.width << 16, modeinfo.height << 16);

	update = vc_dispmanx_update_start(0);
	output->egl_element = vc_dispmanx_element_add(update,
						      output->display,
						      0 /* layer */,
						      &dst_rect,
						      0 /* src resource */,
						      &src_rect,
						      DISPMANX_PROTECTION_NONE,
						      &alphasetup,
						      NULL /* clamp */,
						      DISPMANX_NO_ROTATE);
	vc_dispmanx_update_submit_sync(update);

	output->egl_window.element = output->egl_element;
	output->egl_window.width = modeinfo.width;
	output->egl_window.height = modeinfo.height;

	output->base.repaint = rpi_output_repaint;
	output->base.destroy = rpi_output_destroy;
	if (compositor->max_planes > 0)
		output->base.assign_planes = rpi_output_assign_planes;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	/* XXX: use tvservice to get information from and control the
	 * HDMI and SDTV outputs. See:
	 * /opt/vc/include/interface/vmcs_host/vc_tvservice.h
	 */

	/* only one static mode in list */
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = modeinfo.width;
	output->mode.height = modeinfo.height;
	output->mode.refresh = 60000;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current = &output->mode;
	output->base.origin = &output->mode;
	output->base.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->base.make = "unknown";
	output->base.model = "unknown";

	/* guess 96 dpi */
	mm_width  = modeinfo.width * (25.4f / 96.0f);
	mm_height = modeinfo.height * (25.4f / 96.0f);

	weston_output_init(&output->base, &compositor->base,
			   0, 0, round(mm_width), round(mm_height),
			   WL_OUTPUT_TRANSFORM_NORMAL);

	if (gl_renderer_output_create(&output->base,
			(EGLNativeWindowType)&output->egl_window) < 0)
		goto out_output;

	if (!eglSurfaceAttrib(gl_renderer_display(&compositor->base),
			     gl_renderer_output_surface(&output->base),
			      EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED)) {
		print_egl_error_state();
		weston_log("Failed to set swap behaviour to preserved.\n");
		goto out_gl;
	}

	wl_list_insert(compositor->base.output_list.prev, &output->base.link);

	weston_log("Raspberry Pi HDMI output %dx%d px\n",
		   output->mode.width, output->mode.height);
	weston_log_continue(STAMP_SPACE "guessing %d Hz and 96 dpi\n",
			    output->mode.refresh / 1000);

	return 0;

out_gl:
	gl_renderer_output_destroy(&output->base);
out_output:
	weston_output_destroy(&output->base);
	update = vc_dispmanx_update_start(0);
	vc_dispmanx_element_remove(update, output->egl_element);
	vc_dispmanx_update_submit_sync(update);

out_dmx_close:
	vc_dispmanx_display_close(output->display);

out_pipe:
	rpi_flippipe_release(&output->flippipe);

out_free:
	free(output);
	return -1;
}

static void
rpi_led_update(struct weston_seat *seat_base, enum weston_led leds)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);
	struct evdev_device *device;

	wl_list_for_each(device, &seat->devices_list, link)
		evdev_led_update(device, leds);
}

static const char default_seat[] = "seat0";

static void
device_added(struct udev_device *udev_device, struct rpi_seat *master)
{
	struct evdev_device *device;
	const char *devnode;
	const char *device_seat;
	int fd;

	device_seat = udev_device_get_property_value(udev_device, "ID_SEAT");
	if (!device_seat)
		device_seat = default_seat;

	if (strcmp(device_seat, master->seat_id))
		return;

	devnode = udev_device_get_devnode(udev_device);

	/* Use non-blocking mode so that we can loop on read on
	 * evdev_device_data() until all events on the fd are
	 * read.  mtdev_get() also expects this. */
	fd = open(devnode, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		weston_log("opening input device '%s' failed.\n", devnode);
		return;
	}

	device = evdev_device_create(&master->base, devnode, fd);
	if (!device) {
		close(fd);
		weston_log("not using input device '%s'.\n", devnode);
		return;
	}

	wl_list_insert(master->devices_list.prev, &device->link);
}

static void
evdev_add_devices(struct udev *udev, struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path, *sysname;

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_scan_devices(e);
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(udev, path);

		sysname = udev_device_get_sysname(device);
		if (strncmp("event", sysname, 5) != 0) {
			udev_device_unref(device);
			continue;
		}

		device_added(device, seat);

		udev_device_unref(device);
	}
	udev_enumerate_unref(e);

	evdev_notify_keyboard_focus(&seat->base, &seat->devices_list);

	if (wl_list_empty(&seat->devices_list)) {
		weston_log(
			"warning: no input devices on entering Weston. "
			"Possible causes:\n"
			"\t- no permissions to read /dev/input/event*\n"
			"\t- seats misconfigured "
			"(Weston backend option 'seat', "
			"udev device property ID_SEAT)\n");
	}
}

static int
evdev_udev_handler(int fd, uint32_t mask, void *data)
{
	struct rpi_seat *seat = data;
	struct udev_device *udev_device;
	struct evdev_device *device, *next;
	const char *action;
	const char *devnode;

	udev_device = udev_monitor_receive_device(seat->udev_monitor);
	if (!udev_device)
		return 1;

	action = udev_device_get_action(udev_device);
	if (!action)
		goto out;

	if (strncmp("event", udev_device_get_sysname(udev_device), 5) != 0)
		goto out;

	if (!strcmp(action, "add")) {
		device_added(udev_device, seat);
	}
	else if (!strcmp(action, "remove")) {
		devnode = udev_device_get_devnode(udev_device);
		wl_list_for_each_safe(device, next, &seat->devices_list, link)
			if (!strcmp(device->devnode, devnode)) {
				weston_log("input device %s, %s removed\n",
					   device->devname, device->devnode);
				evdev_device_destroy(device);
				break;
			}
	}

out:
	udev_device_unref(udev_device);

	return 0;
}

static int
evdev_enable_udev_monitor(struct udev *udev, struct weston_seat *seat_base)
{
	struct rpi_seat *master = to_rpi_seat(seat_base);
	struct wl_event_loop *loop;
	struct weston_compositor *c = master->base.compositor;
	int fd;

	master->udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	if (!master->udev_monitor) {
		weston_log("udev: failed to create the udev monitor\n");
		return 0;
	}

	udev_monitor_filter_add_match_subsystem_devtype(master->udev_monitor,
			"input", NULL);

	if (udev_monitor_enable_receiving(master->udev_monitor)) {
		weston_log("udev: failed to bind the udev monitor\n");
		udev_monitor_unref(master->udev_monitor);
		return 0;
	}

	loop = wl_display_get_event_loop(c->wl_display);
	fd = udev_monitor_get_fd(master->udev_monitor);
	master->udev_monitor_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     evdev_udev_handler, master);
	if (!master->udev_monitor_source) {
		udev_monitor_unref(master->udev_monitor);
		return 0;
	}

	return 1;
}

static void
evdev_disable_udev_monitor(struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);

	if (!seat->udev_monitor)
		return;

	udev_monitor_unref(seat->udev_monitor);
	seat->udev_monitor = NULL;
	wl_event_source_remove(seat->udev_monitor_source);
	seat->udev_monitor_source = NULL;
}

static void
evdev_input_create(struct weston_compositor *c, struct udev *udev,
		   const char *seat_id)
{
	struct rpi_seat *seat;

	seat = malloc(sizeof *seat);
	if (seat == NULL)
		return;

	memset(seat, 0, sizeof *seat);
	weston_seat_init(&seat->base, c);
	seat->base.led_update = rpi_led_update;

	wl_list_init(&seat->devices_list);
	seat->seat_id = strdup(seat_id);
	if (!evdev_enable_udev_monitor(udev, &seat->base)) {
		free(seat->seat_id);
		free(seat);
		return;
	}

	evdev_add_devices(udev, &seat->base);
}

static void
evdev_remove_devices(struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);
	struct evdev_device *device, *next;

	wl_list_for_each_safe(device, next, &seat->devices_list, link)
		evdev_device_destroy(device);

	if (seat->base.seat.keyboard)
		notify_keyboard_focus_out(&seat->base);
}

static void
evdev_input_destroy(struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);

	evdev_remove_devices(seat_base);
	evdev_disable_udev_monitor(&seat->base);

	weston_seat_release(seat_base);
	free(seat->seat_id);
	free(seat);
}

static void
rpi_compositor_destroy(struct weston_compositor *base)
{
	struct rpi_compositor *compositor = to_rpi_compositor(base);
	struct weston_seat *seat, *next;

	wl_list_for_each_safe(seat, next, &compositor->base.seat_list, link)
		evdev_input_destroy(seat);

	/* destroys outputs, too */
	weston_compositor_shutdown(&compositor->base);

	gl_renderer_destroy(&compositor->base);
	tty_destroy(compositor->tty);

	bcm_host_deinit();
	free(compositor);
}

static void
vt_func(struct weston_compositor *base, int event)
{
	struct rpi_compositor *compositor = to_rpi_compositor(base);
	struct weston_seat *seat;
	struct weston_output *output;

	switch (event) {
	case TTY_ENTER_VT:
		weston_log("entering VT\n");
		compositor->base.focus = 1;
		compositor->base.state = compositor->prev_state;
		weston_compositor_damage_all(&compositor->base);
		wl_list_for_each(seat, &compositor->base.seat_list, link) {
			evdev_add_devices(compositor->udev, seat);
			evdev_enable_udev_monitor(compositor->udev, seat);
		}
		break;
	case TTY_LEAVE_VT:
		weston_log("leaving VT\n");
		wl_list_for_each(seat, &compositor->base.seat_list, link) {
			evdev_disable_udev_monitor(seat);
			evdev_remove_devices(seat);
		}

		compositor->base.focus = 0;
		compositor->prev_state = compositor->base.state;
		compositor->base.state = WESTON_COMPOSITOR_SLEEPING;

		/* If we have a repaint scheduled (either from a
		 * pending pageflip or the idle handler), make sure we
		 * cancel that so we don't try to pageflip when we're
		 * vt switched away.  The SLEEPING state will prevent
		 * further attemps at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output,
				 &compositor->base.output_list, link) {
			output->repaint_needed = 0;
		}

		break;
	};
}

static void
rpi_restore(struct weston_compositor *base)
{
	struct rpi_compositor *compositor = to_rpi_compositor(base);

	tty_reset(compositor->tty);
}

static void
switch_vt_binding(struct wl_seat *seat, uint32_t time, uint32_t key, void *data)
{
	struct rpi_compositor *ec = data;

	tty_activate_vt(ec->tty, key - KEY_F1 + 1);
}

struct rpi_parameters {
	int tty;
	int max_planes;
	int single_buffer;
};

static struct weston_compositor *
rpi_compositor_create(struct wl_display *display, int argc, char *argv[],
		      const char *config_file, struct rpi_parameters *param)
{
	struct rpi_compositor *compositor;
	const char *seat = default_seat;
	uint32_t key;
	static const EGLint config_attrs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT |
				  EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	weston_log("initializing Raspberry Pi backend\n");

	compositor = calloc(1, sizeof *compositor);
	if (compositor == NULL)
		return NULL;

	if (weston_compositor_init(&compositor->base, display, argc, argv,
				   config_file) < 0)
		goto out_free;

	compositor->udev = udev_new();
	if (compositor->udev == NULL) {
		weston_log("Failed to initialize udev context.\n");
		goto out_compositor;
	}

	compositor->tty = tty_create(&compositor->base, vt_func, param->tty);
	if (!compositor->tty) {
		weston_log("Failed to initialize tty.\n");
		goto out_udev;
	}

	compositor->base.destroy = rpi_compositor_destroy;
	compositor->base.restore = rpi_restore;

	compositor->base.focus = 1;
	compositor->prev_state = WESTON_COMPOSITOR_ACTIVE;
	compositor->max_planes = int_max(param->max_planes, 0);
	compositor->single_buffer = param->single_buffer;

	weston_log("Maximum number of additional Dispmanx planes: %d\n",
		   compositor->max_planes);
	weston_log("Dispmanx planes are %s buffered.\n",
		   compositor->single_buffer ? "single" : "double");

	for (key = KEY_F1; key < KEY_F9; key++)
		weston_compositor_add_key_binding(&compositor->base, key,
						  MODIFIER_CTRL | MODIFIER_ALT,
						  switch_vt_binding, compositor);

	/*
	 * bcm_host_init() creates threads.
	 * Therefore we must have all signal handlers set and signals blocked
	 * before calling it. Otherwise the signals may end in the bcm
	 * threads and cause the default behaviour there. For instance,
	 * SIGUSR1 used for VT switching caused Weston to terminate there.
	 */
	bcm_host_init();

	if (gl_renderer_create(&compositor->base, EGL_DEFAULT_DISPLAY,
		config_attrs, NULL) < 0)
		goto out_tty;

	if (rpi_output_create(compositor) < 0)
		goto out_gl;

	evdev_input_create(&compositor->base, compositor->udev, seat);

	return &compositor->base;

out_gl:
	gl_renderer_destroy(&compositor->base);

out_tty:
	tty_destroy(compositor->tty);

out_udev:
	udev_unref(compositor->udev);

out_compositor:
	weston_compositor_shutdown(&compositor->base);

out_free:
	bcm_host_deinit();
	free(compositor);

	return NULL;
}

/*
 * If you have a recent enough firmware in Raspberry Pi, that
 * supports falling back to off-line hardware compositing, and
 * you have enabled it with dispmanx_offline=1 in /boot/config.txt,
 * then VideoCore should be able to handle almost 100 Dispmanx
 * elements. Therefore use 80 as the default limit.
 *
 * If you don't have off-line compositing support, this would be
 * better as something like 10. Failing on-line compositing may
 * show up as visible glitches, HDMI blanking, or invisible surfaces.
 *
 * When the max-planes number is reached, rpi-backend will start
 * to fall back to GLESv2 compositing.
 */
#define DEFAULT_MAX_PLANES 80

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[],
	     const char *config_file)
{
	struct rpi_parameters param = {
		.tty = 0, /* default to current tty */
		.max_planes = DEFAULT_MAX_PLANES,
		.single_buffer = 0,
	};

	const struct weston_option rpi_options[] = {
		{ WESTON_OPTION_INTEGER, "tty", 0, &param.tty },
		{ WESTON_OPTION_INTEGER, "max-planes", 0, &param.max_planes },
		{ WESTON_OPTION_BOOLEAN, "single-buffer", 0,
		  &param.single_buffer },
	};

	parse_options(rpi_options, ARRAY_LENGTH(rpi_options), argc, argv);

	return rpi_compositor_create(display, argc, argv, config_file, &param);
}

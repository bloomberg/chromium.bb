/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2014 Jason Ekstrand
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <signal.h>
#include <linux/input.h>
#include <errno.h>

#include <wayland-client.h>

#include "compositor.h"
#include "../shared/os-compatibility.h"
#include "fullscreen-shell-client-protocol.h"

struct shared_output {
	struct weston_output *output;
	struct wl_listener output_destroyed;
	struct wl_list seat_list;

	struct {
		struct wl_display *display;
		struct wl_registry *registry;
		struct wl_compositor *compositor;
		struct wl_shm *shm;
		uint32_t shm_formats;
		struct _wl_fullscreen_shell *fshell;
		struct wl_output *output;
		struct wl_surface *surface;
		struct wl_callback *frame_cb;
		struct _wl_fullscreen_shell_mode_feedback *mode_feedback;
	} parent;

	struct wl_event_source *event_source;
	struct wl_listener frame_listener;

	struct {
		int32_t width, height;

		struct wl_list buffers;
		struct wl_list free_buffers;
	} shm;

	int cache_dirty;
	pixman_image_t *cache_image;
	uint32_t *tmp_data;
	size_t tmp_data_size;
};

struct ss_seat {
	struct weston_seat base;
	struct shared_output *output;
	struct wl_list link;

	struct {
		struct wl_seat *seat;
		struct wl_pointer *pointer;
		struct wl_keyboard *keyboard;
	} parent;

	enum weston_key_state_update keyboard_state_update;
	uint32_t key_serial;
};

struct ss_shm_buffer {
	struct shared_output *output;
	struct wl_list link;
	struct wl_list free_link;

	struct wl_buffer *buffer;
	void *data;
	size_t size;
	pixman_region32_t damage;

	pixman_image_t *pm_image;
};

static void
ss_seat_handle_pointer_enter(void *data, struct wl_pointer *pointer,
			     uint32_t serial, struct wl_surface *surface,
			     wl_fixed_t x, wl_fixed_t y)
{
	struct ss_seat *seat = data;

	/* No transformation of input position is required here because we are
	 * always receiving the input in the same coordinates as the output. */

	notify_pointer_focus(&seat->base, NULL, 0, 0);
}

static void
ss_seat_handle_pointer_leave(void *data, struct wl_pointer *pointer,
			     uint32_t serial, struct wl_surface *surface)
{
	struct ss_seat *seat = data;

	notify_pointer_focus(&seat->base, NULL, 0, 0);
}

static void
ss_seat_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct ss_seat *seat = data;

	/* No transformation of input position is required here because we are
	 * always receiving the input in the same coordinates as the output. */

	notify_motion_absolute(&seat->base, time, x, y);
}

static void
ss_seat_handle_button(void *data, struct wl_pointer *pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct ss_seat *seat = data;

	notify_button(&seat->base, time, button, state);
}

static void
ss_seat_handle_axis(void *data, struct wl_pointer *pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
	struct ss_seat *seat = data;

	notify_axis(&seat->base, time, axis, value);
}

static const struct wl_pointer_listener ss_seat_pointer_listener = {
	ss_seat_handle_pointer_enter,
	ss_seat_handle_pointer_leave,
	ss_seat_handle_motion,
	ss_seat_handle_button,
	ss_seat_handle_axis,
};

static void
ss_seat_handle_keymap(void *data, struct wl_keyboard *keyboard,
		      uint32_t format, int fd, uint32_t size)
{
	struct ss_seat *seat = data;
	struct xkb_keymap *keymap;
	char *map_str;

	if (!data)
		goto error;

	if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		if (map_str == MAP_FAILED) {
			weston_log("mmap failed: %m\n");
			goto error;
		}

		keymap = xkb_map_new_from_string(seat->base.compositor->xkb_context,
						 map_str,
						 XKB_KEYMAP_FORMAT_TEXT_V1,
						 0);
		munmap(map_str, size);

		if (!keymap) {
			weston_log("failed to compile keymap\n");
			goto error;
		}

		seat->keyboard_state_update = STATE_UPDATE_NONE;
	} else if (format == WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP) {
		weston_log("No keymap provided; falling back to default\n");
		keymap = NULL;
		seat->keyboard_state_update = STATE_UPDATE_AUTOMATIC;
	} else {
		weston_log("Invalid keymap\n");
		goto error;
	}

	close(fd);

	if (seat->base.keyboard)
		weston_seat_update_keymap(&seat->base, keymap);
	else
		weston_seat_init_keyboard(&seat->base, keymap);

	if (keymap)
		xkb_map_unref(keymap);

	return;

error:
	wl_keyboard_release(seat->parent.keyboard);
	close(fd);
}

static void
ss_seat_handle_keyboard_enter(void *data, struct wl_keyboard *keyboard,
			      uint32_t serial, struct wl_surface *surface,
			      struct wl_array *keys)
{
	struct ss_seat *seat = data;

	/* XXX: If we get a modifier event immediately before the focus,
	 *      we should try to keep the same serial. */
	notify_keyboard_focus_in(&seat->base, keys,
				 STATE_UPDATE_AUTOMATIC);
}

static void
ss_seat_handle_keyboard_leave(void *data, struct wl_keyboard *keyboard,
			      uint32_t serial, struct wl_surface *surface)
{
	struct ss_seat *seat = data;

	notify_keyboard_focus_out(&seat->base);
}

static void
ss_seat_handle_key(void *data, struct wl_keyboard *keyboard,
		   uint32_t serial, uint32_t time,
		   uint32_t key, uint32_t state)
{
	struct ss_seat *seat = data;

	seat->key_serial = serial;
	notify_key(&seat->base, time, key,
		   state ? WL_KEYBOARD_KEY_STATE_PRESSED :
			   WL_KEYBOARD_KEY_STATE_RELEASED,
		   seat->keyboard_state_update);
}

static void
ss_seat_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			 uint32_t serial_in, uint32_t mods_depressed,
			 uint32_t mods_latched, uint32_t mods_locked,
			 uint32_t group)
{
	struct ss_seat *seat = data;
	struct weston_compositor *c = seat->output->output->compositor;
	uint32_t serial_out;

	/* If we get a key event followed by a modifier event with the
	 * same serial number, then we try to preserve those semantics by
	 * reusing the same serial number on the way out too. */
	if (serial_in == seat->key_serial)
		serial_out = wl_display_get_serial(c->wl_display);
	else
		serial_out = wl_display_next_serial(c->wl_display);

	xkb_state_update_mask(seat->base.keyboard->xkb_state.state,
			      mods_depressed, mods_latched,
			      mods_locked, 0, 0, group);
	notify_modifiers(&seat->base, serial_out);
}

static const struct wl_keyboard_listener ss_seat_keyboard_listener = {
	ss_seat_handle_keymap,
	ss_seat_handle_keyboard_enter,
	ss_seat_handle_keyboard_leave,
	ss_seat_handle_key,
	ss_seat_handle_modifiers,
};

static void
ss_seat_handle_capabilities(void *data, struct wl_seat *seat,
			    enum wl_seat_capability caps)
{
	struct ss_seat *ss_seat = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !ss_seat->parent.pointer) {
		ss_seat->parent.pointer = wl_seat_get_pointer(seat);
		wl_pointer_set_user_data(ss_seat->parent.pointer, ss_seat);
		wl_pointer_add_listener(ss_seat->parent.pointer,
					&ss_seat_pointer_listener, ss_seat);
		weston_seat_init_pointer(&ss_seat->base);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && ss_seat->parent.pointer) {
		wl_pointer_destroy(ss_seat->parent.pointer);
		ss_seat->parent.pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !ss_seat->parent.keyboard) {
		ss_seat->parent.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_set_user_data(ss_seat->parent.keyboard, ss_seat);
		wl_keyboard_add_listener(ss_seat->parent.keyboard,
					 &ss_seat_keyboard_listener, ss_seat);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && ss_seat->parent.keyboard) {
		wl_keyboard_destroy(ss_seat->parent.keyboard);
		ss_seat->parent.keyboard = NULL;
	}
}

static const struct wl_seat_listener ss_seat_listener = {
	ss_seat_handle_capabilities,
};

static struct ss_seat *
ss_seat_create(struct shared_output *so, uint32_t id)
{
	struct ss_seat *seat;

	seat = zalloc(sizeof *seat);
	if (seat == NULL)
		return NULL;

	weston_seat_init(&seat->base, so->output->compositor, "default");
	seat->output = so;
	seat->parent.seat = wl_registry_bind(so->parent.registry, id,
					     &wl_seat_interface, 1);
	wl_list_insert(so->seat_list.prev, &seat->link);

	wl_seat_add_listener(seat->parent.seat, &ss_seat_listener, seat);
	wl_seat_set_user_data(seat->parent.seat, seat);

	return seat;
}

static void
ss_seat_destroy(struct ss_seat *seat)
{
	if (seat->parent.pointer)
		wl_pointer_release(seat->parent.pointer);
	if (seat->parent.keyboard)
		wl_keyboard_release(seat->parent.keyboard);
	wl_seat_destroy(seat->parent.seat);

	wl_list_remove(&seat->link);

	weston_seat_release(&seat->base);

	free(seat);
}

static void
ss_shm_buffer_destroy(struct ss_shm_buffer *buffer)
{
	pixman_image_unref(buffer->pm_image);

	wl_buffer_destroy(buffer->buffer);
	munmap(buffer->data, buffer->size);

	pixman_region32_fini(&buffer->damage);

	wl_list_remove(&buffer->link);
	wl_list_remove(&buffer->free_link);
	free(buffer);
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct ss_shm_buffer *sb = data;

	if (sb->output) {
		wl_list_insert(&sb->output->shm.free_buffers, &sb->free_link);
	} else {
		ss_shm_buffer_destroy(sb);
	}
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static struct ss_shm_buffer *
shared_output_get_shm_buffer(struct shared_output *so)
{
	struct ss_shm_buffer *sb, *bnext;
	struct wl_shm_pool *pool;
	int width, height, stride;
	int fd;
	unsigned char *data;

	width = so->output->width;
	height = so->output->height;
	stride = width * 4;

	/* If the size of the output changed, we free the old buffers and
	 * make new ones. */
	if (so->shm.width != width ||
	    so->shm.height != height) {

		/* Destroy free buffers */
		wl_list_for_each_safe(sb, bnext, &so->shm.free_buffers, link)
			ss_shm_buffer_destroy(sb);

		/* Orphan in-use buffers so they get destroyed */
		wl_list_for_each(sb, &so->shm.buffers, link)
			sb->output = NULL;

		so->shm.width = width;
		so->shm.height = height;
	}

	if (!wl_list_empty(&so->shm.free_buffers)) {
		sb = container_of(so->shm.free_buffers.next,
				  struct ss_shm_buffer, free_link);
		wl_list_remove(&sb->free_link);
		wl_list_init(&sb->free_link);

		return sb;
	}

	fd = os_create_anonymous_file(height * stride);
	if (fd < 0) {
		weston_log("os_create_anonymous_file: %m");
		return NULL;
	}

	data = mmap(NULL, height * stride, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		weston_log("mmap: %m");
		close(fd);
		return NULL;
	}

	sb = zalloc(sizeof *sb);

	sb->output = so;
	wl_list_init(&sb->free_link);
	wl_list_insert(&so->shm.buffers, &sb->link);

	pixman_region32_init_rect(&sb->damage, 0, 0, width, height);

	sb->data = data;
	sb->size = height * stride;

	pool = wl_shm_create_pool(so->parent.shm, fd, sb->size);

	sb->buffer = wl_shm_pool_create_buffer(pool, 0,
					       width, height, stride,
					       WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(sb->buffer, &buffer_listener, sb);
	wl_shm_pool_destroy(pool);
	close(fd);

	memset(data, 0, sb->size);

	sb->pm_image =
		pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height,
					 (uint32_t *)data, stride);

	return sb;
}

static void
output_compute_transform(struct weston_output *output,
			 pixman_transform_t *transform)
{
	pixman_fixed_t fw, fh;

	pixman_transform_init_identity(transform);

	fw = pixman_int_to_fixed(output->width);
	fh = pixman_int_to_fixed(output->height);

	switch (output->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		pixman_transform_scale(transform, NULL,
				       pixman_int_to_fixed (-1),
				       pixman_int_to_fixed (1));
		pixman_transform_translate(transform, NULL, fw, 0);
	}

	switch (output->transform) {
	default:
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		break;
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		pixman_transform_rotate(transform, NULL, 0, pixman_fixed_1);
		pixman_transform_translate(transform, NULL, fh, 0);
		break;
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		pixman_transform_rotate(transform, NULL, -pixman_fixed_1, 0);
		pixman_transform_translate(transform, NULL, fw, fh);
		break;
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		pixman_transform_rotate(transform, NULL, 0, -pixman_fixed_1);
		pixman_transform_translate(transform, NULL, 0, fw);
		break;
	}

	pixman_transform_scale(transform, NULL,
			       pixman_fixed_1 * output->current_scale,
			       pixman_fixed_1 * output->current_scale);
}

static void
shared_output_destroy(struct shared_output *so);

static int
shared_output_ensure_tmp_data(struct shared_output *so,
			      pixman_region32_t *region)
{
	pixman_box32_t *ext;
	size_t size;

	if (!pixman_region32_not_empty(region))
		return 0;

	ext = pixman_region32_extents(region);

	/* Damage is in output coordinates.
	 *
	 * We are multiplying by 4 because the temporary data needs to be able
	 * to store an 32 bit-per-pixel buffer.
	 */
	size = 4 * (ext->x2 - ext->x1) * (ext->y2 - ext->y1)
		 * so->output->current_scale * so->output->current_scale;

	if (so->tmp_data != NULL && size <= so->tmp_data_size)
		return 0;

	free(so->tmp_data);
	so->tmp_data = malloc(size);
	if (so->tmp_data == NULL) {
		so->tmp_data_size = 0;
		errno = ENOMEM;
		return -1;
	}

	so->tmp_data_size = size;

	return 0;
}

static void
shared_output_update(struct shared_output *so);

static void
shared_output_frame_callback(void *data, struct wl_callback *cb, uint32_t time)
{
	struct shared_output *so = data;

	if (cb != so->parent.frame_cb)
		return;

	wl_callback_destroy(cb);
	so->parent.frame_cb = NULL;

	shared_output_update(so);
}

static const struct wl_callback_listener shared_output_frame_listener = {
	shared_output_frame_callback
};

static void
shared_output_update(struct shared_output *so)
{
	struct ss_shm_buffer *sb;
	pixman_box32_t *r;
	int i, nrects;
	pixman_transform_t transform;

	/* Only update if we need to */
	if (!so->cache_dirty || so->parent.frame_cb)
		return;

	sb = shared_output_get_shm_buffer(so);
	if (sb == NULL) {
		shared_output_destroy(so);
		return;
	}

	output_compute_transform(so->output, &transform);
	pixman_image_set_transform(so->cache_image, &transform);

	pixman_image_set_clip_region32(sb->pm_image, &sb->damage);

	if (so->output->current_scale == 1) {
		pixman_image_set_filter(so->cache_image,
					PIXMAN_FILTER_NEAREST, NULL, 0);
	} else {
		pixman_image_set_filter(so->cache_image,
					PIXMAN_FILTER_BILINEAR, NULL, 0);
	}

	pixman_image_composite32(PIXMAN_OP_SRC,
				 so->cache_image, /* src */
				 NULL, /* mask */
				 sb->pm_image, /* dest */
				 0, 0, /* src_x, src_y */
				 0, 0, /* mask_x, mask_y */
				 0, 0, /* dest_x, dest_y */
				 so->output->width, /* width */
				 so->output->height /* height */);

	pixman_image_set_transform(sb->pm_image, NULL);
	pixman_image_set_clip_region32(sb->pm_image, NULL);

	r = pixman_region32_rectangles(&sb->damage, &nrects);
	for (i = 0; i < nrects; ++i)
		wl_surface_damage(so->parent.surface, r[i].x1, r[i].y1,
				  r[i].x2 - r[i].x1, r[i].y2 - r[i].y1);

	wl_surface_attach(so->parent.surface, sb->buffer, 0, 0);

	so->parent.frame_cb = wl_surface_frame(so->parent.surface);
	wl_callback_add_listener(so->parent.frame_cb,
				 &shared_output_frame_listener, so);

	wl_surface_commit(so->parent.surface);
	wl_callback_destroy(wl_display_sync(so->parent.display));
	wl_display_flush(so->parent.display);

	/* Clear the buffer damage */
	pixman_region32_fini(&sb->damage);
	pixman_region32_init(&sb->damage);
}

static void
shm_handle_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	struct shared_output *so = data;

	so->parent.shm_formats |= (1 << format);
}

struct wl_shm_listener shm_listener = {
	shm_handle_format
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	struct shared_output *so = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		so->parent.compositor =
			wl_registry_bind(registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0 && !so->parent.output) {
		so->parent.output =
			wl_registry_bind(registry,
					 id, &wl_output_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		ss_seat_create(so, id);
	} else if (strcmp(interface, "wl_shm") == 0) {
		so->parent.shm =
			wl_registry_bind(registry,
					 id, &wl_shm_interface, 1);
		wl_shm_add_listener(so->parent.shm, &shm_listener, so);
	} else if (strcmp(interface, "_wl_fullscreen_shell") == 0) {
		so->parent.fshell =
			wl_registry_bind(registry,
					 id, &_wl_fullscreen_shell_interface, 1);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static int
shared_output_handle_event(int fd, uint32_t mask, void *data)
{
	struct shared_output *so = data;
	int count = 0;

	if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
		shared_output_destroy(so);
		return 0;
	}

	if (mask & WL_EVENT_READABLE)
		count = wl_display_dispatch(so->parent.display);
	if (mask & WL_EVENT_WRITABLE)
		wl_display_flush(so->parent.display);

	if (mask == 0) {
		count = wl_display_dispatch_pending(so->parent.display);
		wl_display_flush(so->parent.display);
	}

	return count;
}

static void
output_destroyed(struct wl_listener *l, void *data)
{
	struct shared_output *so;

	so = container_of(l, struct shared_output, output_destroyed);

	shared_output_destroy(so);
}

static void
mode_feedback_ok(void *data, struct _wl_fullscreen_shell_mode_feedback *fb)
{
	struct shared_output *so = data;

	_wl_fullscreen_shell_mode_feedback_destroy(so->parent.mode_feedback);
}

static void
mode_feedback_failed(void *data, struct _wl_fullscreen_shell_mode_feedback *fb)
{
	struct shared_output *so = data;

	_wl_fullscreen_shell_mode_feedback_destroy(so->parent.mode_feedback);

	weston_log("Screen share failed: present_surface_for_mode failed\n");
	shared_output_destroy(so);
}

struct _wl_fullscreen_shell_mode_feedback_listener mode_feedback_listener = {
	mode_feedback_ok,
	mode_feedback_failed,
	mode_feedback_ok,
};

static void
shared_output_repainted(struct wl_listener *listener, void *data)
{
	struct shared_output *so =
		container_of(listener, struct shared_output, frame_listener);
	pixman_region32_t damage;
	struct ss_shm_buffer *sb;
	int32_t x, y, width, height, stride;
	int i, nrects, do_yflip;
	pixman_box32_t *r;
	uint32_t *cache_data;

	/* Damage in output coordinates */
	pixman_region32_init(&damage);
	pixman_region32_intersect(&damage, &so->output->region,
				  &so->output->previous_damage);
	pixman_region32_translate(&damage, -so->output->x, -so->output->y);

	/* Apply damage to all buffers */
	wl_list_for_each(sb, &so->shm.buffers, link)
		pixman_region32_union(&sb->damage, &sb->damage, &damage);

	/* Transform to buffer coordinates */
	weston_transformed_region(so->output->width, so->output->height,
				  so->output->transform,
				  so->output->current_scale,
				  &damage, &damage);

	width = so->output->current_mode->width;
	height = so->output->current_mode->height;
	stride = width;

	if (!so->cache_image ||
	    pixman_image_get_width(so->cache_image) != width ||
	    pixman_image_get_height(so->cache_image) != height) {
		if (so->cache_image)
			pixman_image_unref(so->cache_image);

		so->cache_image =
			pixman_image_create_bits(PIXMAN_a8r8g8b8,
						 width, height, NULL,
						 stride);
		if (!so->cache_image) {
			shared_output_destroy(so);
			return;
		}

		pixman_region32_fini(&damage);
		pixman_region32_init_rect(&damage, 0, 0, width, height);
	}

	if (shared_output_ensure_tmp_data(so, &damage) < 0) {
		shared_output_destroy(so);
		return;
	}

	do_yflip = !!(so->output->compositor->capabilities & WESTON_CAP_CAPTURE_YFLIP);

	cache_data = pixman_image_get_data(so->cache_image);
	r = pixman_region32_rectangles(&damage, &nrects);
	for (i = 0; i < nrects; ++i) {
		x = r[i].x1;
		y = r[i].y1;
		width = r[i].x2 - r[i].x1;
		height = r[i].y2 - r[i].y1;

		if (do_yflip) {
			so->output->compositor->renderer->read_pixels(
				so->output, PIXMAN_a8r8g8b8, so->tmp_data,
				x, so->output->current_mode->height - r[i].y2,
				width, height);

			pixman_blt(so->tmp_data, cache_data, -width, stride,
				   32, 32, 0, 1 - height, x, y, width, height);
		} else {
			so->output->compositor->renderer->read_pixels(
				so->output, PIXMAN_a8r8g8b8, so->tmp_data,
				x, y, width, height);

			pixman_blt(so->tmp_data, cache_data, width, stride,
				   32, 32, 0, 0, x, y, width, height);
		}
	}

	pixman_region32_fini(&damage);

	so->cache_dirty = 1;

	shared_output_update(so);
}

static struct shared_output *
shared_output_create(struct weston_output *output, int parent_fd)
{
	struct shared_output *so;
	struct wl_event_loop *loop;
	struct ss_seat *seat;
	int epoll_fd;

	so = zalloc(sizeof *so);
	if (so == NULL)
		goto err_close;

	wl_list_init(&so->seat_list);

	so->parent.display = wl_display_connect_to_fd(parent_fd);
	if (!so->parent.display)
		goto err_alloc;

	so->parent.registry = wl_display_get_registry(so->parent.display);
	if (!so->parent.registry)
		goto err_display;
	wl_registry_add_listener(so->parent.registry,
				 &registry_listener, so);
	wl_display_roundtrip(so->parent.display);
	if (so->parent.shm == NULL) {
		weston_log("Screen share failed: No wl_shm found\n");
		goto err_display;
	}
	if (so->parent.fshell == NULL) {
		weston_log("Screen share failed: "
			   "Parent does not support wl_fullscreen_shell\n");
		goto err_display;
	}
	if (so->parent.compositor == NULL) {
		weston_log("Screen share failed: No wl_compositor found\n");
		goto err_display;
	}

	/* Get SHM formats */
	wl_display_roundtrip(so->parent.display);
	if (!(so->parent.shm_formats & (1 << WL_SHM_FORMAT_XRGB8888))) {
		weston_log("Screen share failed: "
			   "WL_SHM_FORMAT_XRGB8888 not available\n");
		goto err_display;
	}

	so->parent.surface =
		wl_compositor_create_surface(so->parent.compositor);
	if (!so->parent.surface) {
		weston_log("Screen share failed: %m");
		goto err_display;
	}

	so->parent.mode_feedback =
		_wl_fullscreen_shell_present_surface_for_mode(so->parent.fshell,
							      so->parent.surface,
							      so->parent.output,
							      output->current_mode->refresh);
	if (!so->parent.mode_feedback) {
		weston_log("Screen share failed: %m");
		goto err_display;
	}
	_wl_fullscreen_shell_mode_feedback_add_listener(so->parent.mode_feedback,
						        &mode_feedback_listener,
						        so);

	loop = wl_display_get_event_loop(output->compositor->wl_display);

	epoll_fd = wl_display_get_fd(so->parent.display);
	so->event_source =
		wl_event_loop_add_fd(loop, epoll_fd, WL_EVENT_READABLE,
				     shared_output_handle_event, so);
	if (!so->event_source) {
		weston_log("Screen share failed: %m");
		goto err_display;
	}

	/* Ok, everything's created.  We should be good to go */
	wl_list_init(&so->shm.buffers);
	wl_list_init(&so->shm.free_buffers);

	so->output = output;
	so->output_destroyed.notify = output_destroyed;
	wl_signal_add(&so->output->destroy_signal, &so->output_destroyed);

	so->frame_listener.notify = shared_output_repainted;
	wl_signal_add(&output->frame_signal, &so->frame_listener);
	output->disable_planes++;
	weston_output_damage(output);

	return so;

err_display:
	wl_list_for_each(seat, &so->seat_list, link)
		ss_seat_destroy(seat);
	wl_display_disconnect(so->parent.display);
err_alloc:
	free(so);
err_close:
	close(parent_fd);
	return NULL;
}

static void
shared_output_destroy(struct shared_output *so)
{
	struct ss_shm_buffer *buffer, *bnext;

	so->output->disable_planes--;

	wl_list_for_each_safe(buffer, bnext, &so->shm.buffers, link)
		ss_shm_buffer_destroy(buffer);
	wl_list_for_each_safe(buffer, bnext, &so->shm.free_buffers, link)
		ss_shm_buffer_destroy(buffer);

	wl_display_disconnect(so->parent.display);
	wl_event_source_remove(so->event_source);

	wl_list_remove(&so->output_destroyed.link);
	wl_list_remove(&so->frame_listener.link);

	pixman_image_unref(so->cache_image);
	free(so->tmp_data);

	free(so);
}

static struct shared_output *
weston_output_share(struct weston_output *output,
		    const char *path, char *const argv[])
{
	int sv[2];
	char str[32];
	pid_t pid;
	sigset_t allsigs;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		weston_log("weston_output_share: socketpair failed: %m\n");
		return NULL;
	}

	pid = fork();

	if (pid == -1) {
		close(sv[0]);
		close(sv[1]);
		weston_log("weston_output_share: fork failed: %m\n");
		return NULL;
	}

	if (pid == 0) {
		/* We don't want anything circular */
		unsetenv("WAYLAND_DISPLAY");
		unsetenv("WAYLAND_SOCKET");

		/* do not give our signal mask to the new process */
		sigfillset(&allsigs);
		sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

		/* Launch clients as the user. Do not launch clients with
		 * wrong euid. */
		if (seteuid(getuid()) == -1) {
			weston_log("weston_output_share: setuid failed: %m\n");
			abort();
		}

		sv[1] = dup(sv[1]);
		if (sv[1] == -1) {
			weston_log("weston_output_share: dup failed: %m\n");
			abort();
		}

		snprintf(str, sizeof str, "%d", sv[1]);
		setenv("WAYLAND_SERVER_SOCKET", str, 1);

		execv(path, argv);
		weston_log("weston_output_share: exec failed: %m\n");
		abort();
	} else {
		close(sv[1]);
		return shared_output_create(output, sv[0]);
	}

	return NULL;
}

static struct weston_output *
weston_output_find(struct weston_compositor *c, int32_t x, int32_t y)
{
	struct weston_output *output;

	wl_list_for_each(output, &c->output_list, link) {
		if (x >= output->x && y >= output->y &&
		    x < output->x + output->width &&
		    y < output->y + output->height)
			return output;
	}

	return NULL;
}

static void
share_output_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
		     void *data)
{
	struct weston_output *output;
	const char *path = BINDIR "/weston";

	if (!seat->pointer) {
		weston_log("Cannot pick output: Seat does not have pointer\n");
		return;
	}

	output = weston_output_find(seat->compositor,
				    wl_fixed_to_int(seat->pointer->x),
				    wl_fixed_to_int(seat->pointer->y));
	if (!output) {
		weston_log("Cannot pick output: Pointer not on any output\n");
		return;
	}

	char *const argv[] = {
		"weston",
		"--backend=rdp-backend.so",
		"--shell=fullscreen-shell.so",
		"--no-clients-resize",
		NULL
	};

	weston_output_share(output, path, argv);
}

WL_EXPORT int
module_init(struct weston_compositor *compositor,
	    int *argc, char *argv[])
{
	weston_compositor_add_key_binding(compositor, KEY_S,
				          MODIFIER_CTRL | MODIFIER_ALT,
					  share_output_binding, compositor);
	return 0;
}

/*
 * Copyright © 2010-2011 Benjamin Franzke
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include "compositor.h"
#include "gl-renderer.h"
#include "../shared/image-loader.h"
#include "../shared/os-compatibility.h"

struct wayland_compositor {
	struct weston_compositor	 base;

	struct {
		struct wl_display *wl_display;
		struct wl_registry *registry;
		struct wl_compositor *compositor;
		struct wl_shell *shell;
		struct wl_output *output;
		struct wl_shm *shm;

		struct {
			int32_t x, y, width, height;
		} screen_allocation;

		struct wl_event_source *wl_source;
		uint32_t event_mask;
	} parent;

	struct {
		int32_t top, bottom, left, right;
	} border;

	struct wl_list input_list;
};

struct wayland_output {
	struct weston_output	base;
	struct {
		int 			 draw_initial_frame;
		struct wl_surface	*surface;
		struct wl_shell_surface	*shell_surface;
		struct wl_egl_window	*egl_window;
	} parent;
	struct weston_mode	mode;
};

struct wayland_input {
	struct weston_seat base;
	struct wayland_compositor *compositor;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;
	struct wl_list link;
	uint32_t key_serial;
	uint32_t enter_serial;
	int focus;
	struct wayland_output *output;
};

static void
create_border(struct wayland_compositor *c)
{
	pixman_image_t *image;
	int32_t edges[4];

	image = load_image(DATADIR "/weston/border.png");
	if (!image) {
		weston_log("could'nt load border image\n");
		return;
	}

	edges[0] = c->border.left;
	edges[1] = c->border.right;
	edges[2] = c->border.top;
	edges[3] = c->border.bottom;

	gl_renderer_set_border(&c->base, pixman_image_get_width(image),
		pixman_image_get_height(image),
		pixman_image_get_data(image), edges);

	pixman_image_unref(image);
}

static void
frame_done(void *data, struct wl_callback *callback, uint32_t time)
{
	struct weston_output *output = data;

	wl_callback_destroy(callback);
	weston_output_finish_frame(output, time);
}

static const struct wl_callback_listener frame_listener = {
	frame_done
};

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static void
draw_initial_frame(struct wayland_output *output)
{
	struct wayland_compositor *c =
		(struct wayland_compositor *) output->base.compositor;
	struct wl_shm *shm = c->parent.shm;
	struct wl_surface *surface = output->parent.surface;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;

	int width, height, stride;
	int size;
	int fd;
	void *data;

	width = output->mode.width + c->border.left + c->border.right;
	height = output->mode.height + c->border.top + c->border.bottom;
	stride = width * 4;
	size = height * stride;

	fd = os_create_anonymous_file(size);
	if (fd < 0) {
		perror("os_create_anonymous_file");
		return;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return;
	}

	pool = wl_shm_create_pool(shm, fd, size);

	buffer = wl_shm_pool_create_buffer(pool, 0,
					   width, height,
					   stride,
					   WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(buffer, &buffer_listener, buffer);
	wl_shm_pool_destroy(pool);
	close(fd);

	memset(data, 0, size);

	wl_surface_attach(surface, buffer, 0, 0);

	/* We only need to damage some part, as its only transparant
	 * pixels anyway. */
	wl_surface_damage(surface, 0, 0, 1, 1);
}

static void
wayland_output_start_repaint_loop(struct weston_output *output_base)
{
	struct wayland_output *output = (struct wayland_output *) output_base;
	struct wl_callback *callback;

	/* If this is the initial frame, we need to attach a buffer so that
	 * the compositor can map the surface and include it in its render
	 * loop. If the surface doesn't end up in the render loop, the frame
	 * callback won't be invoked. The buffer is transparent and of the
	 * same size as the future real output buffer. */
	if (output->parent.draw_initial_frame) {
		output->parent.draw_initial_frame = 0;

		draw_initial_frame(output);
	}

	callback = wl_surface_frame(output->parent.surface);
	wl_callback_add_listener(callback, &frame_listener, output);
	wl_surface_commit(output->parent.surface);
}

static void
wayland_output_repaint(struct weston_output *output_base,
		       pixman_region32_t *damage)
{
	struct wayland_output *output = (struct wayland_output *) output_base;
	struct weston_compositor *ec = output->base.compositor;
	struct wl_callback *callback;

	callback = wl_surface_frame(output->parent.surface);
	wl_callback_add_listener(callback, &frame_listener, output);

	ec->renderer->repaint_output(&output->base, damage);

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, damage);

}

static void
wayland_output_destroy(struct weston_output *output_base)
{
	struct wayland_output *output = (struct wayland_output *) output_base;

	gl_renderer_output_destroy(output_base);

	wl_egl_window_destroy(output->parent.egl_window);
	free(output);

	return;
}

static const struct wl_shell_surface_listener shell_surface_listener;

static int
wayland_compositor_create_output(struct wayland_compositor *c,
				 int width, int height)
{
	struct wayland_output *output;

	output = malloc(sizeof *output);
	if (output == NULL)
		return -1;
	memset(output, 0, sizeof *output);

	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = width;
	output->mode.height = height;
	output->mode.refresh = 60;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current = &output->mode;
	weston_output_init(&output->base, &c->base, 0, 0, width, height,
			   WL_OUTPUT_TRANSFORM_NORMAL, 1);

	output->base.make = "waywayland";
	output->base.model = "none";

	weston_output_move(&output->base, 0, 0);

	output->parent.surface =
		wl_compositor_create_surface(c->parent.compositor);
	wl_surface_set_user_data(output->parent.surface, output);

	output->parent.egl_window =
		wl_egl_window_create(output->parent.surface,
				     width + c->border.left + c->border.right,
				     height + c->border.top + c->border.bottom);
	if (!output->parent.egl_window) {
		weston_log("failure to create wl_egl_window\n");
		goto cleanup_output;
	}

	if (gl_renderer_output_create(&output->base,
			output->parent.egl_window) < 0)
		goto cleanup_window;

	output->parent.draw_initial_frame = 1;
	output->parent.shell_surface =
		wl_shell_get_shell_surface(c->parent.shell,
					   output->parent.surface);
	wl_shell_surface_add_listener(output->parent.shell_surface,
				      &shell_surface_listener, output);
	wl_shell_surface_set_toplevel(output->parent.shell_surface);

	output->base.origin = output->base.current;
	output->base.start_repaint_loop = wayland_output_start_repaint_loop;
	output->base.repaint = wayland_output_repaint;
	output->base.destroy = wayland_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	wl_list_insert(c->base.output_list.prev, &output->base.link);

	return 0;

cleanup_window:
	wl_egl_window_destroy(output->parent.egl_window);
cleanup_output:
	/* FIXME: cleanup weston_output */
	free(output);

	return -1;
}

static void
shell_surface_ping(void *data, struct wl_shell_surface *shell_surface,
		   uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
shell_surface_configure(void *data, struct wl_shell_surface *shell_surface,
			uint32_t edges, int32_t width, int32_t height)
{
	/* FIXME: implement resizing */
}

static void
shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	shell_surface_ping,
	shell_surface_configure,
	shell_surface_popup_done
};

/* Events received from the wayland-server this compositor is client of: */

/* parent output interface */
static void
display_handle_geometry(void *data,
			struct wl_output *wl_output,
			int x,
			int y,
			int physical_width,
			int physical_height,
			int subpixel,
			const char *make,
			const char *model,
			int transform)
{
	struct wayland_compositor *c = data;

	c->parent.screen_allocation.x = x;
	c->parent.screen_allocation.y = y;
}

static void
display_handle_mode(void *data,
		    struct wl_output *wl_output,
		    uint32_t flags,
		    int width,
		    int height,
		    int refresh)
{
	struct wayland_compositor *c = data;

	c->parent.screen_allocation.width = width;
	c->parent.screen_allocation.height = height;
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
	display_handle_mode
};

static void
check_focus(struct wayland_input *input, wl_fixed_t x, wl_fixed_t y)
{
	struct wayland_compositor *c = input->compositor;
	int width, height, inside;

	width = input->output->mode.width;
	height = input->output->mode.height;

	inside = c->border.left <= wl_fixed_to_int(x) &&
		wl_fixed_to_int(x) < width + c->border.left &&
		c->border.top <= wl_fixed_to_int(y) &&
		wl_fixed_to_int(y) < height + c->border.top;

	if (!input->focus && inside) {
		notify_pointer_focus(&input->base, &input->output->base,
				     x - wl_fixed_from_int(c->border.left),
				     y = wl_fixed_from_int(c->border.top));
		wl_pointer_set_cursor(input->pointer,
				      input->enter_serial, NULL, 0, 0);
	} else if (input->focus && !inside) {
		notify_pointer_focus(&input->base, NULL, 0, 0);
		/* FIXME: Should set default cursor here. */
	}

	input->focus = inside;
}

/* parent input interface */
static void
input_handle_pointer_enter(void *data, struct wl_pointer *pointer,
			   uint32_t serial, struct wl_surface *surface,
			   wl_fixed_t x, wl_fixed_t y)
{
	struct wayland_input *input = data;

	/* XXX: If we get a modifier event immediately before the focus,
	 *      we should try to keep the same serial. */
	input->enter_serial = serial;
	input->output = wl_surface_get_user_data(surface);
	check_focus(input, x, y);
}

static void
input_handle_pointer_leave(void *data, struct wl_pointer *pointer,
			   uint32_t serial, struct wl_surface *surface)
{
	struct wayland_input *input = data;

	notify_pointer_focus(&input->base, NULL, 0, 0);
	input->output = NULL;
	input->focus = 0;
}

static void
input_handle_motion(void *data, struct wl_pointer *pointer,
		    uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	check_focus(input, x, y);
	if (input->focus)
		notify_motion(&input->base, time,
			      x - wl_fixed_from_int(c->border.left) -
			      input->base.pointer->x,
			      y - wl_fixed_from_int(c->border.top) -
			      input->base.pointer->y);
}

static void
input_handle_button(void *data, struct wl_pointer *pointer,
		    uint32_t serial, uint32_t time, uint32_t button,
		    uint32_t state_w)
{
	struct wayland_input *input = data;
	enum wl_pointer_button_state state = state_w;

	notify_button(&input->base, time, button, state);
}

static void
input_handle_axis(void *data, struct wl_pointer *pointer,
		  uint32_t time, uint32_t axis, wl_fixed_t value)
{
	struct wayland_input *input = data;

	notify_axis(&input->base, time, axis, value);
}

static const struct wl_pointer_listener pointer_listener = {
	input_handle_pointer_enter,
	input_handle_pointer_leave,
	input_handle_motion,
	input_handle_button,
	input_handle_axis,
};

static void
input_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
		    int fd, uint32_t size)
{
	struct wayland_input *input = data;
	struct xkb_keymap *keymap;
	char *map_str;

	if (!data) {
		close(fd);
		return;
	}

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED) {
		close(fd);
		return;
	}

	keymap = xkb_map_new_from_string(input->compositor->base.xkb_context,
					 map_str,
					 XKB_KEYMAP_FORMAT_TEXT_V1,
					 0);
	munmap(map_str, size);
	close(fd);

	if (!keymap) {
		weston_log("failed to compile keymap\n");
		return;
	}

	weston_seat_init_keyboard(&input->base, keymap);
	xkb_map_unref(keymap);
}

static void
input_handle_keyboard_enter(void *data,
			    struct wl_keyboard *keyboard,
			    uint32_t serial,
			    struct wl_surface *surface,
			    struct wl_array *keys)
{
	struct wayland_input *input = data;

	/* XXX: If we get a modifier event immediately before the focus,
	 *      we should try to keep the same serial. */
	notify_keyboard_focus_in(&input->base, keys,
				 STATE_UPDATE_AUTOMATIC);
}

static void
input_handle_keyboard_leave(void *data,
			    struct wl_keyboard *keyboard,
			    uint32_t serial,
			    struct wl_surface *surface)
{
	struct wayland_input *input = data;

	notify_keyboard_focus_out(&input->base);
}

static void
input_handle_key(void *data, struct wl_keyboard *keyboard,
		 uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	struct wayland_input *input = data;

	input->key_serial = serial;
	notify_key(&input->base, time, key,
		   state ? WL_KEYBOARD_KEY_STATE_PRESSED :
			   WL_KEYBOARD_KEY_STATE_RELEASED,
		   STATE_UPDATE_NONE);
}

static void
input_handle_modifiers(void *data, struct wl_keyboard *keyboard,
		       uint32_t serial_in, uint32_t mods_depressed,
		       uint32_t mods_latched, uint32_t mods_locked,
		       uint32_t group)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;
	uint32_t serial_out;

	/* If we get a key event followed by a modifier event with the
	 * same serial number, then we try to preserve those semantics by
	 * reusing the same serial number on the way out too. */
	if (serial_in == input->key_serial)
		serial_out = wl_display_get_serial(c->base.wl_display);
	else
		serial_out = wl_display_next_serial(c->base.wl_display);

	xkb_state_update_mask(input->base.xkb_state.state,
			      mods_depressed, mods_latched,
			      mods_locked, 0, 0, group);
	notify_modifiers(&input->base, serial_out);
}

static const struct wl_keyboard_listener keyboard_listener = {
	input_handle_keymap,
	input_handle_keyboard_enter,
	input_handle_keyboard_leave,
	input_handle_key,
	input_handle_modifiers,
};

static void
input_handle_capabilities(void *data, struct wl_seat *seat,
		          enum wl_seat_capability caps)
{
	struct wayland_input *input = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
		input->pointer = wl_seat_get_pointer(seat);
		wl_pointer_set_user_data(input->pointer, input);
		wl_pointer_add_listener(input->pointer, &pointer_listener,
					input);
		weston_seat_init_pointer(&input->base);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
		wl_pointer_destroy(input->pointer);
		input->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
		input->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_set_user_data(input->keyboard, input);
		wl_keyboard_add_listener(input->keyboard, &keyboard_listener,
					 input);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
		wl_keyboard_destroy(input->keyboard);
		input->keyboard = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	input_handle_capabilities,
};

static void
display_add_seat(struct wayland_compositor *c, uint32_t id)
{
	struct wayland_input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);

	weston_seat_init(&input->base, &c->base, "default");
	input->compositor = c;
	input->seat = wl_registry_bind(c->parent.registry, id,
				       &wl_seat_interface, 1);
	wl_list_insert(c->input_list.prev, &input->link);

	wl_seat_add_listener(input->seat, &seat_listener, input);
	wl_seat_set_user_data(input->seat, input);
}

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
		       const char *interface, uint32_t version)
{
	struct wayland_compositor *c = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		c->parent.compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		c->parent.output =
			wl_registry_bind(registry, name,
					 &wl_output_interface, 1);
		wl_output_add_listener(c->parent.output, &output_listener, c);
	} else if (strcmp(interface, "wl_shell") == 0) {
		c->parent.shell =
			wl_registry_bind(registry, name,
					 &wl_shell_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		display_add_seat(c, name);
	} else if (strcmp(interface, "wl_shm") == 0) {
		c->parent.shm =
			wl_registry_bind(registry, name, &wl_shm_interface, 1);
	}
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global
};

static int
wayland_compositor_handle_event(int fd, uint32_t mask, void *data)
{
	struct wayland_compositor *c = data;
	int count = 0;

	if (mask & WL_EVENT_READABLE)
		count = wl_display_dispatch(c->parent.wl_display);
	if (mask & WL_EVENT_WRITABLE)
		wl_display_flush(c->parent.wl_display);

	if (mask == 0) {
		count = wl_display_dispatch_pending(c->parent.wl_display);
		wl_display_flush(c->parent.wl_display);
	}

	return count;
}

static void
wayland_restore(struct weston_compositor *ec)
{
}

static void
wayland_destroy(struct weston_compositor *ec)
{
	struct wayland_compositor *c = (struct wayland_compositor *) ec;

	ec->renderer->destroy(ec);

	weston_compositor_shutdown(ec);

	if (c->parent.shm)
		wl_shm_destroy(c->parent.shm);

	free(ec);
}

static struct weston_compositor *
wayland_compositor_create(struct wl_display *display,
			  int width, int height, const char *display_name,
			  int *argc, char *argv[],
			  struct weston_config *config)
{
	struct wayland_compositor *c;
	struct wl_event_loop *loop;
	int fd;

	c = malloc(sizeof *c);
	if (c == NULL)
		return NULL;

	memset(c, 0, sizeof *c);

	if (weston_compositor_init(&c->base, display, argc, argv,
				   config) < 0)
		goto err_free;

	c->parent.wl_display = wl_display_connect(display_name);

	if (c->parent.wl_display == NULL) {
		weston_log("failed to create display: %m\n");
		goto err_compositor;
	}

	wl_list_init(&c->input_list);
	c->parent.registry = wl_display_get_registry(c->parent.wl_display);
	wl_registry_add_listener(c->parent.registry, &registry_listener, c);
	wl_display_dispatch(c->parent.wl_display);

	c->base.wl_display = display;
	if (gl_renderer_create(&c->base, c->parent.wl_display,
			gl_renderer_alpha_attribs,
			NULL) < 0)
		goto err_display;

	c->base.destroy = wayland_destroy;
	c->base.restore = wayland_restore;

	c->border.top = 30;
	c->border.bottom = 24;
	c->border.left = 25;
	c->border.right = 26;

	/* requires border fields */
	if (wayland_compositor_create_output(c, width, height) < 0)
		goto err_gl;

	/* requires gl_renderer_output_state_create called
	 * by wayland_compositor_create_output */
	create_border(c);

	loop = wl_display_get_event_loop(c->base.wl_display);

	fd = wl_display_get_fd(c->parent.wl_display);
	c->parent.wl_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     wayland_compositor_handle_event, c);
	if (c->parent.wl_source == NULL)
		goto err_gl;

	wl_event_source_check(c->parent.wl_source);

	return &c->base;

err_gl:
	c->base.renderer->destroy(&c->base);
err_display:
	wl_display_disconnect(c->parent.wl_display);
err_compositor:
	weston_compositor_shutdown(&c->base);
err_free:
	free(c);
	return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int *argc, char *argv[],
	     struct weston_config *config)
{
	int width = 1024, height = 640;
	char *display_name = NULL;

	const struct weston_option wayland_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &width },
		{ WESTON_OPTION_INTEGER, "height", 0, &height },
		{ WESTON_OPTION_STRING, "display", 0, &display_name },
	};

	parse_options(wayland_options,
		      ARRAY_LENGTH(wayland_options), argc, argv);

	return wayland_compositor_create(display, width, height, display_name,
					 argc, argv, config);
}

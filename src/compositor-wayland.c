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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "compositor.h"

struct wayland_compositor {
	struct weston_compositor	 base;

	struct {
		struct wl_display *display;
		struct wl_compositor *compositor;
		struct wl_shell *shell;
		struct wl_output *output;

		struct {
			int32_t x, y, width, height;
		} screen_allocation;

		struct wl_event_source *wl_source;
		uint32_t event_mask;
	} parent;

	struct wl_list input_list;
};

struct wayland_output {
	struct weston_output	base;

	struct {
		struct wl_surface	*surface;
		struct wl_shell_surface	*shell_surface;
		struct wl_egl_window	*egl_window;
	} parent;
	EGLSurface egl_surface;
	struct weston_mode	mode;
};

struct wayland_input {
	struct wayland_compositor *compositor;
	struct wl_input_device *input_device;
	struct wl_list link;
};

static int
wayland_input_create(struct wayland_compositor *c)
{
	struct weston_input_device *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return -1;

	memset(input, 0, sizeof *input);
	weston_input_device_init(input, &c->base);

	c->base.input_device = &input->input_device;

	return 0;
}

static int
wayland_compositor_init_egl(struct wayland_compositor *c)
{
	EGLint major, minor;
	EGLint n;
	const char *extensions;
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_DEPTH_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	c->base.display = eglGetDisplay(c->parent.display);
	if (c->base.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	if (!eglInitialize(c->base.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

	extensions = eglQueryString(c->base.display, EGL_EXTENSIONS);
	if (!strstr(extensions, "EGL_KHR_surfaceless_gles2")) {
		fprintf(stderr, "EGL_KHR_surfaceless_gles2 not available\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind EGL_OPENGL_ES_API\n");
		return -1;
	}
   	if (!eglChooseConfig(c->base.display, config_attribs,
			     &c->base.config, 1, &n) || n == 0) {
		fprintf(stderr, "failed to choose config: %d\n", n);
		return -1;
	}

	c->base.context = eglCreateContext(c->base.display, c->base.config,
					   EGL_NO_CONTEXT, context_attribs);
	if (c->base.context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	if (!eglMakeCurrent(c->base.display, EGL_NO_SURFACE,
			    EGL_NO_SURFACE, c->base.context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

	return 0;
}

static int
wayland_output_prepare_render(struct weston_output *output_base)
{
	struct wayland_output *output = (struct wayland_output *) output_base;
	struct weston_compositor *ec = output->base.compositor;

	if (!eglMakeCurrent(ec->display, output->egl_surface,
			    output->egl_surface, ec->context)) {
		fprintf(stderr, "failed to make current\n");
		return -1;
	}

	return 0;
}

static void
frame_done(void *data, struct wl_callback *wl_callback, uint32_t time)
{
	struct weston_output *output = data;

	weston_output_finish_frame(output, time);
}

static const struct wl_callback_listener frame_listener = {
	frame_done
};

static int
wayland_output_present(struct weston_output *output_base)
{
	struct wayland_output *output = (struct wayland_output *) output_base;
	struct wayland_compositor *c =
		(struct wayland_compositor *) output->base.compositor;
	struct wl_callback *callback;

	if (wayland_output_prepare_render(&output->base))
		return -1;

	eglSwapBuffers(c->base.display, output->egl_surface);
	callback = wl_surface_frame(output->parent.surface);
	wl_callback_add_listener(callback, &frame_listener, output);

	return 0;
}

static int
wayland_output_prepare_scanout_surface(struct weston_output *output_base,
				       struct weston_surface *es)
{
	return -1;
}

static int
wayland_output_set_cursor(struct weston_output *output_base,
			  struct weston_input_device *input)
{
	return -1;
}

static void
wayland_output_destroy(struct weston_output *output_base)
{
	struct wayland_output *output = (struct wayland_output *) output_base;
	struct weston_compositor *ec = output->base.compositor;

	eglDestroySurface(ec->display, output->egl_surface);
	wl_egl_window_destroy(output->parent.egl_window);
	free(output);

	return;
}

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
			 WL_OUTPUT_FLIPPED);

	output->parent.surface =
		wl_compositor_create_surface(c->parent.compositor);
	wl_surface_set_user_data(output->parent.surface, output);

	output->parent.egl_window =
		wl_egl_window_create(output->parent.surface, width, height);
	if (!output->parent.egl_window) {
		fprintf(stderr, "failure to create wl_egl_window\n");
		goto cleanup_output;
	}

	output->egl_surface =
		eglCreateWindowSurface(c->base.display, c->base.config,
				       output->parent.egl_window, NULL);
	if (!output->egl_surface) {
		fprintf(stderr, "failed to create window surface\n");
		goto cleanup_window;
	}

	if (!eglMakeCurrent(c->base.display, output->egl_surface,
			    output->egl_surface, c->base.context)) {
		fprintf(stderr, "failed to make surface current\n");
		goto cleanup_surface;
		return -1;
	}

	output->parent.shell_surface =
		wl_shell_get_shell_surface(c->parent.shell,
					   output->parent.surface);
	/* FIXME: add shell_surface listener for resizing */
	wl_shell_surface_set_toplevel(output->parent.shell_surface);

	glClearColor(0, 0, 0, 0.5);

	output->base.prepare_render = wayland_output_prepare_render;
	output->base.present = wayland_output_present;
	output->base.prepare_scanout_surface =
		wayland_output_prepare_scanout_surface;
	output->base.set_hardware_cursor = wayland_output_set_cursor;
	output->base.destroy = wayland_output_destroy;

	wl_list_insert(c->base.output_list.prev, &output->base.link);

	return 0;

cleanup_surface:
	eglDestroySurface(c->base.display, output->egl_surface);
cleanup_window:
	wl_egl_window_destroy(output->parent.egl_window);
cleanup_output:
	/* FIXME: cleanup weston_output */
	free(output);

	return -1;
}

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
			const char *model)
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

/* parent input interface */
static void
input_handle_motion(void *data, struct wl_input_device *input_device,
		     uint32_t time,
		     int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	notify_motion(c->base.input_device, time, sx, sy);
}

static void
input_handle_button(void *data,
		     struct wl_input_device *input_device,
		     uint32_t time, uint32_t button, uint32_t state)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	notify_button(c->base.input_device, time, button, state);
}

static void
input_handle_key(void *data, struct wl_input_device *input_device,
		  uint32_t time, uint32_t key, uint32_t state)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;

	notify_key(c->base.input_device, time, key, state);
}

static void
input_handle_pointer_focus(void *data,
			    struct wl_input_device *input_device,
			    uint32_t time, struct wl_surface *surface,
			    int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct wayland_input *input = data;
	struct wayland_output *output;
	struct wayland_compositor *c = input->compositor;

	if (surface) {
		output = wl_surface_get_user_data(surface);
		notify_pointer_focus(c->base.input_device,
				     time, &output->base, sx, sy);
	} else {
		notify_pointer_focus(c->base.input_device, time, NULL, 0, 0);
	}
}

static void
input_handle_keyboard_focus(void *data,
			     struct wl_input_device *input_device,
			     uint32_t time,
			     struct wl_surface *surface,
			     struct wl_array *keys)
{
	struct wayland_input *input = data;
	struct wayland_compositor *c = input->compositor;
	struct wayland_output *output;

	if (surface) {
		output = wl_surface_get_user_data(surface);
		notify_keyboard_focus(c->base.input_device,
				      time, &output->base, keys);
	} else {
		notify_keyboard_focus(c->base.input_device, time, NULL, NULL);
	}
}

static const struct wl_input_device_listener input_device_listener = {
	input_handle_motion,
	input_handle_button,
	input_handle_key,
	input_handle_pointer_focus,
	input_handle_keyboard_focus,
};

static void
display_add_input(struct wayland_compositor *c, uint32_t id)
{
	struct wayland_input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);

	input->compositor = c;
	input->input_device = wl_display_bind(c->parent.display,
					      id, &wl_input_device_interface);
	wl_list_insert(c->input_list.prev, &input->link);

	wl_input_device_add_listener(input->input_device,
				     &input_device_listener, input);
	wl_input_device_set_user_data(input->input_device, input);
}

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct wayland_compositor *c = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		c->parent.compositor =
			wl_display_bind(display, id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_output") == 0) {
		c->parent.output =
			wl_display_bind(display, id, &wl_output_interface);
		wl_output_add_listener(c->parent.output, &output_listener, c);
	} else if (strcmp(interface, "wl_input_device") == 0) {
		display_add_input(c, id);
	} else if (strcmp(interface, "wl_shell") == 0) {
		c->parent.shell =
			wl_display_bind(display, id, &wl_shell_interface);
	}
}

static int
update_event_mask(uint32_t mask, void *data)
{
	struct wayland_compositor *c = data;

	c->parent.event_mask = mask;
	if (c->parent.wl_source)
		wl_event_source_fd_update(c->parent.wl_source, mask);

	return 0;
}

static int
wayland_compositor_handle_event(int fd, uint32_t mask, void *data)
{
	struct wayland_compositor *c = data;

	if (mask & WL_EVENT_READABLE)
		wl_display_iterate(c->parent.display, WL_DISPLAY_READABLE);
	if (mask & WL_EVENT_WRITABLE)
		wl_display_iterate(c->parent.display, WL_DISPLAY_WRITABLE);

	return 1;
}

static void
wayland_destroy(struct weston_compositor *ec)
{
	weston_compositor_shutdown(ec);

	free(ec);
}

static struct weston_compositor *
wayland_compositor_create(struct wl_display *display, int width, int height)
{
	struct wayland_compositor *c;
	struct wl_event_loop *loop;
	int fd;

	c = malloc(sizeof *c);
	if (c == NULL)
		return NULL;

	memset(c, 0, sizeof *c);

	c->parent.display = wl_display_connect(NULL);

	if (c->parent.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return NULL;
	}

	wl_list_init(&c->input_list);
	wl_display_add_global_listener(c->parent.display,
				display_handle_global, c);

	wl_display_iterate(c->parent.display, WL_DISPLAY_READABLE);

	c->base.wl_display = display;
	if (wayland_compositor_init_egl(c) < 0)
		return NULL;

	c->base.destroy = wayland_destroy;

	/* Can't init base class until we have a current egl context */
	if (weston_compositor_init(&c->base, display) < 0)
		return NULL;

	if (wayland_compositor_create_output(c, width, height) < 0)
		return NULL;

	if (wayland_input_create(c) < 0)
		return NULL;

	loop = wl_display_get_event_loop(c->base.wl_display);

	fd = wl_display_get_fd(c->parent.display, update_event_mask, c);
	c->parent.wl_source =
		wl_event_loop_add_fd(loop, fd, c->parent.event_mask,
				     wayland_compositor_handle_event, c);
	if (c->parent.wl_source == NULL)
		return NULL;

	return &c->base;
}

struct weston_compositor *
backend_init(struct wl_display *display, char *options);

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, char *options)
{
	int width = 1024, height = 640, i;
	char *p, *value;

	static char * const tokens[] = { "width", "height", NULL };
	
	p = options;
	while (i = getsubopt(&p, tokens, &value), i != -1) {
		switch (i) {
		case 0:
			width = strtol(value, NULL, 0);
			break;
		case 1:
			height = strtol(value, NULL, 0);
			break;
		}
	}

	return wayland_compositor_create(display, width, height);
}

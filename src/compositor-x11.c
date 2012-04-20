/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2010-2011 Intel Corporation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <linux/input.h>

#include <xcb/xcb.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include "compositor.h"
#include "../shared/config-parser.h"

struct x11_compositor {
	struct weston_compositor	 base;

	EGLSurface		 dummy_pbuffer;

	Display			*dpy;
	xcb_connection_t	*conn;
	xcb_screen_t		*screen;
	xcb_cursor_t		 null_cursor;
	struct wl_array		 keys;
	struct wl_event_source	*xcb_source;
	struct {
		xcb_atom_t		 wm_protocols;
		xcb_atom_t		 wm_normal_hints;
		xcb_atom_t		 wm_size_hints;
		xcb_atom_t		 wm_delete_window;
		xcb_atom_t		 wm_class;
		xcb_atom_t		 net_wm_name;
		xcb_atom_t		 net_wm_icon;
		xcb_atom_t		 net_wm_state;
		xcb_atom_t		 net_wm_state_fullscreen;
		xcb_atom_t		 string;
		xcb_atom_t		 utf8_string;
		xcb_atom_t		 cardinal;
	} atom;
};

struct x11_output {
	struct weston_output	base;

	xcb_window_t		window;
	EGLSurface		egl_surface;
	struct weston_mode	mode;
	struct wl_event_source *finish_frame_timer;
};

struct x11_input {
	struct weston_input_device base;
};


static int
x11_input_create(struct x11_compositor *c)
{
	struct x11_input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return -1;

	memset(input, 0, sizeof *input);
	weston_input_device_init(&input->base, &c->base);

	c->base.input_device = &input->base.input_device;

	return 0;
}

static void
x11_input_destroy(struct x11_compositor *compositor)
{
	struct x11_input *input = container_of(compositor->base.input_device,
					       struct x11_input,
					       base.input_device);

	weston_input_device_release(&input->base);
	free(input);
}

static int
x11_compositor_init_egl(struct x11_compositor *c)
{
	EGLint major, minor;
	EGLint n;
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint pbuffer_attribs[] = {
		EGL_WIDTH, 10,
		EGL_HEIGHT, 10,
		EGL_NONE
	};

	c->base.display = eglGetDisplay(c->dpy);
	if (c->base.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	if (!eglInitialize(c->base.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
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

	c->dummy_pbuffer = eglCreatePbufferSurface(c->base.display,
						   c->base.config,
						   pbuffer_attribs);
	if (c->base.context == NULL) {
		fprintf(stderr, "failed to create dummy pbuffer\n");
		return -1;
	}

	if (!eglMakeCurrent(c->base.display, EGL_NO_SURFACE,
			    EGL_NO_SURFACE, c->base.context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

	return 0;
}

static void
x11_compositor_fini_egl(struct x11_compositor *compositor)
{
	eglMakeCurrent(compositor->base.display,
		       EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglTerminate(compositor->base.display);
	eglReleaseThread();
}

static void
x11_output_repaint(struct weston_output *output_base,
		   pixman_region32_t *damage)
{
	struct x11_output *output = (struct x11_output *)output_base;
	struct x11_compositor *compositor =
		(struct x11_compositor *)output->base.compositor;
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

	wl_event_source_timer_update(output->finish_frame_timer, 10);
}

static int
finish_frame_handler(void *data)
{
	struct x11_output *output = data;
	uint32_t msec;
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	weston_output_finish_frame(&output->base, msec);

	return 1;
}

static void
x11_output_destroy(struct weston_output *output_base)
{
	struct x11_output *output = (struct x11_output *)output_base;
	struct x11_compositor *compositor =
		(struct x11_compositor *)output->base.compositor;

	wl_list_remove(&output->base.link);
	wl_event_source_remove(output->finish_frame_timer);

	eglDestroySurface(compositor->base.display, output->egl_surface);

	xcb_destroy_window(compositor->conn, output->window);

	weston_output_destroy(&output->base);

	free(output);
}

static void
x11_output_set_wm_protocols(struct x11_output *output)
{
	xcb_atom_t list[1];
	struct x11_compositor *c =
		(struct x11_compositor *) output->base.compositor;

	list[0] = c->atom.wm_delete_window;
	xcb_change_property (c->conn, 
			     XCB_PROP_MODE_REPLACE,
			     output->window,
			     c->atom.wm_protocols,
			     XCB_ATOM_ATOM,
			     32,
			     ARRAY_LENGTH(list),
			     list);
}

static void
x11_output_change_state(struct x11_output *output, int add, xcb_atom_t state)
{
	xcb_client_message_event_t event;
	struct x11_compositor *c =
		(struct x11_compositor *) output->base.compositor;
	xcb_screen_iterator_t iter;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  

	memset(&event, 0, sizeof event);
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.window = output->window;
	event.type = c->atom.net_wm_state;

	event.data.data32[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	event.data.data32[1] = state;
	event.data.data32[2] = 0;
	event.data.data32[3] = 0;
	event.data.data32[4] = 0;

	iter = xcb_setup_roots_iterator(xcb_get_setup(c->conn));
	xcb_send_event(c->conn, 0, iter.data->root,
		       XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
		       (void *) &event);
}


struct wm_normal_hints {
    	uint32_t flags;
	uint32_t pad[4];
	int32_t min_width, min_height;
	int32_t max_width, max_height;
    	int32_t width_inc, height_inc;
    	int32_t min_aspect_x, min_aspect_y;
    	int32_t max_aspect_x, max_aspect_y;
	int32_t base_width, base_height;
	int32_t win_gravity;
};

#define WM_NORMAL_HINTS_MIN_SIZE	16
#define WM_NORMAL_HINTS_MAX_SIZE	32

static void
x11_output_set_icon(struct x11_compositor *c,
		    struct x11_output *output, const char *filename)
{
	uint32_t *icon;
	int32_t width, height;
	pixman_image_t *image;

	image = load_image(filename);
	if (!image)
		return;
	width = pixman_image_get_width(image);
	height = pixman_image_get_height(image);
	icon = malloc(width * height * 4 + 8);
	if (!icon) {
		pixman_image_unref(image);
		return;
	}

	icon[0] = width;
	icon[1] = height;
	memcpy(icon + 2, pixman_image_get_data(image), width * height * 4);
	xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE, output->window,
			    c->atom.net_wm_icon, c->atom.cardinal, 32,
			    width * height + 2, icon);
	free(icon);
	pixman_image_unref(image);
}

static int
x11_compositor_create_output(struct x11_compositor *c, int x, int y,
			     int width, int height, int fullscreen)
{
	static const char name[] = "Weston Compositor";
	static const char class[] = "weston-1\0Weston Compositor";
	struct x11_output *output;
	xcb_screen_iterator_t iter;
	struct wm_normal_hints normal_hints;
	struct wl_event_loop *loop;
	uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	uint32_t values[2] = { 
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_ENTER_WINDOW |
		XCB_EVENT_MASK_LEAVE_WINDOW |
		XCB_EVENT_MASK_KEYMAP_STATE |
		XCB_EVENT_MASK_FOCUS_CHANGE,
		0
	};

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
	weston_output_init(&output->base, &c->base, x, y, width, height,
			 WL_OUTPUT_FLIPPED);

	values[1] = c->null_cursor;
	output->window = xcb_generate_id(c->conn);
	iter = xcb_setup_roots_iterator(xcb_get_setup(c->conn));
	xcb_create_window(c->conn,
			  XCB_COPY_FROM_PARENT,
			  output->window,
			  iter.data->root,
			  0, 0,
			  width, height,
			  0,
			  XCB_WINDOW_CLASS_INPUT_OUTPUT,
			  iter.data->root_visual,
			  mask, values);

	/* Don't resize me. */
	memset(&normal_hints, 0, sizeof normal_hints);
	normal_hints.flags =
		WM_NORMAL_HINTS_MAX_SIZE | WM_NORMAL_HINTS_MIN_SIZE;
	normal_hints.min_width = width;
	normal_hints.min_height = height;
	normal_hints.max_width = width;
	normal_hints.max_height = height;
	xcb_change_property (c->conn, XCB_PROP_MODE_REPLACE, output->window,
			     c->atom.wm_normal_hints,
			     c->atom.wm_size_hints, 32,
			     sizeof normal_hints / 4,
			     (uint8_t *) &normal_hints);

	/* Set window name.  Don't bother with non-EWMH WMs. */
	xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE, output->window,
			    c->atom.net_wm_name, c->atom.utf8_string, 8,
			    strlen(name), name);
	xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE, output->window,
			    c->atom.wm_class, c->atom.string, 8,
			    sizeof class, class);

	x11_output_set_icon(c, output, DATADIR "/weston/wayland.png");

	xcb_map_window(c->conn, output->window);

	x11_output_set_wm_protocols(output);

	if (fullscreen)
		x11_output_change_state(output, 1,
					c->atom.net_wm_state_fullscreen);

	output->egl_surface = 
		eglCreateWindowSurface(c->base.display, c->base.config,
				       output->window, NULL);
	if (!output->egl_surface) {
		fprintf(stderr, "failed to create window surface\n");
		return -1;
	}
	if (!eglMakeCurrent(c->base.display, output->egl_surface,
			    output->egl_surface, c->base.context)) {
		fprintf(stderr, "failed to make surface current\n");
		return -1;
	}

	loop = wl_display_get_event_loop(c->base.wl_display);
	output->finish_frame_timer =
		wl_event_loop_add_timer(loop, finish_frame_handler, output);

	output->base.origin = output->base.current;
	output->base.repaint = x11_output_repaint;
	output->base.destroy = x11_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	wl_list_insert(c->base.output_list.prev, &output->base.link);

	return 0;
}

static struct x11_output *
x11_compositor_find_output(struct x11_compositor *c, xcb_window_t window)
{
	struct x11_output *output;

	wl_list_for_each(output, &c->base.output_list, base.link) {
		if (output->window == window)
			return output;
	}

	return NULL;
}

static void
x11_compositor_deliver_button_event(struct x11_compositor *c,
				    xcb_generic_event_t *event, int state)
{
	xcb_button_press_event_t *button_event =
		(xcb_button_press_event_t *) event;
	int button;

	switch (button_event->detail) {
	default:
		button = button_event->detail + BTN_LEFT - 1;
		break;
	case 2:
		button = BTN_MIDDLE;
		break;
	case 3:
		button = BTN_RIGHT;
		break;
	case 4:
		if (state)
			notify_axis(c->base.input_device,
				      weston_compositor_get_time(),
				      WL_INPUT_DEVICE_AXIS_VERTICAL_SCROLL, 1);
		return;
	case 5:
		if (state)
			notify_axis(c->base.input_device,
				      weston_compositor_get_time(),
				      WL_INPUT_DEVICE_AXIS_VERTICAL_SCROLL, -1);
		return;
	case 6:
		if (state)
			notify_axis(c->base.input_device,
				      weston_compositor_get_time(),
				      WL_INPUT_DEVICE_AXIS_HORIZONTAL_SCROLL, 1);
		return;
	case 7:
		if (state)
			notify_axis(c->base.input_device,
				      weston_compositor_get_time(),
				      WL_INPUT_DEVICE_AXIS_HORIZONTAL_SCROLL, -1);
		return;
	}

	notify_button(c->base.input_device,
		      weston_compositor_get_time(), button, state);
}

static int
x11_compositor_next_event(struct x11_compositor *c,
			  xcb_generic_event_t **event, uint32_t mask)
{
	if (mask & WL_EVENT_READABLE) {
		*event = xcb_poll_for_event(c->conn);
	} else {
#ifdef HAVE_XCB_POLL_FOR_QUEUED_EVENT
		*event = xcb_poll_for_queued_event(c->conn);
#else
		*event = xcb_poll_for_event(c->conn);
#endif
	}

	return *event != NULL;
}

static int
x11_compositor_handle_event(int fd, uint32_t mask, void *data)
{
	struct x11_compositor *c = data;
	struct x11_output *output;
	xcb_generic_event_t *event, *prev;
	xcb_client_message_event_t *client_message;
	xcb_motion_notify_event_t *motion_notify;
	xcb_enter_notify_event_t *enter_notify;
	xcb_key_press_event_t *key_press, *key_release;
	xcb_keymap_notify_event_t *keymap_notify;
	xcb_focus_in_event_t *focus_in;
	xcb_atom_t atom;
	uint32_t *k;
	uint32_t i, set;

	prev = NULL;
	while (x11_compositor_next_event(c, &event, mask)) {
		switch (prev ? prev->response_type & ~0x80 : 0x80) {
		case XCB_KEY_RELEASE:
			key_release = (xcb_key_press_event_t *) prev;
			key_press = (xcb_key_press_event_t *) event;
			if ((event->response_type & ~0x80) == XCB_KEY_PRESS &&
			    key_release->time == key_press->time &&
			    key_release->detail == key_press->detail) {
				/* Don't deliver the held key release
				 * event or the new key press event. */
				free(event);
				free(prev);
				prev = NULL;
				continue;
			} else {
				/* Deliver the held key release now
				 * and fall through and handle the new
				 * event below. */
				notify_key(c->base.input_device,
					   weston_compositor_get_time(),
					   key_release->detail - 8, 0);
				free(prev);
				prev = NULL;
				break;
			}

		case XCB_FOCUS_IN:
			/* assert event is keymap_notify */
			focus_in = (xcb_focus_in_event_t *) prev;
			keymap_notify = (xcb_keymap_notify_event_t *) event;
			c->keys.size = 0;
			for (i = 0; i < ARRAY_LENGTH(keymap_notify->keys) * 8; i++) {
				set = keymap_notify->keys[i >> 3] &
					(1 << (i & 7));
				if (set) {
					k = wl_array_add(&c->keys, sizeof *k);
					*k = i;
				}
			}

			output = x11_compositor_find_output(c, focus_in->event);
			notify_keyboard_focus(c->base.input_device, &c->keys);

			free(prev);
			prev = NULL;
			break;

		default:
			/* No previous event held */
			break;
		}

		switch (event->response_type & ~0x80) {
		case XCB_KEY_PRESS:
			key_press = (xcb_key_press_event_t *) event;
			notify_key(c->base.input_device,
				   weston_compositor_get_time(),
				   key_press->detail - 8, 1);
			break;
		case XCB_KEY_RELEASE:
			prev = event;
			break;
		case XCB_BUTTON_PRESS:
			x11_compositor_deliver_button_event(c, event, 1);
			break;
		case XCB_BUTTON_RELEASE:
			x11_compositor_deliver_button_event(c, event, 0);
			break;
		case XCB_MOTION_NOTIFY:
			motion_notify = (xcb_motion_notify_event_t *) event;
			output = x11_compositor_find_output(c, motion_notify->event);
			notify_motion(c->base.input_device,
				      weston_compositor_get_time(),
				      output->base.x + motion_notify->event_x,
				      output->base.y + motion_notify->event_y);
			break;

		case XCB_EXPOSE:
			/* FIXME: schedule output repaint */
			/* output = x11_compositor_find_output(c, expose->window); */

			weston_compositor_schedule_repaint(&c->base);
			break;

		case XCB_ENTER_NOTIFY:
			enter_notify = (xcb_enter_notify_event_t *) event;
			if (enter_notify->state >= Button1Mask)
				break;
			output = x11_compositor_find_output(c, enter_notify->event);
			notify_pointer_focus(c->base.input_device,
					     &output->base,
					     output->base.x + enter_notify->event_x,
					     output->base.y + enter_notify->event_y);
			break;

		case XCB_LEAVE_NOTIFY:
			enter_notify = (xcb_enter_notify_event_t *) event;
			if (enter_notify->state >= Button1Mask)
				break;
			output = x11_compositor_find_output(c, enter_notify->event);
			notify_pointer_focus(c->base.input_device, NULL,
					     output->base.x + enter_notify->event_x,
					     output->base.y + enter_notify->event_y);
			break;

		case XCB_CLIENT_MESSAGE:
			client_message = (xcb_client_message_event_t *) event;
			atom = client_message->data.data32[0];
			if (atom == c->atom.wm_delete_window)
				wl_display_terminate(c->base.wl_display);
			break;

		case XCB_FOCUS_IN:
			focus_in = (xcb_focus_in_event_t *) event;
			if (focus_in->mode == XCB_NOTIFY_MODE_WHILE_GRABBED)
				break;

			prev = event;
			break;

		case XCB_FOCUS_OUT:
			focus_in = (xcb_focus_in_event_t *) event;
			if (focus_in->mode == XCB_NOTIFY_MODE_WHILE_GRABBED ||
			    focus_in->mode == XCB_NOTIFY_MODE_UNGRAB)
				break;
			notify_keyboard_focus(c->base.input_device, NULL);
			break;

		default:
			break;
		}

		if (prev != event)
			free (event);
	}

	switch (prev ? prev->response_type & ~0x80 : 0x80) {
	case XCB_KEY_RELEASE:
		key_release = (xcb_key_press_event_t *) prev;
		notify_key(c->base.input_device,
			   weston_compositor_get_time(),
			   key_release->detail - 8, 0);
		free(prev);
		prev = NULL;
		break;
	default:
		break;
	}

	return event != NULL;
}

#define F(field) offsetof(struct x11_compositor, field)

static void
x11_compositor_get_resources(struct x11_compositor *c)
{
	static const struct { const char *name; int offset; } atoms[] = {
		{ "WM_PROTOCOLS",	F(atom.wm_protocols) },
		{ "WM_NORMAL_HINTS",	F(atom.wm_normal_hints) },
		{ "WM_SIZE_HINTS",	F(atom.wm_size_hints) },
		{ "WM_DELETE_WINDOW",	F(atom.wm_delete_window) },
		{ "WM_CLASS",		F(atom.wm_class) },
		{ "_NET_WM_NAME",	F(atom.net_wm_name) },
		{ "_NET_WM_ICON",	F(atom.net_wm_icon) },
		{ "_NET_WM_STATE",	F(atom.net_wm_state) },
		{ "_NET_WM_STATE_FULLSCREEN", F(atom.net_wm_state_fullscreen) },
		{ "STRING",		F(atom.string) },
		{ "UTF8_STRING",	F(atom.utf8_string) },
		{ "CARDINAL",		F(atom.cardinal) },
	};

	xcb_intern_atom_cookie_t cookies[ARRAY_LENGTH(atoms)];
	xcb_intern_atom_reply_t *reply;
	xcb_pixmap_t pixmap;
	xcb_gc_t gc;
	unsigned int i;
	uint8_t data[] = { 0, 0, 0, 0 };

	for (i = 0; i < ARRAY_LENGTH(atoms); i++)
		cookies[i] = xcb_intern_atom (c->conn, 0,
					      strlen(atoms[i].name),
					      atoms[i].name);

	for (i = 0; i < ARRAY_LENGTH(atoms); i++) {
		reply = xcb_intern_atom_reply (c->conn, cookies[i], NULL);
		*(xcb_atom_t *) ((char *) c + atoms[i].offset) = reply->atom;
		free(reply);
	}

	pixmap = xcb_generate_id(c->conn);
	gc = xcb_generate_id(c->conn);
	xcb_create_pixmap(c->conn, 1, pixmap, c->screen->root, 1, 1);
	xcb_create_gc(c->conn, gc, pixmap, 0, NULL);
	xcb_put_image(c->conn, XCB_IMAGE_FORMAT_XY_PIXMAP,
		      pixmap, gc, 1, 1, 0, 0, 0, 32, sizeof data, data);
	c->null_cursor = xcb_generate_id(c->conn);
	xcb_create_cursor (c->conn, c->null_cursor,
			   pixmap, pixmap, 0, 0, 0,  0, 0, 0,  1, 1);
	xcb_free_gc(c->conn, gc);
	xcb_free_pixmap(c->conn, pixmap);
}

static void
x11_destroy(struct weston_compositor *ec)
{
	struct x11_compositor *compositor = (struct x11_compositor *)ec;

	wl_event_source_remove(compositor->xcb_source);
	x11_input_destroy(compositor);

	weston_compositor_shutdown(ec); /* destroys outputs, too */

	x11_compositor_fini_egl(compositor);

	XCloseDisplay(compositor->dpy);
	free(ec);
}

static struct weston_compositor *
x11_compositor_create(struct wl_display *display,
		      int width, int height, int count, int fullscreen)
{
	struct x11_compositor *c;
	xcb_screen_iterator_t s;
	int i, x;

	c = malloc(sizeof *c);
	if (c == NULL)
		return NULL;

	memset(c, 0, sizeof *c);

	c->dpy = XOpenDisplay(NULL);
	if (c->dpy == NULL)
		return NULL;

	c->conn = XGetXCBConnection(c->dpy);
	XSetEventQueueOwner(c->dpy, XCBOwnsEventQueue);

	if (xcb_connection_has_error(c->conn))
		return NULL;

	s = xcb_setup_roots_iterator(xcb_get_setup(c->conn));
	c->screen = s.data;
	wl_array_init(&c->keys);

	x11_compositor_get_resources(c);

	c->base.wl_display = display;
	if (x11_compositor_init_egl(c) < 0)
		return NULL;

	c->base.destroy = x11_destroy;

	/* Can't init base class until we have a current egl context */
	if (weston_compositor_init(&c->base, display) < 0)
		return NULL;

	for (i = 0, x = 0; i < count; i++) {
		if (x11_compositor_create_output(c, x, 0, width, height,
						 fullscreen) < 0)
			return NULL;
		x += width;
	}

	if (x11_input_create(c) < 0)
		return NULL;

	c->xcb_source =
		wl_event_loop_add_fd(c->base.input_loop,
				     xcb_get_file_descriptor(c->conn),
				     WL_EVENT_READABLE,
				     x11_compositor_handle_event, c);
	wl_event_source_check(c->xcb_source);

	return &c->base;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[])
{
	int width = 1024, height = 640, fullscreen = 0, count = 1;

	const struct weston_option x11_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &width },
		{ WESTON_OPTION_INTEGER, "height", 0, &height },
		{ WESTON_OPTION_BOOLEAN, "fullscreen", 0, &fullscreen },
		{ WESTON_OPTION_INTEGER, "output-count", 0, &count },
	};

	parse_options(x11_options, ARRAY_LENGTH(x11_options), argc, argv);

	return x11_compositor_create(display,
				     width, height, count, fullscreen);
}

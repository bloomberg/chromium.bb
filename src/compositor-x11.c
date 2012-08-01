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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <linux/input.h>

#include <xcb/xcb.h>
#ifdef HAVE_XCB_XKB
#include <xcb/xkb.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xkbcommon/xkbcommon.h>

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
	struct xkb_keymap	*xkb_keymap;
	unsigned int		 has_xkb;
	uint8_t			 xkb_event_base;
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
		xcb_atom_t		 xkb_names;
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
	struct weston_seat base;
};

static struct xkb_keymap *
x11_compositor_get_keymap(struct x11_compositor *c)
{
	xcb_get_property_cookie_t cookie;
	xcb_get_property_reply_t *reply;
	xcb_generic_error_t *error;
	struct xkb_rule_names names;
	struct xkb_keymap *ret;
	const char *value_all, *value_part;
	int length_all, length_part;

	memset(&names, 0, sizeof(names));

	cookie = xcb_get_property(c->conn, 0, c->screen->root,
				  c->atom.xkb_names, c->atom.string, 0, 1024);
	reply = xcb_get_property_reply(c->conn, cookie, &error);
	if (reply == NULL)
		return NULL;

	value_all = xcb_get_property_value(reply);
	length_all = xcb_get_property_value_length(reply);
	value_part = value_all;

#define copy_prop_value(to) \
	length_part = strlen(value_part); \
	if (value_part + length_part < (value_all + length_all) && \
	    length_part > 0) \
		names.to = value_part; \
	value_part += length_part + 1;

	copy_prop_value(rules);
	copy_prop_value(model);
	copy_prop_value(layout);
	copy_prop_value(variant);
	copy_prop_value(options);
#undef copy_prop_value

	ret = xkb_map_new_from_names(c->base.xkb_context, &names, 0);

	free(reply);
	return ret;
}

static void
x11_compositor_setup_xkb(struct x11_compositor *c)
{
#ifndef HAVE_XCB_XKB
	weston_log("XCB-XKB not available during build\n");
	c->has_xkb = 0;
	c->xkb_event_base = 0;
	return;
#else
	const xcb_query_extension_reply_t *ext;
	xcb_generic_error_t *error;
	xcb_void_cookie_t select;
	xcb_xkb_per_client_flags_cookie_t pcf;
	xcb_xkb_per_client_flags_reply_t *pcf_reply;

	c->has_xkb = 0;
	c->xkb_event_base = 0;

	ext = xcb_get_extension_data(c->conn, &xcb_xkb_id);
	if (!ext) {
		weston_log("XKB extension not available on host X11 server\n");
		return;
	}
	c->xkb_event_base = ext->first_event;

	select = xcb_xkb_select_events(c->conn,
				       XCB_XKB_ID_USE_CORE_KBD,
				       XCB_XKB_EVENT_TYPE_STATE_NOTIFY,
				       0,
				       XCB_XKB_EVENT_TYPE_STATE_NOTIFY,
				       0,
				       0,
				       NULL);
	error = xcb_request_check(c->conn, select);
	if (error) {
		weston_log("error: failed to select for XKB state events\n");
		return;
	}

	pcf = xcb_xkb_per_client_flags(c->conn,
				       XCB_XKB_ID_USE_CORE_KBD,
				       XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
				       XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
				       0,
				       0,
				       0);
	pcf_reply = xcb_xkb_per_client_flags_reply(c->conn, pcf, &error);
	free(pcf_reply);
	if (error) {
		weston_log("failed to set XKB per-client flags, not using "
			   "detectable repeat\n");
		return;
	}

	c->has_xkb = 1;
#endif
}

static int
x11_input_create(struct x11_compositor *c, int no_input)
{
	struct x11_input *input;
	struct xkb_keymap *keymap;

	input = malloc(sizeof *input);
	if (input == NULL)
		return -1;

	memset(input, 0, sizeof *input);
	weston_seat_init(&input->base, &c->base);
	c->base.seat = &input->base;

	if (no_input)
		return 0;

	weston_seat_init_pointer(&input->base);

	x11_compositor_setup_xkb(c);

	keymap = x11_compositor_get_keymap(c);
	weston_seat_init_keyboard(&input->base, keymap);
	if (keymap)
		xkb_map_unref(keymap);

	return 0;
}

static void
x11_input_destroy(struct x11_compositor *compositor)
{
	struct x11_input *input = container_of(compositor->base.seat,
					       struct x11_input,
					       base);

	weston_seat_release(&input->base);
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

	c->base.egl_display = eglGetDisplay(c->dpy);
	if (c->base.egl_display == NULL) {
		weston_log("failed to create display\n");
		return -1;
	}

	if (!eglInitialize(c->base.egl_display, &major, &minor)) {
		weston_log("failed to initialize display\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		weston_log("failed to bind EGL_OPENGL_ES_API\n");
		return -1;
	}
	if (!eglChooseConfig(c->base.egl_display, config_attribs,
			     &c->base.egl_config, 1, &n) || n == 0) {
		weston_log("failed to choose config: %d\n", n);
		return -1;
	}

	c->base.egl_context =
		eglCreateContext(c->base.egl_display, c->base.egl_config,
				 EGL_NO_CONTEXT, context_attribs);
	if (c->base.egl_context == NULL) {
		weston_log("failed to create context\n");
		return -1;
	}

	c->dummy_pbuffer = eglCreatePbufferSurface(c->base.egl_display,
						   c->base.egl_config,
						   pbuffer_attribs);
	if (c->dummy_pbuffer == NULL) {
		weston_log("failed to create dummy pbuffer\n");
		return -1;
	}

	if (!eglMakeCurrent(c->base.egl_display, c->dummy_pbuffer,
			    c->dummy_pbuffer, c->base.egl_context)) {
		weston_log("failed to make context current\n");
		return -1;
	}

	return 0;
}

static void
x11_compositor_fini_egl(struct x11_compositor *compositor)
{
	eglMakeCurrent(compositor->base.egl_display,
		       EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglTerminate(compositor->base.egl_display);
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

	if (!eglMakeCurrent(compositor->base.egl_display, output->egl_surface,
			    output->egl_surface,
			    compositor->base.egl_context)) {
		weston_log("failed to make current\n");
		return;
	}

	wl_list_for_each_reverse(surface, &compositor->base.surface_list, link)
		weston_surface_draw(surface, &output->base, damage);

	wl_signal_emit(&output->base.frame_signal, output);

	eglSwapBuffers(compositor->base.egl_display, output->egl_surface);

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

	eglDestroySurface(compositor->base.egl_display, output->egl_surface);

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
			     int width, int height, int fullscreen,
			     int no_input)
{
	static const char name[] = "Weston Compositor";
	static const char class[] = "weston-1\0Weston Compositor";
	struct x11_output *output;
	xcb_screen_iterator_t iter;
	struct wm_normal_hints normal_hints;
	struct wl_event_loop *loop;
	uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	uint32_t values[2] = {
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY,
		0
	};

	if (!no_input)
		values[0] |=
			XCB_EVENT_MASK_KEY_PRESS |
			XCB_EVENT_MASK_KEY_RELEASE |
			XCB_EVENT_MASK_BUTTON_PRESS |
			XCB_EVENT_MASK_BUTTON_RELEASE |
			XCB_EVENT_MASK_POINTER_MOTION |
			XCB_EVENT_MASK_ENTER_WINDOW |
			XCB_EVENT_MASK_LEAVE_WINDOW |
			XCB_EVENT_MASK_KEYMAP_STATE |
			XCB_EVENT_MASK_FOCUS_CHANGE;

	output = malloc(sizeof *output);
	if (output == NULL)
		return -1;

	memset(output, 0, sizeof *output);

	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = width;
	output->mode.height = height;
	output->mode.refresh = 60000;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current = &output->mode;
	output->base.make = "xwayland";
	output->base.model = "none";
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
		eglCreateWindowSurface(c->base.egl_display, c->base.egl_config,
				       output->window, NULL);
	if (!output->egl_surface) {
		weston_log("failed to create window surface\n");
		return -1;
	}
	if (!eglMakeCurrent(c->base.egl_display, output->egl_surface,
			    output->egl_surface, c->base.egl_context)) {
		weston_log("failed to make surface current\n");
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

	weston_log("x11 output %dx%d, window id %d\n",
		   width, height, output->window);

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

static uint32_t
get_xkb_mod_mask(struct x11_compositor *c, uint32_t in)
{
	struct weston_xkb_info *info = &c->base.seat->xkb_info;
	uint32_t ret = 0;

	if ((in & ShiftMask) && info->shift_mod != XKB_MOD_INVALID)
		ret |= (1 << info->shift_mod);
	if ((in & LockMask) && info->caps_mod != XKB_MOD_INVALID)
		ret |= (1 << info->caps_mod);
	if ((in & ControlMask) && info->ctrl_mod != XKB_MOD_INVALID)
		ret |= (1 << info->ctrl_mod);
	if ((in & Mod1Mask) && info->alt_mod != XKB_MOD_INVALID)
		ret |= (1 << info->alt_mod);
	if ((in & Mod2Mask) && info->mod2_mod != XKB_MOD_INVALID)
		ret |= (1 << info->mod2_mod);
	if ((in & Mod3Mask) && info->mod3_mod != XKB_MOD_INVALID)
		ret |= (1 << info->mod3_mod);
	if ((in & Mod4Mask) && info->super_mod != XKB_MOD_INVALID)
		ret |= (1 << info->super_mod);
	if ((in & Mod5Mask) && info->mod5_mod != XKB_MOD_INVALID)
		ret |= (1 << info->mod5_mod);

	return ret;
}

#ifdef HAVE_XCB_XKB
static void
update_xkb_state(struct x11_compositor *c, xcb_xkb_state_notify_event_t *state)
{
	struct weston_compositor *ec = &c->base;
	struct wl_seat *seat = &ec->seat->seat;

	xkb_state_update_mask(c->base.seat->xkb_state.state,
			      get_xkb_mod_mask(c, state->baseMods),
			      get_xkb_mod_mask(c, state->latchedMods),
			      get_xkb_mod_mask(c, state->lockedMods),
			      0,
			      0,
			      state->group);

	notify_modifiers(seat, wl_display_next_serial(c->base.wl_display));
}
#endif

/**
 * This is monumentally unpleasant.  If we don't have XCB-XKB bindings,
 * the best we can do (given that XCB also lacks XI2 support), is to take
 * the state from the core key events.  Unfortunately that only gives us
 * the effective (i.e. union of depressed/latched/locked) state, and we
 * need the granularity.
 *
 * So we still update the state with every key event we see, but also use
 * the state field from X11 events as a mask so we don't get any stuck
 * modifiers.
 */
static void
update_xkb_state_from_core(struct x11_compositor *c, uint16_t x11_mask)
{
	uint32_t mask = get_xkb_mod_mask(c, x11_mask);
	struct wl_keyboard *keyboard = &c->base.seat->keyboard;

	xkb_state_update_mask(c->base.seat->xkb_state.state,
			      keyboard->modifiers.mods_depressed & mask,
			      keyboard->modifiers.mods_latched & mask,
			      keyboard->modifiers.mods_locked & mask,
			      0,
			      0,
			      (x11_mask >> 13) & 3);
	notify_modifiers(&c->base.seat->seat,
			 wl_display_next_serial(c->base.wl_display));
}

static void
x11_compositor_deliver_button_event(struct x11_compositor *c,
				    xcb_generic_event_t *event, int state)
{
	xcb_button_press_event_t *button_event =
		(xcb_button_press_event_t *) event;
	uint32_t button;

	if (!c->has_xkb)
		update_xkb_state_from_core(c, button_event->state);

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
			notify_axis(&c->base.seat->seat,
				      weston_compositor_get_time(),
				      WL_POINTER_AXIS_VERTICAL_SCROLL,
				      wl_fixed_from_int(1));
		return;
	case 5:
		if (state)
			notify_axis(&c->base.seat->seat,
				      weston_compositor_get_time(),
				      WL_POINTER_AXIS_VERTICAL_SCROLL,
				      wl_fixed_from_int(-1));
		return;
	case 6:
		if (state)
			notify_axis(&c->base.seat->seat,
				      weston_compositor_get_time(),
				      WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				      wl_fixed_from_int(1));
		return;
	case 7:
		if (state)
			notify_axis(&c->base.seat->seat,
				      weston_compositor_get_time(),
				      WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				      wl_fixed_from_int(-1));
		return;
	}

	notify_button(&c->base.seat->seat,
		      weston_compositor_get_time(), button,
		      state ? WL_POINTER_BUTTON_STATE_PRESSED :
			      WL_POINTER_BUTTON_STATE_RELEASED);
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
	xcb_expose_event_t *expose;
	xcb_atom_t atom;
	uint32_t *k;
	uint32_t i, set;
	wl_fixed_t x, y;
	int count;

	prev = NULL;
	count = 0;
	while (x11_compositor_next_event(c, &event, mask)) {
		switch (prev ? prev->response_type & ~0x80 : 0x80) {
		case XCB_KEY_RELEASE:
			/* Suppress key repeat events; this is only used if we
			 * don't have XCB XKB support. */
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
				update_xkb_state_from_core(c, key_release->state);
				notify_key(&c->base.seat->seat,
					   weston_compositor_get_time(),
					   key_release->detail - 8,
					   WL_KEYBOARD_KEY_STATE_RELEASED,
					   STATE_UPDATE_AUTOMATIC);
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
			/* Unfortunately the state only comes with the enter
			 * event, rather than with the focus event.  I'm not
			 * sure of the exact semantics around it and whether
			 * we can ensure that we get both? */
			notify_keyboard_focus_in(&c->base.seat->seat, &c->keys,
						 STATE_UPDATE_AUTOMATIC);

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
			if (!c->has_xkb)
				update_xkb_state_from_core(c, key_press->state);
			notify_key(&c->base.seat->seat,
				   weston_compositor_get_time(),
				   key_press->detail - 8,
				   WL_KEYBOARD_KEY_STATE_PRESSED,
				   c->has_xkb ? STATE_UPDATE_NONE :
						STATE_UPDATE_AUTOMATIC);
			break;
		case XCB_KEY_RELEASE:
			/* If we don't have XKB, we need to use the lame
			 * autorepeat detection above. */
			if (!c->has_xkb) {
				prev = event;
				break;
			}
			key_release = (xcb_key_press_event_t *) event;
			notify_key(&c->base.seat->seat,
				   weston_compositor_get_time(),
				   key_release->detail - 8,
				   WL_KEYBOARD_KEY_STATE_RELEASED,
				   STATE_UPDATE_NONE);
			break;
		case XCB_BUTTON_PRESS:
			x11_compositor_deliver_button_event(c, event, 1);
			break;
		case XCB_BUTTON_RELEASE:
			x11_compositor_deliver_button_event(c, event, 0);
			break;
		case XCB_MOTION_NOTIFY:
			motion_notify = (xcb_motion_notify_event_t *) event;
			if (!c->has_xkb)
				update_xkb_state_from_core(c, motion_notify->state);
			output = x11_compositor_find_output(c, motion_notify->event);
			x = wl_fixed_from_int(output->base.x + motion_notify->event_x);
			y = wl_fixed_from_int(output->base.y + motion_notify->event_y);
			notify_motion(&c->base.seat->seat,
				      weston_compositor_get_time(), x, y);
			break;

		case XCB_EXPOSE:
			expose = (xcb_expose_event_t *) event;
			output = x11_compositor_find_output(c, expose->window);
			weston_output_schedule_repaint(&output->base);
			break;

		case XCB_ENTER_NOTIFY:
			enter_notify = (xcb_enter_notify_event_t *) event;
			if (enter_notify->state >= Button1Mask)
				break;
			if (!c->has_xkb)
				update_xkb_state_from_core(c, enter_notify->state);
			output = x11_compositor_find_output(c, enter_notify->event);
			x = wl_fixed_from_int(output->base.x + enter_notify->event_x);
			y = wl_fixed_from_int(output->base.y + enter_notify->event_y);

			notify_pointer_focus(&c->base.seat->seat,
					     &output->base, x, y);
			break;

		case XCB_LEAVE_NOTIFY:
			enter_notify = (xcb_enter_notify_event_t *) event;
			if (enter_notify->state >= Button1Mask)
				break;
			if (!c->has_xkb)
				update_xkb_state_from_core(c, enter_notify->state);
			output = x11_compositor_find_output(c, enter_notify->event);
			notify_pointer_focus(&c->base.seat->seat, NULL, 0, 0);
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
			notify_keyboard_focus_out(&c->base.seat->seat);
			break;

		default:
			break;
		}

#ifdef HAVE_XCB_XKB
		if (c->has_xkb &&
		    (event->response_type & ~0x80) == c->xkb_event_base) {
			xcb_xkb_state_notify_event_t *state =
				(xcb_xkb_state_notify_event_t *) event;
			if (state->xkbType == XCB_XKB_STATE_NOTIFY)
				update_xkb_state(c, state);
		}
#endif

		count++;
		if (prev != event)
			free (event);
	}

	switch (prev ? prev->response_type & ~0x80 : 0x80) {
	case XCB_KEY_RELEASE:
		key_release = (xcb_key_press_event_t *) prev;
		update_xkb_state_from_core(c, key_release->state);
		notify_key(&c->base.seat->seat,
			   weston_compositor_get_time(),
			   key_release->detail - 8,
			   WL_KEYBOARD_KEY_STATE_RELEASED,
			   STATE_UPDATE_AUTOMATIC);
		free(prev);
		prev = NULL;
		break;
	default:
		break;
	}

	return count;
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
		{ "_XKB_RULES_NAMES",	F(atom.xkb_names) },
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
x11_restore(struct weston_compositor *ec)
{
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
		      int width, int height, int count, int fullscreen,
		      int no_input,
		      int argc, char *argv[], const char *config_file)
{
	struct x11_compositor *c;
	xcb_screen_iterator_t s;
	int i, x;

	weston_log("initializing x11 backend\n");

	c = malloc(sizeof *c);
	if (c == NULL)
		return NULL;

	memset(c, 0, sizeof *c);

	if (weston_compositor_init(&c->base, display, argc, argv,
				   config_file) < 0)
		goto err_free;

	c->dpy = XOpenDisplay(NULL);
	if (c->dpy == NULL)
		goto err_free;

	c->conn = XGetXCBConnection(c->dpy);
	XSetEventQueueOwner(c->dpy, XCBOwnsEventQueue);

	if (xcb_connection_has_error(c->conn))
		goto err_xdisplay;

	s = xcb_setup_roots_iterator(xcb_get_setup(c->conn));
	c->screen = s.data;
	wl_array_init(&c->keys);

	x11_compositor_get_resources(c);

	c->base.wl_display = display;
	if (x11_compositor_init_egl(c) < 0)
		goto err_xdisplay;

	c->base.destroy = x11_destroy;
	c->base.restore = x11_restore;

	if (weston_compositor_init_gl(&c->base) < 0)
		goto err_egl;

	if (x11_input_create(c, no_input) < 0)
		goto err_egl;

	for (i = 0, x = 0; i < count; i++) {
		if (x11_compositor_create_output(c, x, 0, width, height,
						 fullscreen, no_input) < 0)
			goto err_x11_input;
		x += width;
	}

	c->xcb_source =
		wl_event_loop_add_fd(c->base.input_loop,
				     xcb_get_file_descriptor(c->conn),
				     WL_EVENT_READABLE,
				     x11_compositor_handle_event, c);
	wl_event_source_check(c->xcb_source);

	return &c->base;

err_x11_input:
	x11_input_destroy(c);
err_egl:
	x11_compositor_fini_egl(c);
err_xdisplay:
	XCloseDisplay(c->dpy);
err_free:
	free(c);
	return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[],
	     const char *config_file)
{
	int width = 1024, height = 640, fullscreen = 0, count = 1;
	int no_input = 0;

	const struct weston_option x11_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &width },
		{ WESTON_OPTION_INTEGER, "height", 0, &height },
		{ WESTON_OPTION_BOOLEAN, "fullscreen", 0, &fullscreen },
		{ WESTON_OPTION_INTEGER, "output-count", 0, &count },
		{ WESTON_OPTION_BOOLEAN, "no-input", 0, &no_input },
	};

	parse_options(x11_options, ARRAY_LENGTH(x11_options), argc, argv);

	return x11_compositor_create(display,
				     width, height, count, fullscreen,
				     no_input,
				     argc, argv, config_file);
}

/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2013 Vasily Khoruzhick <anarsoul@gmail.com>
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

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <linux/input.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>
#ifdef HAVE_XCB_XKB
#include <xcb/xkb.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include <xkbcommon/xkbcommon.h>

#include "compositor.h"
#include "gl-renderer.h"
#include "pixman-renderer.h"
#include "../shared/config-parser.h"
#include "../shared/image-loader.h"
#include "presentation_timing-server-protocol.h"

#define DEFAULT_AXIS_STEP_DISTANCE wl_fixed_from_int(10)

static int option_width;
static int option_height;
static int option_scale;
static int option_count;

struct x11_compositor {
	struct weston_compositor	 base;

	Display			*dpy;
	xcb_connection_t	*conn;
	xcb_screen_t		*screen;
	xcb_cursor_t		 null_cursor;
	struct wl_array		 keys;
	struct wl_event_source	*xcb_source;
	struct xkb_keymap	*xkb_keymap;
	unsigned int		 has_xkb;
	uint8_t			 xkb_event_base;
	int			 use_pixman;

	int			 has_net_wm_state_fullscreen;

	/* We could map multi-pointer X to multiple wayland seats, but
	 * for now we only support core X input. */
	struct weston_seat		 core_seat;
	wl_fixed_t			 prev_x;
	wl_fixed_t			 prev_y;

	struct {
		xcb_atom_t		 wm_protocols;
		xcb_atom_t		 wm_normal_hints;
		xcb_atom_t		 wm_size_hints;
		xcb_atom_t		 wm_delete_window;
		xcb_atom_t		 wm_class;
		xcb_atom_t		 net_wm_name;
		xcb_atom_t		 net_supporting_wm_check;
		xcb_atom_t		 net_supported;
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
	struct weston_mode	mode;
	struct wl_event_source *finish_frame_timer;

	xcb_gc_t		gc;
	xcb_shm_seg_t		segment;
	pixman_image_t	       *hw_surface;
	int			shm_id;
	void		       *buf;
	uint8_t			depth;
	int32_t                 scale;
};

struct window_delete_data {
	struct x11_compositor	*compositor;
	xcb_window_t		window;
};

struct gl_renderer_interface *gl_renderer;

static struct xkb_keymap *
x11_compositor_get_keymap(struct x11_compositor *c)
{
	xcb_get_property_cookie_t cookie;
	xcb_get_property_reply_t *reply;
	struct xkb_rule_names names;
	struct xkb_keymap *ret;
	const char *value_all, *value_part;
	int length_all, length_part;

	memset(&names, 0, sizeof(names));

	cookie = xcb_get_property(c->conn, 0, c->screen->root,
				  c->atom.xkb_names, c->atom.string, 0, 1024);
	reply = xcb_get_property_reply(c->conn, cookie, NULL);
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

	ret = xkb_keymap_new_from_names(c->base.xkb_context, &names, 0);

	free(reply);
	return ret;
}

static uint32_t
get_xkb_mod_mask(struct x11_compositor *c, uint32_t in)
{
	struct weston_xkb_info *info = c->core_seat.keyboard->xkb_info;
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
	xcb_xkb_use_extension_cookie_t use_ext;
	xcb_xkb_use_extension_reply_t *use_ext_reply;
	xcb_xkb_per_client_flags_cookie_t pcf;
	xcb_xkb_per_client_flags_reply_t *pcf_reply;
	xcb_xkb_get_state_cookie_t state;
	xcb_xkb_get_state_reply_t *state_reply;
	uint32_t values[1] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

	c->has_xkb = 0;
	c->xkb_event_base = 0;

	ext = xcb_get_extension_data(c->conn, &xcb_xkb_id);
	if (!ext) {
		weston_log("XKB extension not available on host X11 server\n");
		return;
	}
	c->xkb_event_base = ext->first_event;

	select = xcb_xkb_select_events_checked(c->conn,
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
		free(error);
		return;
	}

	use_ext = xcb_xkb_use_extension(c->conn,
					XCB_XKB_MAJOR_VERSION,
					XCB_XKB_MINOR_VERSION);
	use_ext_reply = xcb_xkb_use_extension_reply(c->conn, use_ext, NULL);
	if (!use_ext_reply) {
		weston_log("couldn't start using XKB extension\n");
		return;
	}

	if (!use_ext_reply->supported) {
		weston_log("XKB extension version on the server is too old "
			   "(want %d.%d, has %d.%d)\n",
			   XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION,
			   use_ext_reply->serverMajor, use_ext_reply->serverMinor);
		free(use_ext_reply);
		return;
	}
	free(use_ext_reply);

	pcf = xcb_xkb_per_client_flags(c->conn,
				       XCB_XKB_ID_USE_CORE_KBD,
				       XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
				       XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
				       0,
				       0,
				       0);
	pcf_reply = xcb_xkb_per_client_flags_reply(c->conn, pcf, NULL);
	if (!pcf_reply ||
	    !(pcf_reply->value & XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT)) {
		weston_log("failed to set XKB per-client flags, not using "
			   "detectable repeat\n");
		free(pcf_reply);
		return;
	}
	free(pcf_reply);

	state = xcb_xkb_get_state(c->conn, XCB_XKB_ID_USE_CORE_KBD);
	state_reply = xcb_xkb_get_state_reply(c->conn, state, NULL);
	if (!state_reply) {
		weston_log("failed to get initial XKB state\n");
		return;
	}

	xkb_state_update_mask(c->core_seat.keyboard->xkb_state.state,
			      get_xkb_mod_mask(c, state_reply->baseMods),
			      get_xkb_mod_mask(c, state_reply->latchedMods),
			      get_xkb_mod_mask(c, state_reply->lockedMods),
			      0,
			      0,
			      state_reply->group);

	free(state_reply);

	xcb_change_window_attributes(c->conn, c->screen->root,
				     XCB_CW_EVENT_MASK, values);

	c->has_xkb = 1;
#endif
}

#ifdef HAVE_XCB_XKB
static void
update_xkb_keymap(struct x11_compositor *c)
{
	struct xkb_keymap *keymap;

	keymap = x11_compositor_get_keymap(c);
	if (!keymap) {
		weston_log("failed to get XKB keymap\n");
		return;
	}
	weston_seat_update_keymap(&c->core_seat, keymap);
	xkb_keymap_unref(keymap);
}
#endif

static int
x11_input_create(struct x11_compositor *c, int no_input)
{
	struct xkb_keymap *keymap;

	weston_seat_init(&c->core_seat, &c->base, "default");

	if (no_input)
		return 0;

	weston_seat_init_pointer(&c->core_seat);

	keymap = x11_compositor_get_keymap(c);
	if (weston_seat_init_keyboard(&c->core_seat, keymap) < 0)
		return -1;
	xkb_keymap_unref(keymap);

	x11_compositor_setup_xkb(c);

	return 0;
}

static void
x11_input_destroy(struct x11_compositor *compositor)
{
	weston_seat_release(&compositor->core_seat);
}

static void
x11_output_start_repaint_loop(struct weston_output *output)
{
	struct timespec ts;

	weston_compositor_read_presentation_clock(output->compositor, &ts);
	weston_output_finish_frame(output, &ts, PRESENTATION_FEEDBACK_INVALID);
}

static int
x11_output_repaint_gl(struct weston_output *output_base,
		      pixman_region32_t *damage)
{
	struct x11_output *output = (struct x11_output *)output_base;
	struct weston_compositor *ec = output->base.compositor;

	ec->renderer->repaint_output(output_base, damage);

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, damage);

	wl_event_source_timer_update(output->finish_frame_timer, 10);
	return 0;
}

static void
set_clip_for_output(struct weston_output *output_base, pixman_region32_t *region)
{
	struct x11_output *output = (struct x11_output *)output_base;
	struct weston_compositor *ec = output->base.compositor;
	struct x11_compositor *c = (struct x11_compositor *)ec;
	pixman_region32_t transformed_region;
	pixman_box32_t *rects;
	xcb_rectangle_t *output_rects;
	xcb_void_cookie_t cookie;
	int nrects, i;
	xcb_generic_error_t *err;

	pixman_region32_init(&transformed_region);
	pixman_region32_copy(&transformed_region, region);
	pixman_region32_translate(&transformed_region,
				  -output_base->x, -output_base->y);
	weston_transformed_region(output_base->width, output_base->height,
				  output_base->transform,
				  output_base->current_scale,
				  &transformed_region, &transformed_region);

	rects = pixman_region32_rectangles(&transformed_region, &nrects);
	output_rects = calloc(nrects, sizeof(xcb_rectangle_t));

	if (output_rects == NULL) {
		pixman_region32_fini(&transformed_region);
		return;
	}

	for (i = 0; i < nrects; i++) {
		output_rects[i].x = rects[i].x1;
		output_rects[i].y = rects[i].y1;
		output_rects[i].width = rects[i].x2 - rects[i].x1;
		output_rects[i].height = rects[i].y2 - rects[i].y1;
	}

	pixman_region32_fini(&transformed_region);

	cookie = xcb_set_clip_rectangles_checked(c->conn, XCB_CLIP_ORDERING_UNSORTED,
					output->gc,
					0, 0, nrects,
					output_rects);
	err = xcb_request_check(c->conn, cookie);
	if (err != NULL) {
		weston_log("Failed to set clip rects, err: %d\n", err->error_code);
		free(err);
	}
	free(output_rects);
}


static int
x11_output_repaint_shm(struct weston_output *output_base,
		       pixman_region32_t *damage)
{
	struct x11_output *output = (struct x11_output *)output_base;
	struct weston_compositor *ec = output->base.compositor;
	struct x11_compositor *c = (struct x11_compositor *)ec;
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *err;

	pixman_renderer_output_set_buffer(output_base, output->hw_surface);
	ec->renderer->repaint_output(output_base, damage);

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, damage);
	set_clip_for_output(output_base, damage);
	cookie = xcb_shm_put_image_checked(c->conn, output->window, output->gc,
					pixman_image_get_width(output->hw_surface),
					pixman_image_get_height(output->hw_surface),
					0, 0,
					pixman_image_get_width(output->hw_surface),
					pixman_image_get_height(output->hw_surface),
					0, 0, output->depth, XCB_IMAGE_FORMAT_Z_PIXMAP,
					0, output->segment, 0);
	err = xcb_request_check(c->conn, cookie);
	if (err != NULL) {
		weston_log("Failed to put shm image, err: %d\n", err->error_code);
		free(err);
	}

	wl_event_source_timer_update(output->finish_frame_timer, 10);
	return 0;
}

static int
finish_frame_handler(void *data)
{
	struct x11_output *output = data;
	struct timespec ts;

	weston_compositor_read_presentation_clock(output->base.compositor, &ts);
	weston_output_finish_frame(&output->base, &ts, 0);

	return 1;
}

static void
x11_output_deinit_shm(struct x11_compositor *c, struct x11_output *output)
{
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *err;
	xcb_free_gc(c->conn, output->gc);

	pixman_image_unref(output->hw_surface);
	output->hw_surface = NULL;
	cookie = xcb_shm_detach_checked(c->conn, output->segment);
	err = xcb_request_check(c->conn, cookie);
	if (err) {
		weston_log("xcb_shm_detach failed, error %d\n", err->error_code);
		free(err);
	}
	shmdt(output->buf);
}

static void
x11_output_destroy(struct weston_output *output_base)
{
	struct x11_output *output = (struct x11_output *)output_base;
	struct x11_compositor *compositor =
		(struct x11_compositor *)output->base.compositor;

	wl_event_source_remove(output->finish_frame_timer);

	if (compositor->use_pixman) {
		pixman_renderer_output_destroy(output_base);
		x11_output_deinit_shm(compositor, output);
	} else
		gl_renderer->output_destroy(output_base);

	xcb_destroy_window(compositor->conn, output->window);

	weston_output_destroy(&output->base);

	free(output);
}

static void
x11_output_set_wm_protocols(struct x11_compositor *c,
			    struct x11_output *output)
{
	xcb_atom_t list[1];

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

static void
x11_output_wait_for_map(struct x11_compositor *c, struct x11_output *output)
{
	xcb_map_notify_event_t *map_notify;
	xcb_configure_notify_event_t *configure_notify;
	xcb_generic_event_t *event;
	int mapped = 0, configured = 0;
	uint8_t response_type;

	/* This isn't the nicest way to do this.  Ideally, we could
	 * just go back to the main loop and once we get the configure
	 * notify, we add the output to the compositor.  While we do
	 * support output hotplug, we can't start up with no outputs.
	 * We could add the output and then resize once we get the
	 * configure notify, but we don't want to start up and
	 * immediately resize the output.
	 *
	 * Also, some window managers don't give us our final
	 * fullscreen size before map_notify, so if we don't get a
	 * configure_notify before map_notify, we just wait for the
	 * first one and hope that's our size. */

	xcb_flush(c->conn);

	while (!mapped || !configured) {
		event = xcb_wait_for_event(c->conn);
		response_type = event->response_type & ~0x80;

		switch (response_type) {
		case XCB_MAP_NOTIFY:
			map_notify = (xcb_map_notify_event_t *) event;
			if (map_notify->window == output->window)
				mapped = 1;
			break;

		case XCB_CONFIGURE_NOTIFY:
			configure_notify =
				(xcb_configure_notify_event_t *) event;


			if (configure_notify->width % output->scale != 0 ||
			    configure_notify->height % output->scale != 0)
				weston_log("Resolution is not a multiple of screen size, rounding\n");
			output->mode.width = configure_notify->width;
			output->mode.height = configure_notify->height;
			configured = 1;
			break;
		}
	}
}

static xcb_visualtype_t *
find_visual_by_id(xcb_screen_t *screen,
		   xcb_visualid_t id)
{
	xcb_depth_iterator_t i;
	xcb_visualtype_iterator_t j;
	for (i = xcb_screen_allowed_depths_iterator(screen);
	     i.rem;
	     xcb_depth_next(&i)) {
		for (j = xcb_depth_visuals_iterator(i.data);
		     j.rem;
		     xcb_visualtype_next(&j)) {
			if (j.data->visual_id == id)
				return j.data;
		}
	}
	return 0;
}

static uint8_t
get_depth_of_visual(xcb_screen_t *screen,
		   xcb_visualid_t id)
{
	xcb_depth_iterator_t i;
	xcb_visualtype_iterator_t j;
	for (i = xcb_screen_allowed_depths_iterator(screen);
	     i.rem;
	     xcb_depth_next(&i)) {
		for (j = xcb_depth_visuals_iterator(i.data);
		     j.rem;
		     xcb_visualtype_next(&j)) {
			if (j.data->visual_id == id)
				return i.data->depth;
		}
	}
	return 0;
}

static int
x11_output_init_shm(struct x11_compositor *c, struct x11_output *output,
	int width, int height)
{
	xcb_screen_iterator_t iter;
	xcb_visualtype_t *visual_type;
	xcb_format_iterator_t fmt;
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *err;
	const xcb_query_extension_reply_t *ext;
	int bitsperpixel = 0;
	pixman_format_code_t pixman_format;

	/* Check if SHM is available */
	ext = xcb_get_extension_data(c->conn, &xcb_shm_id);
	if (ext == NULL || !ext->present) {
		/* SHM is missing */
		weston_log("SHM extension is not available\n");
		errno = ENOENT;
		return -1;
	}

	iter = xcb_setup_roots_iterator(xcb_get_setup(c->conn));
	visual_type = find_visual_by_id(iter.data, iter.data->root_visual);
	if (!visual_type) {
		weston_log("Failed to lookup visual for root window\n");
		errno = ENOENT;
		return -1;
	}
	weston_log("Found visual, bits per value: %d, red_mask: %.8x, green_mask: %.8x, blue_mask: %.8x\n",
		visual_type->bits_per_rgb_value,
		visual_type->red_mask,
		visual_type->green_mask,
		visual_type->blue_mask);
	output->depth = get_depth_of_visual(iter.data, iter.data->root_visual);
	weston_log("Visual depth is %d\n", output->depth);

	for (fmt = xcb_setup_pixmap_formats_iterator(xcb_get_setup(c->conn));
	     fmt.rem;
	     xcb_format_next(&fmt)) {
		if (fmt.data->depth == output->depth) {
			bitsperpixel = fmt.data->bits_per_pixel;
			break;
		}
	}
	weston_log("Found format for depth %d, bpp: %d\n",
		output->depth, bitsperpixel);

	if  (bitsperpixel == 32 &&
	     visual_type->red_mask == 0xff0000 &&
	     visual_type->green_mask == 0x00ff00 &&
	     visual_type->blue_mask == 0x0000ff) {
		weston_log("Will use x8r8g8b8 format for SHM surfaces\n");
		pixman_format = PIXMAN_x8r8g8b8;
	} else if (bitsperpixel == 16 &&
	           visual_type->red_mask == 0x00f800 &&
	           visual_type->green_mask == 0x0007e0 &&
	           visual_type->blue_mask == 0x00001f) {
		weston_log("Will use r5g6b5 format for SHM surfaces\n");
		pixman_format = PIXMAN_r5g6b5;
	} else {
		weston_log("Can't find appropriate format for SHM pixmap\n");
		errno = ENOTSUP;
		return -1;
	}


	/* Create SHM segment and attach it */
	output->shm_id = shmget(IPC_PRIVATE, width * height * (bitsperpixel / 8), IPC_CREAT | S_IRWXU);
	if (output->shm_id == -1) {
		weston_log("x11shm: failed to allocate SHM segment\n");
		return -1;
	}
	output->buf = shmat(output->shm_id, NULL, 0 /* read/write */);
	if (-1 == (long)output->buf) {
		weston_log("x11shm: failed to attach SHM segment\n");
		return -1;
	}
	output->segment = xcb_generate_id(c->conn);
	cookie = xcb_shm_attach_checked(c->conn, output->segment, output->shm_id, 1);
	err = xcb_request_check(c->conn, cookie);
	if (err) {
		weston_log("x11shm: xcb_shm_attach error %d\n", err->error_code);
		free(err);
		return -1;
	}

	shmctl(output->shm_id, IPC_RMID, NULL);

	/* Now create pixman image */
	output->hw_surface = pixman_image_create_bits(pixman_format, width, height, output->buf,
		width * (bitsperpixel / 8));

	output->gc = xcb_generate_id(c->conn);
	xcb_create_gc(c->conn, output->gc, output->window, 0, NULL);

	return 0;
}

static struct x11_output *
x11_compositor_create_output(struct x11_compositor *c, int x, int y,
			     int width, int height, int fullscreen,
			     int no_input, char *configured_name,
			     uint32_t transform, int32_t scale)
{
	static const char name[] = "Weston Compositor";
	static const char class[] = "weston-1\0Weston Compositor";
	char title[32];
	struct x11_output *output;
	xcb_screen_iterator_t iter;
	struct wm_normal_hints normal_hints;
	struct wl_event_loop *loop;
	int output_width, output_height, width_mm, height_mm;
	int ret;
	uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	xcb_atom_t atom_list[1];
	uint32_t values[2] = {
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY,
		0
	};

	output_width = width * scale;
	output_height = height * scale;

	if (configured_name)
		sprintf(title, "%s - %s", name, configured_name);
	else
		strcpy(title, name);

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

	output = zalloc(sizeof *output);
	if (output == NULL)
		return NULL;

	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	output->mode.width = output_width;
	output->mode.height = output_height;
	output->mode.refresh = 60000;
	output->scale = scale;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	values[1] = c->null_cursor;
	output->window = xcb_generate_id(c->conn);
	iter = xcb_setup_roots_iterator(xcb_get_setup(c->conn));
	xcb_create_window(c->conn,
			  XCB_COPY_FROM_PARENT,
			  output->window,
			  iter.data->root,
			  0, 0,
			  output_width, output_height,
			  0,
			  XCB_WINDOW_CLASS_INPUT_OUTPUT,
			  iter.data->root_visual,
			  mask, values);

	if (fullscreen) {
		atom_list[0] = c->atom.net_wm_state_fullscreen;
		xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE,
				    output->window,
				    c->atom.net_wm_state,
				    XCB_ATOM_ATOM, 32,
				    ARRAY_LENGTH(atom_list), atom_list);
	} else {
		/* Don't resize me. */
		memset(&normal_hints, 0, sizeof normal_hints);
		normal_hints.flags =
			WM_NORMAL_HINTS_MAX_SIZE | WM_NORMAL_HINTS_MIN_SIZE;
		normal_hints.min_width = output_width;
		normal_hints.min_height = output_height;
		normal_hints.max_width = output_width;
		normal_hints.max_height = output_height;
		xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE, output->window,
				    c->atom.wm_normal_hints,
				    c->atom.wm_size_hints, 32,
				    sizeof normal_hints / 4,
				    (uint8_t *) &normal_hints);
	}

	/* Set window name.  Don't bother with non-EWMH WMs. */
	xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE, output->window,
			    c->atom.net_wm_name, c->atom.utf8_string, 8,
			    strlen(title), title);
	xcb_change_property(c->conn, XCB_PROP_MODE_REPLACE, output->window,
			    c->atom.wm_class, c->atom.string, 8,
			    sizeof class, class);

	x11_output_set_icon(c, output, DATADIR "/weston/wayland.png");

	x11_output_set_wm_protocols(c, output);

	xcb_map_window(c->conn, output->window);

	if (fullscreen)
		x11_output_wait_for_map(c, output);

	output->base.start_repaint_loop = x11_output_start_repaint_loop;
	if (c->use_pixman)
		output->base.repaint = x11_output_repaint_shm;
	else
		output->base.repaint = x11_output_repaint_gl;
	output->base.destroy = x11_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;
	output->base.current_mode = &output->mode;
	output->base.make = "weston-X11";
	output->base.model = "none";

	if (configured_name)
		output->base.name = strdup(configured_name);

	width_mm = width * c->screen->width_in_millimeters /
		c->screen->width_in_pixels;
	height_mm = height * c->screen->height_in_millimeters /
		c->screen->height_in_pixels;
	weston_output_init(&output->base, &c->base,
			   x, y, width_mm, height_mm, transform, scale);

	if (c->use_pixman) {
		if (x11_output_init_shm(c, output,
					output->mode.width,
					output->mode.height) < 0)
			return NULL;
		if (pixman_renderer_output_create(&output->base) < 0) {
			x11_output_deinit_shm(c, output);
			return NULL;
		}
	} else {
		/* eglCreatePlatformWindowSurfaceEXT takes a Window*
		 * but eglCreateWindowSurface takes a Window. */
		Window xid = (Window) output->window;

		ret = gl_renderer->output_create(&output->base,
						 (EGLNativeWindowType) output->window,
						 &xid,
						 gl_renderer->opaque_attribs,
						 NULL,
						 0);
		if (ret < 0)
			return NULL;
	}

	loop = wl_display_get_event_loop(c->base.wl_display);
	output->finish_frame_timer =
		wl_event_loop_add_timer(loop, finish_frame_handler, output);

	weston_compositor_add_output(&c->base, &output->base);

	weston_log("x11 output %dx%d, window id %d\n",
		   width, height, output->window);

	return output;
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
x11_compositor_delete_window(struct x11_compositor *c, xcb_window_t window)
{
	struct x11_output *output;

	output = x11_compositor_find_output(c, window);
	if (output)
		x11_output_destroy(&output->base);

	xcb_flush(c->conn);

	if (wl_list_empty(&c->base.output_list))
		wl_display_terminate(c->base.wl_display);
}

static void delete_cb(void *data)
{
	struct window_delete_data *wd = data;

	x11_compositor_delete_window(wd->compositor, wd->window);
	free(wd);
}

#ifdef HAVE_XCB_XKB
static void
update_xkb_state(struct x11_compositor *c, xcb_xkb_state_notify_event_t *state)
{
	xkb_state_update_mask(c->core_seat.keyboard->xkb_state.state,
			      get_xkb_mod_mask(c, state->baseMods),
			      get_xkb_mod_mask(c, state->latchedMods),
			      get_xkb_mod_mask(c, state->lockedMods),
			      0,
			      0,
			      state->group);

	notify_modifiers(&c->core_seat,
			 wl_display_next_serial(c->base.wl_display));
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
	struct weston_keyboard *keyboard = c->core_seat.keyboard;

	xkb_state_update_mask(c->core_seat.keyboard->xkb_state.state,
			      keyboard->modifiers.mods_depressed & mask,
			      keyboard->modifiers.mods_latched & mask,
			      keyboard->modifiers.mods_locked & mask,
			      0,
			      0,
			      (x11_mask >> 13) & 3);
	notify_modifiers(&c->core_seat,
			 wl_display_next_serial(c->base.wl_display));
}

static void
x11_compositor_deliver_button_event(struct x11_compositor *c,
				    xcb_generic_event_t *event, int state)
{
	xcb_button_press_event_t *button_event =
		(xcb_button_press_event_t *) event;
	uint32_t button;
	struct x11_output *output;

	output = x11_compositor_find_output(c, button_event->event);
	if (!output)
		return;

	if (state)
		xcb_grab_pointer(c->conn, 0, output->window,
				 XCB_EVENT_MASK_BUTTON_PRESS |
				 XCB_EVENT_MASK_BUTTON_RELEASE |
				 XCB_EVENT_MASK_POINTER_MOTION |
				 XCB_EVENT_MASK_ENTER_WINDOW |
				 XCB_EVENT_MASK_LEAVE_WINDOW,
				 XCB_GRAB_MODE_ASYNC,
				 XCB_GRAB_MODE_ASYNC,
				 output->window, XCB_CURSOR_NONE,
				 button_event->time);
	else
		xcb_ungrab_pointer(c->conn, button_event->time);

	if (!c->has_xkb)
		update_xkb_state_from_core(c, button_event->state);

	switch (button_event->detail) {
	case 1:
		button = BTN_LEFT;
		break;
	case 2:
		button = BTN_MIDDLE;
		break;
	case 3:
		button = BTN_RIGHT;
		break;
	case 4:
		/* Axis are measured in pixels, but the xcb events are discrete
		 * steps. Therefore move the axis by some pixels every step. */
		if (state)
			notify_axis(&c->core_seat,
				    weston_compositor_get_time(),
				    WL_POINTER_AXIS_VERTICAL_SCROLL,
				    -DEFAULT_AXIS_STEP_DISTANCE);
		return;
	case 5:
		if (state)
			notify_axis(&c->core_seat,
				    weston_compositor_get_time(),
				    WL_POINTER_AXIS_VERTICAL_SCROLL,
				    DEFAULT_AXIS_STEP_DISTANCE);
		return;
	case 6:
		if (state)
			notify_axis(&c->core_seat,
				    weston_compositor_get_time(),
				    WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				    -DEFAULT_AXIS_STEP_DISTANCE);
		return;
	case 7:
		if (state)
			notify_axis(&c->core_seat,
				    weston_compositor_get_time(),
				    WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				    DEFAULT_AXIS_STEP_DISTANCE);
		return;
	default:
		button = button_event->detail + BTN_SIDE - 8;
		break;
	}

	notify_button(&c->core_seat,
		      weston_compositor_get_time(), button,
		      state ? WL_POINTER_BUTTON_STATE_PRESSED :
			      WL_POINTER_BUTTON_STATE_RELEASED);
}

static void
x11_compositor_deliver_motion_event(struct x11_compositor *c,
					xcb_generic_event_t *event)
{
	struct x11_output *output;
	wl_fixed_t x, y;
	xcb_motion_notify_event_t *motion_notify =
			(xcb_motion_notify_event_t *) event;

	if (!c->has_xkb)
		update_xkb_state_from_core(c, motion_notify->state);
	output = x11_compositor_find_output(c, motion_notify->event);
	if (!output)
		return;

	weston_output_transform_coordinate(&output->base,
					   wl_fixed_from_int(motion_notify->event_x),
					   wl_fixed_from_int(motion_notify->event_y),
					   &x, &y);

	notify_motion(&c->core_seat, weston_compositor_get_time(),
		      x - c->prev_x, y - c->prev_y);

	c->prev_x = x;
	c->prev_y = y;
}

static void
x11_compositor_deliver_enter_event(struct x11_compositor *c,
					xcb_generic_event_t *event)
{
	struct x11_output *output;
	wl_fixed_t x, y;

	xcb_enter_notify_event_t *enter_notify =
			(xcb_enter_notify_event_t *) event;
	if (enter_notify->state >= Button1Mask)
		return;
	if (!c->has_xkb)
		update_xkb_state_from_core(c, enter_notify->state);
	output = x11_compositor_find_output(c, enter_notify->event);
	if (!output)
		return;

	weston_output_transform_coordinate(&output->base,
					   wl_fixed_from_int(enter_notify->event_x),
					   wl_fixed_from_int(enter_notify->event_y), &x, &y);

	notify_pointer_focus(&c->core_seat, &output->base, x, y);

	c->prev_x = x;
	c->prev_y = y;
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
	xcb_enter_notify_event_t *enter_notify;
	xcb_key_press_event_t *key_press, *key_release;
	xcb_keymap_notify_event_t *keymap_notify;
	xcb_focus_in_event_t *focus_in;
	xcb_expose_event_t *expose;
	xcb_atom_t atom;
	xcb_window_t window;
	uint32_t *k;
	uint32_t i, set;
	uint8_t response_type;
	int count;

	prev = NULL;
	count = 0;
	while (x11_compositor_next_event(c, &event, mask)) {
		response_type = event->response_type & ~0x80;

		switch (prev ? prev->response_type & ~0x80 : 0x80) {
		case XCB_KEY_RELEASE:
			/* Suppress key repeat events; this is only used if we
			 * don't have XCB XKB support. */
			key_release = (xcb_key_press_event_t *) prev;
			key_press = (xcb_key_press_event_t *) event;
			if (response_type == XCB_KEY_PRESS &&
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
				notify_key(&c->core_seat,
					   weston_compositor_get_time(),
					   key_release->detail - 8,
					   WL_KEYBOARD_KEY_STATE_RELEASED,
					   STATE_UPDATE_AUTOMATIC);
				free(prev);
				prev = NULL;
				break;
			}

		case XCB_FOCUS_IN:
			assert(response_type == XCB_KEYMAP_NOTIFY);
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

			/* Unfortunately the state only comes with the enter
			 * event, rather than with the focus event.  I'm not
			 * sure of the exact semantics around it and whether
			 * we can ensure that we get both? */
			notify_keyboard_focus_in(&c->core_seat, &c->keys,
						 STATE_UPDATE_AUTOMATIC);

			free(prev);
			prev = NULL;
			break;

		default:
			/* No previous event held */
			break;
		}

		switch (response_type) {
		case XCB_KEY_PRESS:
			key_press = (xcb_key_press_event_t *) event;
			if (!c->has_xkb)
				update_xkb_state_from_core(c, key_press->state);
			notify_key(&c->core_seat,
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
			notify_key(&c->core_seat,
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
			x11_compositor_deliver_motion_event(c, event);
			break;

		case XCB_EXPOSE:
			expose = (xcb_expose_event_t *) event;
			output = x11_compositor_find_output(c, expose->window);
			if (!output)
				break;

			weston_output_damage(&output->base);
			weston_output_schedule_repaint(&output->base);
			break;

		case XCB_ENTER_NOTIFY:
			x11_compositor_deliver_enter_event(c, event);
			break;

		case XCB_LEAVE_NOTIFY:
			enter_notify = (xcb_enter_notify_event_t *) event;
			if (enter_notify->state >= Button1Mask)
				break;
			if (!c->has_xkb)
				update_xkb_state_from_core(c, enter_notify->state);
			notify_pointer_focus(&c->core_seat, NULL, 0, 0);
			break;

		case XCB_CLIENT_MESSAGE:
			client_message = (xcb_client_message_event_t *) event;
			atom = client_message->data.data32[0];
			window = client_message->window;
			if (atom == c->atom.wm_delete_window) {
				struct wl_event_loop *loop;
				struct window_delete_data *data = malloc(sizeof *data);

				/* if malloc failed we should at least try to
				 * delete the window, even if it may result in
				 * a crash.
				 */
				if (!data) {
					x11_compositor_delete_window(c, window);
					break;
				}
				data->compositor = c;
				data->window = window;
				loop = wl_display_get_event_loop(c->base.wl_display);
				wl_event_loop_add_idle(loop, delete_cb, data);
			}
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
			notify_keyboard_focus_out(&c->core_seat);
			break;

		default:
			break;
		}

#ifdef HAVE_XCB_XKB
		if (c->has_xkb) {
			if (response_type == c->xkb_event_base) {
				xcb_xkb_state_notify_event_t *state =
					(xcb_xkb_state_notify_event_t *) event;
				if (state->xkbType == XCB_XKB_STATE_NOTIFY)
					update_xkb_state(c, state);
			} else if (response_type == XCB_PROPERTY_NOTIFY) {
				xcb_property_notify_event_t *prop_notify =
					(xcb_property_notify_event_t *) event;
				if (prop_notify->window == c->screen->root &&
				    prop_notify->atom == c->atom.xkb_names &&
				    prop_notify->state == XCB_PROPERTY_NEW_VALUE)
					update_xkb_keymap(c);
			}
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
		notify_key(&c->core_seat,
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
		{ "_NET_SUPPORTING_WM_CHECK",
					F(atom.net_supporting_wm_check) },
		{ "_NET_SUPPORTED",     F(atom.net_supported) },
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
x11_compositor_get_wm_info(struct x11_compositor *c)
{
	xcb_get_property_cookie_t cookie;
	xcb_get_property_reply_t *reply;
	xcb_atom_t *atom;
	unsigned int i;

	cookie = xcb_get_property(c->conn, 0, c->screen->root,
				  c->atom.net_supported,
				  XCB_ATOM_ATOM, 0, 1024);
	reply = xcb_get_property_reply(c->conn, cookie, NULL);
	if (reply == NULL)
		return;

	atom = (xcb_atom_t *) xcb_get_property_value(reply);

	for (i = 0; i < reply->value_len; i++) {
		if (atom[i] == c->atom.net_wm_state_fullscreen)
			c->has_net_wm_state_fullscreen = 1;
	}

	free(reply);
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

	XCloseDisplay(compositor->dpy);
	free(ec);
}

static int
init_gl_renderer(struct x11_compositor *c)
{
	int ret;

	gl_renderer = weston_load_module("gl-renderer.so",
					 "gl_renderer_interface");
	if (!gl_renderer)
		return -1;

	ret = gl_renderer->create(&c->base, EGL_PLATFORM_X11_KHR, (void *) c->dpy,
				  gl_renderer->opaque_attribs, NULL, 0);

	return ret;
}
static struct weston_compositor *
x11_compositor_create(struct wl_display *display,
		      int fullscreen,
		      int no_input,
		      int use_pixman,
		      int *argc, char *argv[],
		      struct weston_config *config)
{
	struct x11_compositor *c;
	struct x11_output *output;
	struct weston_config_section *section;
	xcb_screen_iterator_t s;
	int i, x = 0, output_count = 0;
	int width, height, scale, count;
	const char *section_name;
	char *name, *t, *mode;
	uint32_t transform;

	weston_log("initializing x11 backend\n");

	c = zalloc(sizeof *c);
	if (c == NULL)
		return NULL;

	if (weston_compositor_init(&c->base, display, argc, argv, config) < 0)
		goto err_free;

	if (weston_compositor_set_presentation_clock_software(&c->base) < 0)
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
	x11_compositor_get_wm_info(c);

	if (!c->has_net_wm_state_fullscreen && fullscreen) {
		weston_log("Can not fullscreen without window manager support"
			   "(need _NET_WM_STATE_FULLSCREEN)\n");
		fullscreen = 0;
	}

	c->base.wl_display = display;
	c->use_pixman = use_pixman;
	if (c->use_pixman) {
		if (pixman_renderer_init(&c->base) < 0)
			goto err_xdisplay;
	}
	else if (init_gl_renderer(c) < 0) {
		goto err_xdisplay;
	}
	weston_log("Using %s renderer\n", use_pixman ? "pixman" : "gl");

	c->base.destroy = x11_destroy;
	c->base.restore = x11_restore;

	if (x11_input_create(c, no_input) < 0)
		goto err_renderer;

	width = option_width ? option_width : 1024;
	height = option_height ? option_height : 640;
	scale = option_scale ? option_scale : 1;
	count = option_count ? option_count : 1;

	section = NULL;
	while (weston_config_next_section(c->base.config,
					  &section, &section_name)) {
		if (strcmp(section_name, "output") != 0)
			continue;
		weston_config_section_get_string(section, "name", &name, NULL);
		if (name == NULL || name[0] != 'X') {
			free(name);
			continue;
		}

		weston_config_section_get_string(section,
						 "mode", &mode, "1024x600");
		if (sscanf(mode, "%dx%d", &width, &height) != 2) {
			weston_log("Invalid mode \"%s\" for output %s\n",
				   mode, name);
			width = 1024;
			height = 600;
		}
		free(mode);

		if (option_width)
			width = option_width;
		if (option_height)
			height = option_height;

		weston_config_section_get_int(section, "scale", &scale, 1);
		if (option_scale)
			scale = option_scale;

		weston_config_section_get_string(section,
						 "transform", &t, "normal");
		if (weston_parse_transform(t, &transform) < 0)
			weston_log("Invalid transform \"%s\" for output %s\n",
				   t, name);
		free(t);

		output = x11_compositor_create_output(c, x, 0,
						      width, height,
						      fullscreen, no_input,
						      name, transform, scale);
		free(name);
		if (output == NULL)
			goto err_x11_input;

		x = pixman_region32_extents(&output->base.region)->x2;

		output_count++;
		if (option_count && output_count >= option_count)
			break;
	}

	for (i = output_count; i < count; i++) {
		output = x11_compositor_create_output(c, x, 0, width, height,
						      fullscreen, no_input, NULL,
						      WL_OUTPUT_TRANSFORM_NORMAL, scale);
		if (output == NULL)
			goto err_x11_input;
		x = pixman_region32_extents(&output->base.region)->x2;
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
err_renderer:
	c->base.renderer->destroy(&c->base);
err_xdisplay:
	XCloseDisplay(c->dpy);
err_free:
	free(c);
	return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int *argc, char *argv[],
	     struct weston_config *config)
{
	int fullscreen = 0;
	int no_input = 0;
	int use_pixman = 0;

	const struct weston_option x11_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &option_width },
		{ WESTON_OPTION_INTEGER, "height", 0, &option_height },
		{ WESTON_OPTION_INTEGER, "scale", 0, &option_scale },
		{ WESTON_OPTION_BOOLEAN, "fullscreen", 'f', &fullscreen },
		{ WESTON_OPTION_INTEGER, "output-count", 0, &option_count },
		{ WESTON_OPTION_BOOLEAN, "no-input", 0, &no_input },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &use_pixman },
	};

	parse_options(x11_options, ARRAY_LENGTH(x11_options), argc, argv);

	return x11_compositor_create(display,
				     fullscreen,
				     no_input,
				     use_pixman,
				     argc, argv, config);
}

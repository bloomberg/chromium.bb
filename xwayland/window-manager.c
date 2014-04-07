/*
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xcursor/Xcursor.h>
#include <linux/input.h>

#include "xwayland.h"

#include "cairo-util.h"
#include "compositor.h"
#include "hash.h"

struct wm_size_hints {
    	uint32_t flags;
	int32_t x, y;
	int32_t width, height;	/* should set so old wm's don't mess up */
	int32_t min_width, min_height;
	int32_t max_width, max_height;
    	int32_t width_inc, height_inc;
	struct {
		int32_t x;
		int32_t y;
	} min_aspect, max_aspect;
	int32_t base_width, base_height;
	int32_t win_gravity;
};

#define USPosition	(1L << 0)
#define USSize		(1L << 1)
#define PPosition	(1L << 2)
#define PSize		(1L << 3)
#define PMinSize	(1L << 4)
#define PMaxSize	(1L << 5)
#define PResizeInc	(1L << 6)
#define PAspect		(1L << 7)
#define PBaseSize	(1L << 8)
#define PWinGravity	(1L << 9)

struct motif_wm_hints {
	uint32_t flags;
	uint32_t functions;
	uint32_t decorations;
	int32_t input_mode;
	uint32_t status;
};

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW      (1L<<0)

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */

struct weston_wm_window {
	struct weston_wm *wm;
	xcb_window_t id;
	xcb_window_t frame_id;
	struct frame *frame;
	cairo_surface_t *cairo_surface;
	uint32_t surface_id;
	struct weston_surface *surface;
	struct shell_surface *shsurf;
	struct weston_view *view;
	struct wl_listener surface_destroy_listener;
	struct wl_event_source *repaint_source;
	struct wl_event_source *configure_source;
	int properties_dirty;
	int pid;
	char *machine;
	char *class;
	char *name;
	struct weston_wm_window *transient_for;
	uint32_t protocols;
	xcb_atom_t type;
	int width, height;
	int x, y;
	int saved_width, saved_height;
	int decorate;
	int override_redirect;
	int fullscreen;
	int has_alpha;
	int delete_window;
	struct wm_size_hints size_hints;
	struct motif_wm_hints motif_hints;
	struct wl_list link;
};

static struct weston_wm_window *
get_wm_window(struct weston_surface *surface);

static void
weston_wm_window_schedule_repaint(struct weston_wm_window *window);

static void
xserver_map_shell_surface(struct weston_wm_window *window,
			  struct weston_surface *surface);

static int __attribute__ ((format (printf, 1, 2)))
wm_log(const char *fmt, ...)
{
#ifdef WM_DEBUG
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = weston_vlog(fmt, argp);
	va_end(argp);

	return l;
#else
	return 0;
#endif
}

static int __attribute__ ((format (printf, 1, 2)))
wm_log_continue(const char *fmt, ...)
{
#ifdef WM_DEBUG
	int l;
	va_list argp;

	va_start(argp, fmt);
	l = weston_vlog_continue(fmt, argp);
	va_end(argp);

	return l;
#else
	return 0;
#endif
}


const char *
get_atom_name(xcb_connection_t *c, xcb_atom_t atom)
{
	xcb_get_atom_name_cookie_t cookie;
	xcb_get_atom_name_reply_t *reply;
	xcb_generic_error_t *e;
	static char buffer[64];

	if (atom == XCB_ATOM_NONE)
		return "None";

	cookie = xcb_get_atom_name (c, atom);
	reply = xcb_get_atom_name_reply (c, cookie, &e);

	if(reply) {
		snprintf(buffer, sizeof buffer, "%.*s",
			 xcb_get_atom_name_name_length (reply),
			 xcb_get_atom_name_name (reply));
	} else {
		snprintf(buffer, sizeof buffer, "(atom %u)", atom);
	}

	free(reply);

	return buffer;
}

static xcb_cursor_t
xcb_cursor_image_load_cursor(struct weston_wm *wm, const XcursorImage *img)
{
	xcb_connection_t *c = wm->conn;
	xcb_screen_iterator_t s = xcb_setup_roots_iterator(xcb_get_setup(c));
	xcb_screen_t *screen = s.data;
	xcb_gcontext_t gc;
	xcb_pixmap_t pix;
	xcb_render_picture_t pic;
	xcb_cursor_t cursor;
	int stride = img->width * 4;

	pix = xcb_generate_id(c);
	xcb_create_pixmap(c, 32, pix, screen->root, img->width, img->height);

	pic = xcb_generate_id(c);
	xcb_render_create_picture(c, pic, pix, wm->format_rgba.id, 0, 0);

	gc = xcb_generate_id(c);
	xcb_create_gc(c, gc, pix, 0, 0);

	xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc,
		      img->width, img->height, 0, 0, 0, 32,
		      stride * img->height, (uint8_t *) img->pixels);
	xcb_free_gc(c, gc);

	cursor = xcb_generate_id(c);
	xcb_render_create_cursor(c, cursor, pic, img->xhot, img->yhot);

	xcb_render_free_picture(c, pic);
	xcb_free_pixmap(c, pix);

	return cursor;
}

static xcb_cursor_t
xcb_cursor_images_load_cursor(struct weston_wm *wm, const XcursorImages *images)
{
	/* TODO: treat animated cursors as well */
	if (images->nimage != 1)
		return -1;

	return xcb_cursor_image_load_cursor(wm, images->images[0]);
}

static xcb_cursor_t
xcb_cursor_library_load_cursor(struct weston_wm *wm, const char *file)
{
	xcb_cursor_t cursor;
	XcursorImages *images;
	char *v = NULL;
	int size = 0;

	if (!file)
		return 0;

	v = getenv ("XCURSOR_SIZE");
	if (v)
		size = atoi(v);

	if (!size)
		size = 32;

	images = XcursorLibraryLoadImages (file, NULL, size);
	if (!images)
		return -1;

	cursor = xcb_cursor_images_load_cursor (wm, images);
	XcursorImagesDestroy (images);

	return cursor;
}

void
dump_property(struct weston_wm *wm,
	      xcb_atom_t property, xcb_get_property_reply_t *reply)
{
	int32_t *incr_value;
	const char *text_value, *name;
	xcb_atom_t *atom_value;
	int width, len;
	uint32_t i;

	width = wm_log_continue("%s: ", get_atom_name(wm->conn, property));
	if (reply == NULL) {
		wm_log_continue("(no reply)\n");
		return;
	}

	width += wm_log_continue("%s/%d, length %d (value_len %d): ",
				 get_atom_name(wm->conn, reply->type),
				 reply->format,
				 xcb_get_property_value_length(reply),
				 reply->value_len);

	if (reply->type == wm->atom.incr) {
		incr_value = xcb_get_property_value(reply);
		wm_log_continue("%d\n", *incr_value);
	} else if (reply->type == wm->atom.utf8_string ||
	      reply->type == wm->atom.string) {
		text_value = xcb_get_property_value(reply);
		if (reply->value_len > 40)
			len = 40;
		else
			len = reply->value_len;
		wm_log_continue("\"%.*s\"\n", len, text_value);
	} else if (reply->type == XCB_ATOM_ATOM) {
		atom_value = xcb_get_property_value(reply);
		for (i = 0; i < reply->value_len; i++) {
			name = get_atom_name(wm->conn, atom_value[i]);
			if (width + strlen(name) + 2 > 78) {
				wm_log_continue("\n    ");
				width = 4;
			} else if (i > 0) {
				width +=  wm_log_continue(", ");
			}

			width +=  wm_log_continue("%s", name);
		}
		wm_log_continue("\n");
	} else {
		wm_log_continue("huh?\n");
	}
}

static void
read_and_dump_property(struct weston_wm *wm,
		       xcb_window_t window, xcb_atom_t property)
{
	xcb_get_property_reply_t *reply;
	xcb_get_property_cookie_t cookie;

	cookie = xcb_get_property(wm->conn, 0, window,
				  property, XCB_ATOM_ANY, 0, 2048);
	reply = xcb_get_property_reply(wm->conn, cookie, NULL);

	dump_property(wm, property, reply);

	free(reply);
}

/* We reuse some predefined, but otherwise useles atoms */
#define TYPE_WM_PROTOCOLS	XCB_ATOM_CUT_BUFFER0
#define TYPE_MOTIF_WM_HINTS	XCB_ATOM_CUT_BUFFER1
#define TYPE_NET_WM_STATE	XCB_ATOM_CUT_BUFFER2
#define TYPE_WM_NORMAL_HINTS	XCB_ATOM_CUT_BUFFER3

static void
weston_wm_window_read_properties(struct weston_wm_window *window)
{
	struct weston_wm *wm = window->wm;
	struct weston_shell_interface *shell_interface =
		&wm->server->compositor->shell_interface;

#define F(field) offsetof(struct weston_wm_window, field)
	const struct {
		xcb_atom_t atom;
		xcb_atom_t type;
		int offset;
	} props[] = {
		{ XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, F(class) },
		{ XCB_ATOM_WM_NAME, XCB_ATOM_STRING, F(name) },
		{ XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, F(transient_for) },
		{ wm->atom.wm_protocols, TYPE_WM_PROTOCOLS, F(protocols) },
		{ wm->atom.wm_normal_hints, TYPE_WM_NORMAL_HINTS, F(protocols) },
		{ wm->atom.net_wm_state, TYPE_NET_WM_STATE },
		{ wm->atom.net_wm_window_type, XCB_ATOM_ATOM, F(type) },
		{ wm->atom.net_wm_name, XCB_ATOM_STRING, F(name) },
		{ wm->atom.net_wm_pid, XCB_ATOM_CARDINAL, F(pid) },
		{ wm->atom.motif_wm_hints, TYPE_MOTIF_WM_HINTS, 0 },
		{ wm->atom.wm_client_machine, XCB_ATOM_WM_CLIENT_MACHINE, F(machine) },
	};
#undef F

	xcb_get_property_cookie_t cookie[ARRAY_LENGTH(props)];
	xcb_get_property_reply_t *reply;
	void *p;
	uint32_t *xid;
	xcb_atom_t *atom;
	uint32_t i;

	if (!window->properties_dirty)
		return;
	window->properties_dirty = 0;

	for (i = 0; i < ARRAY_LENGTH(props); i++)
		cookie[i] = xcb_get_property(wm->conn,
					     0, /* delete */
					     window->id,
					     props[i].atom,
					     XCB_ATOM_ANY, 0, 2048);

	window->decorate = !window->override_redirect;
	window->size_hints.flags = 0;
	window->motif_hints.flags = 0;
	window->delete_window = 0;

	for (i = 0; i < ARRAY_LENGTH(props); i++)  {
		reply = xcb_get_property_reply(wm->conn, cookie[i], NULL);
		if (!reply)
			/* Bad window, typically */
			continue;
		if (reply->type == XCB_ATOM_NONE) {
			/* No such property */
			free(reply);
			continue;
		}

		p = ((char *) window + props[i].offset);

		switch (props[i].type) {
		case XCB_ATOM_WM_CLIENT_MACHINE:
		case XCB_ATOM_STRING:
			/* FIXME: We're using this for both string and
			   utf8_string */
			if (*(char **) p)
				free(*(char **) p);

			*(char **) p =
				strndup(xcb_get_property_value(reply),
					xcb_get_property_value_length(reply));
			break;
		case XCB_ATOM_WINDOW:
			xid = xcb_get_property_value(reply);
			*(struct weston_wm_window **) p =
				hash_table_lookup(wm->window_hash, *xid);
			break;
		case XCB_ATOM_CARDINAL:
		case XCB_ATOM_ATOM:
			atom = xcb_get_property_value(reply);
			*(xcb_atom_t *) p = *atom;
			break;
		case TYPE_WM_PROTOCOLS:
			atom = xcb_get_property_value(reply);
			for (i = 0; i < reply->value_len; i++)
				if (atom[i] == wm->atom.wm_delete_window)
					window->delete_window = 1;
			break;

			break;
		case TYPE_WM_NORMAL_HINTS:
			memcpy(&window->size_hints,
			       xcb_get_property_value(reply),
			       sizeof window->size_hints);
			break;
		case TYPE_NET_WM_STATE:
			window->fullscreen = 0;
			atom = xcb_get_property_value(reply);
			for (i = 0; i < reply->value_len; i++)
				if (atom[i] == wm->atom.net_wm_state_fullscreen)
					window->fullscreen = 1;
			break;
		case TYPE_MOTIF_WM_HINTS:
			memcpy(&window->motif_hints,
			       xcb_get_property_value(reply),
			       sizeof window->motif_hints);
			if (window->motif_hints.flags & MWM_HINTS_DECORATIONS)
				window->decorate =
					window->motif_hints.decorations > 0;
			break;
		default:
			break;
		}
		free(reply);
	}

	if (window->shsurf && window->name)
		shell_interface->set_title(window->shsurf, window->name);
	if (window->frame && window->name)
		frame_set_title(window->frame, window->name);
}

static void
weston_wm_window_get_frame_size(struct weston_wm_window *window,
				int *width, int *height)
{
	struct theme *t = window->wm->theme;

	if (window->fullscreen) {
		*width = window->width;
		*height = window->height;
	} else if (window->decorate && window->frame) {
		*width = frame_width(window->frame);
		*height = frame_height(window->frame);
	} else {
		*width = window->width + t->margin * 2;
		*height = window->height + t->margin * 2;
	}
}

static void
weston_wm_window_get_child_position(struct weston_wm_window *window,
				    int *x, int *y)
{
	struct theme *t = window->wm->theme;

	if (window->fullscreen) {
		*x = 0;
		*y = 0;
	} else if (window->decorate && window->frame) {
		frame_interior(window->frame, x, y, NULL, NULL);
	} else {
		*x = t->margin;
		*y = t->margin;
	}
}

static void
weston_wm_window_send_configure_notify(struct weston_wm_window *window)
{
	xcb_configure_notify_event_t configure_notify;
	struct weston_wm *wm = window->wm;
	int x, y;

	weston_wm_window_get_child_position(window, &x, &y);
	configure_notify.response_type = XCB_CONFIGURE_NOTIFY;
	configure_notify.pad0 = 0;
	configure_notify.event = window->id;
	configure_notify.window = window->id;
	configure_notify.above_sibling = XCB_WINDOW_NONE;
	configure_notify.x = x;
	configure_notify.y = y;
	configure_notify.width = window->width;
	configure_notify.height = window->height;
	configure_notify.border_width = 0;
	configure_notify.override_redirect = 0;
	configure_notify.pad1 = 0;

	xcb_send_event(wm->conn, 0, window->id, 
		       XCB_EVENT_MASK_STRUCTURE_NOTIFY,
		       (char *) &configure_notify);
}

static void
weston_wm_handle_configure_request(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_configure_request_event_t *configure_request = 
		(xcb_configure_request_event_t *) event;
	struct weston_wm_window *window;
	uint32_t mask, values[16];
	int x, y, width, height, i = 0;

	wm_log("XCB_CONFIGURE_REQUEST (window %d) %d,%d @ %dx%d\n",
	       configure_request->window,
	       configure_request->x, configure_request->y,
	       configure_request->width, configure_request->height);

	window = hash_table_lookup(wm->window_hash, configure_request->window);

	if (window->fullscreen) {
		weston_wm_window_send_configure_notify(window);
		return;
	}

	if (configure_request->value_mask & XCB_CONFIG_WINDOW_WIDTH)
		window->width = configure_request->width;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
		window->height = configure_request->height;

	if (window->frame)
		frame_resize_inside(window->frame, window->width, window->height);

	weston_wm_window_get_child_position(window, &x, &y);
	values[i++] = x;
	values[i++] = y;
	values[i++] = window->width;
	values[i++] = window->height;
	values[i++] = 0;
	mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
		XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
		XCB_CONFIG_WINDOW_BORDER_WIDTH;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
		values[i++] = configure_request->sibling;
		mask |= XCB_CONFIG_WINDOW_SIBLING;
	}
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
		values[i++] = configure_request->stack_mode;
		mask |= XCB_CONFIG_WINDOW_STACK_MODE;
	}

	xcb_configure_window(wm->conn, window->id, mask, values);

	weston_wm_window_get_frame_size(window, &width, &height);
	values[0] = width;
	values[1] = height;
	mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
	xcb_configure_window(wm->conn, window->frame_id, mask, values);

	weston_wm_window_schedule_repaint(window);
}

static int
our_resource(struct weston_wm *wm, uint32_t id)
{
	const xcb_setup_t *setup;

	setup = xcb_get_setup(wm->conn);

	return (id & ~setup->resource_id_mask) == setup->resource_id_base;
}

static void
weston_wm_handle_configure_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_configure_notify_event_t *configure_notify = 
		(xcb_configure_notify_event_t *) event;
	struct weston_wm_window *window;

	wm_log("XCB_CONFIGURE_NOTIFY (window %d) %d,%d @ %dx%d\n",
	       configure_notify->window,
	       configure_notify->x, configure_notify->y,
	       configure_notify->width, configure_notify->height);

	window = hash_table_lookup(wm->window_hash, configure_notify->window);
	window->x = configure_notify->x;
	window->y = configure_notify->y;
	if (window->override_redirect) {
		window->width = configure_notify->width;
		window->height = configure_notify->height;
		if (window->frame)
			frame_resize_inside(window->frame,
					    window->width, window->height);
	}
}

static void
weston_wm_kill_client(struct wl_listener *listener, void *data)
{
	struct weston_surface *surface = data;
	struct weston_wm_window *window = get_wm_window(surface);
	char name[1024];

	if (!window)
		return;

	gethostname(name, 1024);

	/* this is only one heuristic to guess the PID of a client is valid,
	 * assuming it's compliant with icccm and ewmh. Non-compliants and
	 * remote applications of course fail. */
	if (!strcmp(window->machine, name) && window->pid != 0)
		kill(window->pid, SIGKILL);
}

static void
weston_wm_create_surface(struct wl_listener *listener, void *data)
{
	struct weston_surface *surface = data;
	struct weston_wm *wm =
		container_of(listener,
			     struct weston_wm, create_surface_listener);
	struct weston_wm_window *window;

	if (wl_resource_get_client(surface->resource) != wm->server->client)
		return;

	wl_list_for_each(window, &wm->unpaired_window_list, link)
		if (window->surface_id ==
		    wl_resource_get_id(surface->resource)) {
			xserver_map_shell_surface(window, surface);
			break;
		}	
}

static void
weston_wm_window_activate(struct wl_listener *listener, void *data)
{
	struct weston_surface *surface = data;
	struct weston_wm_window *window = NULL;
	struct weston_wm *wm =
		container_of(listener, struct weston_wm, activate_listener);
	xcb_client_message_event_t client_message;

	if (surface) {
		window = get_wm_window(surface);
	}

	if (window) {
		client_message.response_type = XCB_CLIENT_MESSAGE;
		client_message.format = 32;
		client_message.window = window->id;
		client_message.type = wm->atom.wm_protocols;
		client_message.data.data32[0] = wm->atom.wm_take_focus;
		client_message.data.data32[1] = XCB_TIME_CURRENT_TIME;

		xcb_send_event(wm->conn, 0, window->id, 
			       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
			       (char *) &client_message);

		xcb_set_input_focus (wm->conn, XCB_INPUT_FOCUS_POINTER_ROOT,
				     window->id, XCB_TIME_CURRENT_TIME);
	} else {
		xcb_set_input_focus (wm->conn,
				     XCB_INPUT_FOCUS_POINTER_ROOT,
				     XCB_NONE,
				     XCB_TIME_CURRENT_TIME);
	}

	if (wm->focus_window) {
		if (wm->focus_window->frame)
			frame_unset_flag(wm->focus_window->frame, FRAME_FLAG_ACTIVE);
		weston_wm_window_schedule_repaint(wm->focus_window);
	}
	wm->focus_window = window;
	if (wm->focus_window) {
		if (wm->focus_window->frame)
			frame_set_flag(wm->focus_window->frame, FRAME_FLAG_ACTIVE);
		weston_wm_window_schedule_repaint(wm->focus_window);
	}
}

static void
weston_wm_window_transform(struct wl_listener *listener, void *data)
{
	struct weston_surface *surface = data;
	struct weston_wm_window *window = get_wm_window(surface);
	struct weston_wm *wm =
		container_of(listener, struct weston_wm, transform_listener);
	uint32_t mask, values[2];

	if (!window || !wm)
		return;

	if (!window->view || !weston_view_is_mapped(window->view))
		return;

	if (window->x != window->view->geometry.x ||
	    window->y != window->view->geometry.y) {
		values[0] = window->view->geometry.x;
		values[1] = window->view->geometry.y;
		mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;

		xcb_configure_window(wm->conn, window->frame_id, mask, values);
		xcb_flush(wm->conn);
	}
}

#define ICCCM_WITHDRAWN_STATE	0
#define ICCCM_NORMAL_STATE	1
#define ICCCM_ICONIC_STATE	3

static void
weston_wm_window_set_wm_state(struct weston_wm_window *window, int32_t state)
{
	struct weston_wm *wm = window->wm;
	uint32_t property[2];

	property[0] = state;
	property[1] = XCB_WINDOW_NONE;

	xcb_change_property(wm->conn,
			    XCB_PROP_MODE_REPLACE,
			    window->id,
			    wm->atom.wm_state,
			    wm->atom.wm_state,
			    32, /* format */
			    2, property);
}

static void
weston_wm_window_set_net_wm_state(struct weston_wm_window *window)
{
	struct weston_wm *wm = window->wm;
	uint32_t property[1];
	int i;

	i = 0;
	if (window->fullscreen)
		property[i++] = wm->atom.net_wm_state_fullscreen;

	xcb_change_property(wm->conn,
			    XCB_PROP_MODE_REPLACE,
			    window->id,
			    wm->atom.net_wm_state,
			    XCB_ATOM_ATOM,
			    32, /* format */
			    i, property);
}

static void
weston_wm_window_create_frame(struct weston_wm_window *window)
{
	struct weston_wm *wm = window->wm;
	uint32_t values[3];
	int x, y, width, height;

	window->frame = frame_create(window->wm->theme,
				     window->width, window->height,
				     FRAME_BUTTON_CLOSE, window->name);
	frame_resize_inside(window->frame, window->width, window->height);

	weston_wm_window_get_frame_size(window, &width, &height);
	weston_wm_window_get_child_position(window, &x, &y);

	values[0] = wm->screen->black_pixel;
	values[1] =
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_ENTER_WINDOW |
		XCB_EVENT_MASK_LEAVE_WINDOW |
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
	values[2] = wm->colormap;

	window->frame_id = xcb_generate_id(wm->conn);
	xcb_create_window(wm->conn,
			  32,
			  window->frame_id,
			  wm->screen->root,
			  0, 0,
			  width, height,
			  0,
			  XCB_WINDOW_CLASS_INPUT_OUTPUT,
			  wm->visual_id,
			  XCB_CW_BORDER_PIXEL |
			  XCB_CW_EVENT_MASK |
			  XCB_CW_COLORMAP, values);

	xcb_reparent_window(wm->conn, window->id, window->frame_id, x, y);

	values[0] = 0;
	xcb_configure_window(wm->conn, window->id,
			     XCB_CONFIG_WINDOW_BORDER_WIDTH, values);

	window->cairo_surface =
		cairo_xcb_surface_create_with_xrender_format(wm->conn,
							     wm->screen,
							     window->frame_id,
							     &wm->format_rgba,
							     width, height);

	hash_table_insert(wm->window_hash, window->frame_id, window);
}

static void
weston_wm_handle_map_request(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_map_request_event_t *map_request =
		(xcb_map_request_event_t *) event;
	struct weston_wm_window *window;

	if (our_resource(wm, map_request->window)) {
		wm_log("XCB_MAP_REQUEST (window %d, ours)\n",
		       map_request->window);
		return;
	}

	window = hash_table_lookup(wm->window_hash, map_request->window);

	weston_wm_window_read_properties(window);

	if (window->frame_id == XCB_WINDOW_NONE)
		weston_wm_window_create_frame(window);

	wm_log("XCB_MAP_REQUEST (window %d, %p, frame %d)\n",
	       window->id, window, window->frame_id);

	weston_wm_window_set_wm_state(window, ICCCM_NORMAL_STATE);
	weston_wm_window_set_net_wm_state(window);

	xcb_map_window(wm->conn, map_request->window);
	xcb_map_window(wm->conn, window->frame_id);
}

static void
weston_wm_handle_map_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_map_notify_event_t *map_notify = (xcb_map_notify_event_t *) event;

	if (our_resource(wm, map_notify->window)) {
		wm_log("XCB_MAP_NOTIFY (window %d, ours)\n",
		       map_notify->window);
			return;
	}

	wm_log("XCB_MAP_NOTIFY (window %d)\n", map_notify->window);
}

static void
weston_wm_handle_unmap_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_unmap_notify_event_t *unmap_notify =
		(xcb_unmap_notify_event_t *) event;
	struct weston_wm_window *window;

	wm_log("XCB_UNMAP_NOTIFY (window %d, event %d%s)\n",
	       unmap_notify->window,
	       unmap_notify->event,
	       our_resource(wm, unmap_notify->window) ? ", ours" : "");

	if (our_resource(wm, unmap_notify->window))
		return;

	if (unmap_notify->response_type & SEND_EVENT_MASK)
		/* We just ignore the ICCCM 4.1.4 synthetic unmap notify
		 * as it may come in after we've destroyed the window. */
		return;

	window = hash_table_lookup(wm->window_hash, unmap_notify->window);
	if (wm->focus_window == window)
		wm->focus_window = NULL;
	if (window->surface)
		wl_list_remove(&window->surface_destroy_listener.link);
	window->surface = NULL;
	window->shsurf = NULL;
	window->view = NULL;
	xcb_unmap_window(wm->conn, window->frame_id);
}

static void
weston_wm_window_draw_decoration(void *data)
{
	struct weston_wm_window *window = data;
	struct weston_wm *wm = window->wm;
	struct theme *t = wm->theme;
	cairo_t *cr;
	int x, y, width, height;
	int32_t input_x, input_y, input_w, input_h;

	uint32_t flags = 0;

	weston_wm_window_read_properties(window);

	window->repaint_source = NULL;

	weston_wm_window_get_frame_size(window, &width, &height);
	weston_wm_window_get_child_position(window, &x, &y);

	cairo_xcb_surface_set_size(window->cairo_surface, width, height);
	cr = cairo_create(window->cairo_surface);

	if (window->fullscreen) {
		/* nothing */
	} else if (window->decorate) {
		if (wm->focus_window == window)
			flags |= THEME_FRAME_ACTIVE;

		frame_repaint(window->frame, cr);
	} else {
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_rgba(cr, 0, 0, 0, 0);
		cairo_paint(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
		tile_mask(cr, t->shadow, 2, 2, width + 8, height + 8, 64, 64);
	}

	cairo_destroy(cr);

	if (window->surface) {
		pixman_region32_fini(&window->surface->pending.opaque);
		if(window->has_alpha) {
			pixman_region32_init(&window->surface->pending.opaque);
		} else {
			/* We leave an extra pixel around the X window area to
			 * make sure we don't sample from the undefined alpha
			 * channel when filtering. */
			pixman_region32_init_rect(&window->surface->pending.opaque, 
						  x - 1, y - 1,
						  window->width + 2,
						  window->height + 2);
		}
		if (window->view)
			weston_view_geometry_dirty(window->view);

		pixman_region32_fini(&window->surface->pending.input);

		if (window->fullscreen) {
			input_x = 0;
			input_y = 0;
			input_w = window->width;
			input_h = window->height;
		} else if (window->decorate) {
			frame_input_rect(window->frame, &input_x, &input_y,
					 &input_w, &input_h);
		}

		pixman_region32_init_rect(&window->surface->pending.input,
					  input_x, input_y, input_w, input_h);
	}
}

static void
weston_wm_window_schedule_repaint(struct weston_wm_window *window)
{
	struct weston_wm *wm = window->wm;
	int width, height;

	if (window->frame_id == XCB_WINDOW_NONE) {
		if (window->surface != NULL) {
			weston_wm_window_get_frame_size(window, &width, &height);
			pixman_region32_fini(&window->surface->pending.opaque);
			if(window->has_alpha) {
				pixman_region32_init(&window->surface->pending.opaque);
			} else {
				pixman_region32_init_rect(&window->surface->pending.opaque, 0, 0,
							  width, height);
			}
			if (window->view)
				weston_view_geometry_dirty(window->view);
		}
		return;
	}

	if (window->repaint_source)
		return;

	window->repaint_source =
		wl_event_loop_add_idle(wm->server->loop,
				       weston_wm_window_draw_decoration,
				       window);
}

static void
weston_wm_handle_property_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_property_notify_event_t *property_notify =
		(xcb_property_notify_event_t *) event;
	struct weston_wm_window *window;

	window = hash_table_lookup(wm->window_hash, property_notify->window);
	if (!window)
		return;

	window->properties_dirty = 1;

	wm_log("XCB_PROPERTY_NOTIFY: window %d, ", property_notify->window);
	if (property_notify->state == XCB_PROPERTY_DELETE)
		wm_log("deleted\n");
	else
		read_and_dump_property(wm, property_notify->window,
				       property_notify->atom);

	if (property_notify->atom == wm->atom.net_wm_name ||
	    property_notify->atom == XCB_ATOM_WM_NAME)
		weston_wm_window_schedule_repaint(window);
}

static void
weston_wm_window_create(struct weston_wm *wm,
			xcb_window_t id, int width, int height, int x, int y, int override)
{
	struct weston_wm_window *window;
	uint32_t values[1];
	xcb_get_geometry_cookie_t geometry_cookie;
	xcb_get_geometry_reply_t *geometry_reply;

	window = zalloc(sizeof *window);
	if (window == NULL) {
		wm_log("failed to allocate window\n");
		return;
	}

	geometry_cookie = xcb_get_geometry(wm->conn, id);

	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(wm->conn, id, XCB_CW_EVENT_MASK, values);

	window->wm = wm;
	window->id = id;
	window->properties_dirty = 1;
	window->override_redirect = override;
	window->width = width;
	window->height = height;
	window->x = x;
	window->y = y;

	geometry_reply = xcb_get_geometry_reply(wm->conn, geometry_cookie, NULL);
	/* technically we should use XRender and check the visual format's
	alpha_mask, but checking depth is simpler and works in all known cases */
	if(geometry_reply != NULL)
		window->has_alpha = geometry_reply->depth == 32;
	free(geometry_reply);

	hash_table_insert(wm->window_hash, id, window);
}

static void
weston_wm_window_destroy(struct weston_wm_window *window)
{
	struct weston_wm *wm = window->wm;

	if (window->repaint_source)
		wl_event_source_remove(window->repaint_source);
	if (window->cairo_surface)
		cairo_surface_destroy(window->cairo_surface);

	if (window->frame_id) {
		xcb_reparent_window(wm->conn, window->id, wm->wm_window, 0, 0);
		xcb_destroy_window(wm->conn, window->frame_id);
		weston_wm_window_set_wm_state(window, ICCCM_WITHDRAWN_STATE);
		hash_table_remove(wm->window_hash, window->frame_id);
		window->frame_id = XCB_WINDOW_NONE;
	}

	hash_table_remove(window->wm->window_hash, window->id);
	free(window);
}

static void
weston_wm_handle_create_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_create_notify_event_t *create_notify =
		(xcb_create_notify_event_t *) event;

	wm_log("XCB_CREATE_NOTIFY (window %d, width %d, height %d%s%s)\n",
	       create_notify->window,
	       create_notify->width, create_notify->height,
	       create_notify->override_redirect ? ", override" : "",
	       our_resource(wm, create_notify->window) ? ", ours" : "");

	if (our_resource(wm, create_notify->window))
		return;

	weston_wm_window_create(wm, create_notify->window,
				create_notify->width, create_notify->height,
				create_notify->x, create_notify->y,
				create_notify->override_redirect);
}

static void
weston_wm_handle_destroy_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_destroy_notify_event_t *destroy_notify =
		(xcb_destroy_notify_event_t *) event;
	struct weston_wm_window *window;

	wm_log("XCB_DESTROY_NOTIFY, win %d, event %d%s\n",
	       destroy_notify->window,
	       destroy_notify->event,
	       our_resource(wm, destroy_notify->window) ? ", ours" : "");

	if (our_resource(wm, destroy_notify->window))
		return;

	window = hash_table_lookup(wm->window_hash, destroy_notify->window);
	weston_wm_window_destroy(window);
}

static void
weston_wm_handle_reparent_notify(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_reparent_notify_event_t *reparent_notify =
		(xcb_reparent_notify_event_t *) event;
	struct weston_wm_window *window;

	wm_log("XCB_REPARENT_NOTIFY (window %d, parent %d, event %d)\n",
	       reparent_notify->window,
	       reparent_notify->parent,
	       reparent_notify->event);

	if (reparent_notify->parent == wm->screen->root) {
		weston_wm_window_create(wm, reparent_notify->window, 10, 10,
					reparent_notify->x, reparent_notify->y,
					reparent_notify->override_redirect);
	} else if (!our_resource(wm, reparent_notify->parent)) {
		window = hash_table_lookup(wm->window_hash,
					   reparent_notify->window);
		weston_wm_window_destroy(window);
	}
}

struct weston_seat *
weston_wm_pick_seat(struct weston_wm *wm)
{
	return container_of(wm->server->compositor->seat_list.next,
			    struct weston_seat, link);
}

static void
weston_wm_window_handle_moveresize(struct weston_wm_window *window,
				   xcb_client_message_event_t *client_message)
{
	static const int map[] = {
		THEME_LOCATION_RESIZING_TOP_LEFT,
		THEME_LOCATION_RESIZING_TOP,
		THEME_LOCATION_RESIZING_TOP_RIGHT,
		THEME_LOCATION_RESIZING_RIGHT,
		THEME_LOCATION_RESIZING_BOTTOM_RIGHT,
		THEME_LOCATION_RESIZING_BOTTOM,
		THEME_LOCATION_RESIZING_BOTTOM_LEFT,
		THEME_LOCATION_RESIZING_LEFT
	};

	struct weston_wm *wm = window->wm;
	struct weston_seat *seat = weston_wm_pick_seat(wm);
	int detail;
	struct weston_shell_interface *shell_interface =
		&wm->server->compositor->shell_interface;

	if (seat->pointer->button_count != 1 || !window->view
	    || seat->pointer->focus != window->view)
		return;

	detail = client_message->data.data32[2];
	switch (detail) {
	case _NET_WM_MOVERESIZE_MOVE:
		shell_interface->move(window->shsurf, seat);
		break;
	case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
	case _NET_WM_MOVERESIZE_SIZE_TOP:
	case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
	case _NET_WM_MOVERESIZE_SIZE_RIGHT:
	case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
	case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
	case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
	case _NET_WM_MOVERESIZE_SIZE_LEFT:
		shell_interface->resize(window->shsurf, seat, map[detail]);
		break;
	case _NET_WM_MOVERESIZE_CANCEL:
		break;
	}
}

#define _NET_WM_STATE_REMOVE	0
#define _NET_WM_STATE_ADD	1
#define _NET_WM_STATE_TOGGLE	2

static int
update_state(int action, int *state)
{
	int new_state, changed;

	switch (action) {
	case _NET_WM_STATE_REMOVE:
		new_state = 0;
		break;
	case _NET_WM_STATE_ADD:
		new_state = 1;
		break;
	case _NET_WM_STATE_TOGGLE:
		new_state = !*state;
		break;
	default:
		return 0;
	}

	changed = (*state != new_state);
	*state = new_state;

	return changed;
}

static void
weston_wm_window_configure(void *data);

static void
weston_wm_window_handle_state(struct weston_wm_window *window,
			      xcb_client_message_event_t *client_message)
{
	struct weston_wm *wm = window->wm;
	struct weston_shell_interface *shell_interface =
		&wm->server->compositor->shell_interface;
	uint32_t action, property;

	action = client_message->data.data32[0];
	property = client_message->data.data32[1];

	if (property == wm->atom.net_wm_state_fullscreen &&
	    update_state(action, &window->fullscreen)) {
		weston_wm_window_set_net_wm_state(window);
		if (window->fullscreen) {
			window->saved_width = window->width;
			window->saved_height = window->height;

			if (window->shsurf)
				shell_interface->set_fullscreen(window->shsurf,
								WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
								0, NULL);
		} else {
			shell_interface->set_toplevel(window->shsurf);
			window->width = window->saved_width;
			window->height = window->saved_height;
			if (window->frame)
				frame_resize_inside(window->frame,
						    window->width,
						    window->height);
			weston_wm_window_configure(window);
		}
	}
}

static void
surface_destroy(struct wl_listener *listener, void *data)
{
	struct weston_wm_window *window =
		container_of(listener,
			     struct weston_wm_window, surface_destroy_listener);

	wm_log("surface for xid %d destroyed\n", window->id);

	/* This should have been freed by the shell.
	 * Don't try to use it later. */
	window->shsurf = NULL;
	window->surface = NULL;
	window->view = NULL;
}

static void
weston_wm_window_handle_surface_id(struct weston_wm_window *window,
				   xcb_client_message_event_t *client_message)
{
	struct weston_wm *wm = window->wm;
	struct wl_resource *resource;

	if (window->surface_id != 0) {
		wm_log("already have surface id for window %d\n", window->id);
		return;
	}

	/* Xwayland will send the wayland requests to create the
	 * wl_surface before sending this client message.  Even so, we
	 * can end up handling the X event before the wayland requests
	 * and thus when we try to look up the surface ID, the surface
	 * hasn't been created yet.  In that case put the window on
	 * the unpaired window list and continue when the surface gets
	 * created. */
	window->surface_id = client_message->data.data32[0];
	resource = wl_client_get_object(wm->server->client,
					window->surface_id);
	if (resource)
		xserver_map_shell_surface(window,
					  wl_resource_get_user_data(resource));
	else
		wl_list_insert(&wm->unpaired_window_list, &window->link);
}

static void
weston_wm_handle_client_message(struct weston_wm *wm,
				xcb_generic_event_t *event)
{
	xcb_client_message_event_t *client_message =
		(xcb_client_message_event_t *) event;
	struct weston_wm_window *window;

	window = hash_table_lookup(wm->window_hash, client_message->window);

	wm_log("XCB_CLIENT_MESSAGE (%s %d %d %d %d %d win %d)\n",
	       get_atom_name(wm->conn, client_message->type),
	       client_message->data.data32[0],
	       client_message->data.data32[1],
	       client_message->data.data32[2],
	       client_message->data.data32[3],
	       client_message->data.data32[4],
	       client_message->window);

	if (client_message->type == wm->atom.net_wm_moveresize)
		weston_wm_window_handle_moveresize(window, client_message);
	else if (client_message->type == wm->atom.net_wm_state)
		weston_wm_window_handle_state(window, client_message);
	else if (client_message->type == wm->atom.wl_surface_id)
		weston_wm_window_handle_surface_id(window, client_message);
}

enum cursor_type {
	XWM_CURSOR_TOP,
	XWM_CURSOR_BOTTOM,
	XWM_CURSOR_LEFT,
	XWM_CURSOR_RIGHT,
	XWM_CURSOR_TOP_LEFT,
	XWM_CURSOR_TOP_RIGHT,
	XWM_CURSOR_BOTTOM_LEFT,
	XWM_CURSOR_BOTTOM_RIGHT,
	XWM_CURSOR_LEFT_PTR,
};

/*
 * The following correspondences between file names and cursors was copied
 * from: https://bugs.kde.org/attachment.cgi?id=67313
 */

static const char *bottom_left_corners[] = {
	"bottom_left_corner",
	"sw-resize",
	"size_bdiag"
};

static const char *bottom_right_corners[] = {
	"bottom_right_corner",
	"se-resize",
	"size_fdiag"
};

static const char *bottom_sides[] = {
	"bottom_side",
	"s-resize",
	"size_ver"
};

static const char *left_ptrs[] = {
	"left_ptr",
	"default",
	"top_left_arrow",
	"left-arrow"
};

static const char *left_sides[] = {
	"left_side",
	"w-resize",
	"size_hor"
};

static const char *right_sides[] = {
	"right_side",
	"e-resize",
	"size_hor"
};

static const char *top_left_corners[] = {
	"top_left_corner",
	"nw-resize",
	"size_fdiag"
};

static const char *top_right_corners[] = {
	"top_right_corner",
	"ne-resize",
	"size_bdiag"
};

static const char *top_sides[] = {
	"top_side",
	"n-resize",
	"size_ver"
};

struct cursor_alternatives {
	const char **names;
	size_t count;
};

static const struct cursor_alternatives cursors[] = {
	{top_sides, ARRAY_LENGTH(top_sides)},
	{bottom_sides, ARRAY_LENGTH(bottom_sides)},
	{left_sides, ARRAY_LENGTH(left_sides)},
	{right_sides, ARRAY_LENGTH(right_sides)},
	{top_left_corners, ARRAY_LENGTH(top_left_corners)},
	{top_right_corners, ARRAY_LENGTH(top_right_corners)},
	{bottom_left_corners, ARRAY_LENGTH(bottom_left_corners)},
	{bottom_right_corners, ARRAY_LENGTH(bottom_right_corners)},
	{left_ptrs, ARRAY_LENGTH(left_ptrs)},
};

static void
weston_wm_create_cursors(struct weston_wm *wm)
{
	const char *name;
	int i, count = ARRAY_LENGTH(cursors);
	size_t j;

	wm->cursors = malloc(count * sizeof(xcb_cursor_t));
	for (i = 0; i < count; i++) {
		for (j = 0; j < cursors[i].count; j++) {
			name = cursors[i].names[j];
			wm->cursors[i] =
				xcb_cursor_library_load_cursor(wm, name);
			if (wm->cursors[i] != (xcb_cursor_t)-1)
				break;
		}
	}

	wm->last_cursor = -1;
}

static void
weston_wm_destroy_cursors(struct weston_wm *wm)
{
	uint8_t i;

	for (i = 0; i < ARRAY_LENGTH(cursors); i++)
		xcb_free_cursor(wm->conn, wm->cursors[i]);

	free(wm->cursors);
}

static int
get_cursor_for_location(enum theme_location location)
{
	// int location = theme_get_location(t, x, y, width, height, 0);

	switch (location) {
		case THEME_LOCATION_RESIZING_TOP:
			return XWM_CURSOR_TOP;
		case THEME_LOCATION_RESIZING_BOTTOM:
			return XWM_CURSOR_BOTTOM;
		case THEME_LOCATION_RESIZING_LEFT:
			return XWM_CURSOR_LEFT;
		case THEME_LOCATION_RESIZING_RIGHT:
			return XWM_CURSOR_RIGHT;
		case THEME_LOCATION_RESIZING_TOP_LEFT:
			return XWM_CURSOR_TOP_LEFT;
		case THEME_LOCATION_RESIZING_TOP_RIGHT:
			return XWM_CURSOR_TOP_RIGHT;
		case THEME_LOCATION_RESIZING_BOTTOM_LEFT:
			return XWM_CURSOR_BOTTOM_LEFT;
		case THEME_LOCATION_RESIZING_BOTTOM_RIGHT:
			return XWM_CURSOR_BOTTOM_RIGHT;
		case THEME_LOCATION_EXTERIOR:
		case THEME_LOCATION_TITLEBAR:
		default:
			return XWM_CURSOR_LEFT_PTR;
	}
}

static void
weston_wm_window_set_cursor(struct weston_wm *wm, xcb_window_t window_id,
			    int cursor)
{
	uint32_t cursor_value_list;

	if (wm->last_cursor == cursor)
		return;

	wm->last_cursor = cursor;

	cursor_value_list = wm->cursors[cursor];
	xcb_change_window_attributes (wm->conn, window_id,
				      XCB_CW_CURSOR, &cursor_value_list);
	xcb_flush(wm->conn);
}

static void
weston_wm_window_close(struct weston_wm_window *window, xcb_timestamp_t time)
{
	xcb_client_message_event_t client_message;

	if (window->delete_window) {
		client_message.response_type = XCB_CLIENT_MESSAGE;
		client_message.format = 32;
		client_message.window = window->id;
		client_message.type = window->wm->atom.wm_protocols;
		client_message.data.data32[0] =
			window->wm->atom.wm_delete_window;
		client_message.data.data32[1] = time;

		xcb_send_event(window->wm->conn, 0, window->id,
			       XCB_EVENT_MASK_NO_EVENT,
			       (char *) &client_message);
	} else {
		xcb_kill_client(window->wm->conn, window->id);
	}
}

static void
weston_wm_handle_button(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_button_press_event_t *button = (xcb_button_press_event_t *) event;
	struct weston_shell_interface *shell_interface =
		&wm->server->compositor->shell_interface;
	struct weston_seat *seat = weston_wm_pick_seat(wm);
	struct weston_wm_window *window;
	enum theme_location location;
	enum frame_button_state button_state;
	uint32_t button_id;

	wm_log("XCB_BUTTON_%s (detail %d)\n",
	       button->response_type == XCB_BUTTON_PRESS ?
	       "PRESS" : "RELEASE", button->detail);

	window = hash_table_lookup(wm->window_hash, button->event);
	if (!window || !window->decorate)
		return;

	if (button->detail != 1 && button->detail != 2)
		return;

	button_state = button->response_type == XCB_BUTTON_PRESS ?
		FRAME_BUTTON_PRESSED : FRAME_BUTTON_RELEASED;
	button_id = button->detail == 1 ? BTN_LEFT : BTN_RIGHT;

	location = frame_pointer_button(window->frame, NULL,
					button_id, button_state);
	if (frame_status(window->frame) & FRAME_STATUS_REPAINT)
		weston_wm_window_schedule_repaint(window);

	if (frame_status(window->frame) & FRAME_STATUS_MOVE) {
		shell_interface->move(window->shsurf, seat);
		frame_status_clear(window->frame, FRAME_STATUS_MOVE);
	}

	if (frame_status(window->frame) & FRAME_STATUS_RESIZE) {
		shell_interface->resize(window->shsurf, seat, location);
		frame_status_clear(window->frame, FRAME_STATUS_RESIZE);
	}

	if (frame_status(window->frame) & FRAME_STATUS_CLOSE) {
		weston_wm_window_close(window, button->time);
		frame_status_clear(window->frame, FRAME_STATUS_CLOSE);
	}
}

static void
weston_wm_handle_motion(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *) event;
	struct weston_wm_window *window;
	enum theme_location location;
	int cursor;

	window = hash_table_lookup(wm->window_hash, motion->event);
	if (!window || !window->decorate)
		return;

	location = frame_pointer_motion(window->frame, NULL,
					motion->event_x, motion->event_y);
	if (frame_status(window->frame) & FRAME_STATUS_REPAINT)
		weston_wm_window_schedule_repaint(window);

	cursor = get_cursor_for_location(location);
	weston_wm_window_set_cursor(wm, window->frame_id, cursor);
}

static void
weston_wm_handle_enter(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_enter_notify_event_t *enter = (xcb_enter_notify_event_t *) event;
	struct weston_wm_window *window;
	enum theme_location location;
	int cursor;

	window = hash_table_lookup(wm->window_hash, enter->event);
	if (!window || !window->decorate)
		return;

	location = frame_pointer_enter(window->frame, NULL,
				       enter->event_x, enter->event_y);
	if (frame_status(window->frame) & FRAME_STATUS_REPAINT)
		weston_wm_window_schedule_repaint(window);

	cursor = get_cursor_for_location(location);
	weston_wm_window_set_cursor(wm, window->frame_id, cursor);
}

static void
weston_wm_handle_leave(struct weston_wm *wm, xcb_generic_event_t *event)
{
	xcb_leave_notify_event_t *leave = (xcb_leave_notify_event_t *) event;
	struct weston_wm_window *window;

	window = hash_table_lookup(wm->window_hash, leave->event);
	if (!window || !window->decorate)
		return;

	frame_pointer_leave(window->frame, NULL);
	if (frame_status(window->frame) & FRAME_STATUS_REPAINT)
		weston_wm_window_schedule_repaint(window);

	weston_wm_window_set_cursor(wm, window->frame_id, XWM_CURSOR_LEFT_PTR);
}

static int
weston_wm_handle_event(int fd, uint32_t mask, void *data)
{
	struct weston_wm *wm = data;
	xcb_generic_event_t *event;
	int count = 0;

	while (event = xcb_poll_for_event(wm->conn), event != NULL) {
		if (weston_wm_handle_selection_event(wm, event)) {
			free(event);
			count++;
			continue;
		}

		if (weston_wm_handle_dnd_event(wm, event)) {
			free(event);
			count++;
			continue;
		}

		switch (EVENT_TYPE(event)) {
		case XCB_BUTTON_PRESS:
		case XCB_BUTTON_RELEASE:
			weston_wm_handle_button(wm, event);
			break;
		case XCB_ENTER_NOTIFY:
			weston_wm_handle_enter(wm, event);
			break;
		case XCB_LEAVE_NOTIFY:
			weston_wm_handle_leave(wm, event);
			break;
		case XCB_MOTION_NOTIFY:
			weston_wm_handle_motion(wm, event);
			break;
		case XCB_CREATE_NOTIFY:
			weston_wm_handle_create_notify(wm, event);
			break;
		case XCB_MAP_REQUEST:
			weston_wm_handle_map_request(wm, event);
			break;
		case XCB_MAP_NOTIFY:
			weston_wm_handle_map_notify(wm, event);
			break;
		case XCB_UNMAP_NOTIFY:
			weston_wm_handle_unmap_notify(wm, event);
			break;
		case XCB_REPARENT_NOTIFY:
			weston_wm_handle_reparent_notify(wm, event);
			break;
		case XCB_CONFIGURE_REQUEST:
			weston_wm_handle_configure_request(wm, event);
			break;
		case XCB_CONFIGURE_NOTIFY:
			weston_wm_handle_configure_notify(wm, event);
			break;
		case XCB_DESTROY_NOTIFY:
			weston_wm_handle_destroy_notify(wm, event);
			break;
		case XCB_MAPPING_NOTIFY:
			wm_log("XCB_MAPPING_NOTIFY\n");
			break;
		case XCB_PROPERTY_NOTIFY:
			weston_wm_handle_property_notify(wm, event);
			break;
		case XCB_CLIENT_MESSAGE:
			weston_wm_handle_client_message(wm, event);
			break;
		}

		free(event);
		count++;
	}

	xcb_flush(wm->conn);

	return count;
}

static void
weston_wm_get_visual_and_colormap(struct weston_wm *wm)
{
	xcb_depth_iterator_t d_iter;
	xcb_visualtype_iterator_t vt_iter;
	xcb_visualtype_t *visualtype;

	d_iter = xcb_screen_allowed_depths_iterator(wm->screen);
	visualtype = NULL;
	while (d_iter.rem > 0) {
		if (d_iter.data->depth == 32) {
			vt_iter = xcb_depth_visuals_iterator(d_iter.data);
			visualtype = vt_iter.data;
			break;
		}

		xcb_depth_next(&d_iter);
	}

	if (visualtype == NULL) {
		weston_log("no 32 bit visualtype\n");
		return;
	}

	wm->visual_id = visualtype->visual_id;
	wm->colormap = xcb_generate_id(wm->conn);
	xcb_create_colormap(wm->conn, XCB_COLORMAP_ALLOC_NONE,
			    wm->colormap, wm->screen->root, wm->visual_id);
}

static void
weston_wm_get_resources(struct weston_wm *wm)
{

#define F(field) offsetof(struct weston_wm, field)

	static const struct { const char *name; int offset; } atoms[] = {
		{ "WM_PROTOCOLS",	F(atom.wm_protocols) },
		{ "WM_NORMAL_HINTS",	F(atom.wm_normal_hints) },
		{ "WM_TAKE_FOCUS",	F(atom.wm_take_focus) },
		{ "WM_DELETE_WINDOW",	F(atom.wm_delete_window) },
		{ "WM_STATE",		F(atom.wm_state) },
		{ "WM_S0",		F(atom.wm_s0) },
		{ "WM_CLIENT_MACHINE",	F(atom.wm_client_machine) },
		{ "_NET_WM_CM_S0",	F(atom.net_wm_cm_s0) },
		{ "_NET_WM_NAME",	F(atom.net_wm_name) },
		{ "_NET_WM_PID",	F(atom.net_wm_pid) },
		{ "_NET_WM_ICON",	F(atom.net_wm_icon) },
		{ "_NET_WM_STATE",	F(atom.net_wm_state) },
		{ "_NET_WM_STATE_FULLSCREEN", F(atom.net_wm_state_fullscreen) },
		{ "_NET_WM_USER_TIME", F(atom.net_wm_user_time) },
		{ "_NET_WM_ICON_NAME", F(atom.net_wm_icon_name) },
		{ "_NET_WM_WINDOW_TYPE", F(atom.net_wm_window_type) },

		{ "_NET_WM_WINDOW_TYPE_DESKTOP", F(atom.net_wm_window_type_desktop) },
		{ "_NET_WM_WINDOW_TYPE_DOCK", F(atom.net_wm_window_type_dock) },
		{ "_NET_WM_WINDOW_TYPE_TOOLBAR", F(atom.net_wm_window_type_toolbar) },
		{ "_NET_WM_WINDOW_TYPE_MENU", F(atom.net_wm_window_type_menu) },
		{ "_NET_WM_WINDOW_TYPE_UTILITY", F(atom.net_wm_window_type_utility) },
		{ "_NET_WM_WINDOW_TYPE_SPLASH", F(atom.net_wm_window_type_splash) },
		{ "_NET_WM_WINDOW_TYPE_DIALOG", F(atom.net_wm_window_type_dialog) },
		{ "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", F(atom.net_wm_window_type_dropdown) },
		{ "_NET_WM_WINDOW_TYPE_POPUP_MENU", F(atom.net_wm_window_type_popup) },
		{ "_NET_WM_WINDOW_TYPE_TOOLTIP", F(atom.net_wm_window_type_tooltip) },
		{ "_NET_WM_WINDOW_TYPE_NOTIFICATION", F(atom.net_wm_window_type_notification) },
		{ "_NET_WM_WINDOW_TYPE_COMBO", F(atom.net_wm_window_type_combo) },
		{ "_NET_WM_WINDOW_TYPE_DND", F(atom.net_wm_window_type_dnd) },
		{ "_NET_WM_WINDOW_TYPE_NORMAL",	F(atom.net_wm_window_type_normal) },

		{ "_NET_WM_MOVERESIZE", F(atom.net_wm_moveresize) },
		{ "_NET_SUPPORTING_WM_CHECK",
					F(atom.net_supporting_wm_check) },
		{ "_NET_SUPPORTED",     F(atom.net_supported) },
		{ "_MOTIF_WM_HINTS",	F(atom.motif_wm_hints) },
		{ "CLIPBOARD",		F(atom.clipboard) },
		{ "CLIPBOARD_MANAGER",	F(atom.clipboard_manager) },
		{ "TARGETS",		F(atom.targets) },
		{ "UTF8_STRING",	F(atom.utf8_string) },
		{ "_WL_SELECTION",	F(atom.wl_selection) },
		{ "INCR",		F(atom.incr) },
		{ "TIMESTAMP",		F(atom.timestamp) },
		{ "MULTIPLE",		F(atom.multiple) },
		{ "UTF8_STRING"	,	F(atom.utf8_string) },
		{ "COMPOUND_TEXT",	F(atom.compound_text) },
		{ "TEXT",		F(atom.text) },
		{ "STRING",		F(atom.string) },
		{ "text/plain;charset=utf-8",	F(atom.text_plain_utf8) },
		{ "text/plain",		F(atom.text_plain) },
		{ "XdndSelection",	F(atom.xdnd_selection) },
		{ "XdndAware",		F(atom.xdnd_aware) },
		{ "XdndEnter",		F(atom.xdnd_enter) },
		{ "XdndLeave",		F(atom.xdnd_leave) },
		{ "XdndDrop",		F(atom.xdnd_drop) },
		{ "XdndStatus",		F(atom.xdnd_status) },
		{ "XdndFinished",	F(atom.xdnd_finished) },
		{ "XdndTypeList",	F(atom.xdnd_type_list) },
		{ "XdndActionCopy",	F(atom.xdnd_action_copy) },
		{ "WL_SURFACE_ID",	F(atom.wl_surface_id) }
	};
#undef F

	xcb_xfixes_query_version_cookie_t xfixes_cookie;
	xcb_xfixes_query_version_reply_t *xfixes_reply;
	xcb_intern_atom_cookie_t cookies[ARRAY_LENGTH(atoms)];
	xcb_intern_atom_reply_t *reply;
	xcb_render_query_pict_formats_reply_t *formats_reply;
	xcb_render_query_pict_formats_cookie_t formats_cookie;
	xcb_render_pictforminfo_t *formats;
	uint32_t i;

	xcb_prefetch_extension_data (wm->conn, &xcb_xfixes_id);
	xcb_prefetch_extension_data (wm->conn, &xcb_composite_id);

	formats_cookie = xcb_render_query_pict_formats(wm->conn);

	for (i = 0; i < ARRAY_LENGTH(atoms); i++)
		cookies[i] = xcb_intern_atom (wm->conn, 0,
					      strlen(atoms[i].name),
					      atoms[i].name);

	for (i = 0; i < ARRAY_LENGTH(atoms); i++) {
		reply = xcb_intern_atom_reply (wm->conn, cookies[i], NULL);
		*(xcb_atom_t *) ((char *) wm + atoms[i].offset) = reply->atom;
		free(reply);
	}

	wm->xfixes = xcb_get_extension_data(wm->conn, &xcb_xfixes_id);
	if (!wm->xfixes || !wm->xfixes->present)
		weston_log("xfixes not available\n");

	xfixes_cookie = xcb_xfixes_query_version(wm->conn,
						 XCB_XFIXES_MAJOR_VERSION,
						 XCB_XFIXES_MINOR_VERSION);
	xfixes_reply = xcb_xfixes_query_version_reply(wm->conn,
						      xfixes_cookie, NULL);

	weston_log("xfixes version: %d.%d\n",
	       xfixes_reply->major_version, xfixes_reply->minor_version);

	free(xfixes_reply);

	formats_reply = xcb_render_query_pict_formats_reply(wm->conn,
							    formats_cookie, 0);
	if (formats_reply == NULL)
		return;

	formats = xcb_render_query_pict_formats_formats(formats_reply);
	for (i = 0; i < formats_reply->num_formats; i++) {
		if (formats[i].direct.red_mask != 0xff &&
		    formats[i].direct.red_shift != 16)
			continue;
		if (formats[i].type == XCB_RENDER_PICT_TYPE_DIRECT &&
		    formats[i].depth == 24)
			wm->format_rgb = formats[i];
		if (formats[i].type == XCB_RENDER_PICT_TYPE_DIRECT &&
		    formats[i].depth == 32 &&
		    formats[i].direct.alpha_mask == 0xff &&
		    formats[i].direct.alpha_shift == 24)
			wm->format_rgba = formats[i];
	}

	free(formats_reply);
}

static void
weston_wm_create_wm_window(struct weston_wm *wm)
{
	static const char name[] = "Weston WM";

	wm->wm_window = xcb_generate_id(wm->conn);
	xcb_create_window(wm->conn,
			  XCB_COPY_FROM_PARENT,
			  wm->wm_window,
			  wm->screen->root,
			  0, 0,
			  10, 10,
			  0,
			  XCB_WINDOW_CLASS_INPUT_OUTPUT,
			  wm->screen->root_visual,
			  0, NULL);

	xcb_change_property(wm->conn,
			    XCB_PROP_MODE_REPLACE,
			    wm->wm_window,
			    wm->atom.net_supporting_wm_check,
			    XCB_ATOM_WINDOW,
			    32, /* format */
			    1, &wm->wm_window);

	xcb_change_property(wm->conn,
			    XCB_PROP_MODE_REPLACE,
			    wm->wm_window,
			    wm->atom.net_wm_name,
			    wm->atom.utf8_string,
			    8, /* format */
			    strlen(name), name);

	xcb_change_property(wm->conn,
			    XCB_PROP_MODE_REPLACE,
			    wm->screen->root,
			    wm->atom.net_supporting_wm_check,
			    XCB_ATOM_WINDOW,
			    32, /* format */
			    1, &wm->wm_window);

	/* Claim the WM_S0 selection even though we don't suport
	 * the --replace functionality. */
	xcb_set_selection_owner(wm->conn,
				wm->wm_window,
				wm->atom.wm_s0,
				XCB_TIME_CURRENT_TIME);

	xcb_set_selection_owner(wm->conn,
				wm->wm_window,
				wm->atom.net_wm_cm_s0,
				XCB_TIME_CURRENT_TIME);
}

struct weston_wm *
weston_wm_create(struct weston_xserver *wxs, int fd)
{
	struct weston_wm *wm;
	struct wl_event_loop *loop;
	xcb_screen_iterator_t s;
	uint32_t values[1];
	xcb_atom_t supported[3];

	wm = zalloc(sizeof *wm);
	if (wm == NULL)
		return NULL;

	wm->server = wxs;
	wm->window_hash = hash_table_create();
	if (wm->window_hash == NULL) {
		free(wm);
		return NULL;
	}

	/* xcb_connect_to_fd takes ownership of the fd. */
	wm->conn = xcb_connect_to_fd(fd, NULL);
	if (xcb_connection_has_error(wm->conn)) {
		weston_log("xcb_connect_to_fd failed\n");
		close(fd);
		hash_table_destroy(wm->window_hash);
		free(wm);
		return NULL;
	}

	s = xcb_setup_roots_iterator(xcb_get_setup(wm->conn));
	wm->screen = s.data;

	loop = wl_display_get_event_loop(wxs->wl_display);
	wm->source =
		wl_event_loop_add_fd(loop, fd,
				     WL_EVENT_READABLE,
				     weston_wm_handle_event, wm);
	wl_event_source_check(wm->source);

	weston_wm_get_resources(wm);
	weston_wm_get_visual_and_colormap(wm);

	values[0] =
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(wm->conn, wm->screen->root,
				     XCB_CW_EVENT_MASK, values);

	xcb_composite_redirect_subwindows(wm->conn, wm->screen->root,
					  XCB_COMPOSITE_REDIRECT_MANUAL);

	wm->theme = theme_create();

	supported[0] = wm->atom.net_wm_moveresize;
	supported[1] = wm->atom.net_wm_state;
	supported[2] = wm->atom.net_wm_state_fullscreen;
	xcb_change_property(wm->conn,
			    XCB_PROP_MODE_REPLACE,
			    wm->screen->root,
			    wm->atom.net_supported,
			    XCB_ATOM_ATOM,
			    32, /* format */
			    ARRAY_LENGTH(supported), supported);

	weston_wm_selection_init(wm);

	weston_wm_dnd_init(wm);

	xcb_flush(wm->conn);

	wm->create_surface_listener.notify = weston_wm_create_surface;
	wl_signal_add(&wxs->compositor->create_surface_signal,
		      &wm->create_surface_listener);
	wm->activate_listener.notify = weston_wm_window_activate;
	wl_signal_add(&wxs->compositor->activate_signal,
		      &wm->activate_listener);
	wm->transform_listener.notify = weston_wm_window_transform;
	wl_signal_add(&wxs->compositor->transform_signal,
		      &wm->transform_listener);
	wm->kill_listener.notify = weston_wm_kill_client;
	wl_signal_add(&wxs->compositor->kill_signal,
		      &wm->kill_listener);
	wl_list_init(&wm->unpaired_window_list);

	weston_wm_create_cursors(wm);
	weston_wm_window_set_cursor(wm, wm->screen->root, XWM_CURSOR_LEFT_PTR);

	/* Create wm window and take WM_S0 selection last, which
	 * signals to Xwayland that we're done with setup. */
	weston_wm_create_wm_window(wm);

	weston_log("created wm, root %d\n", wm->screen->root);

	return wm;
}

void
weston_wm_destroy(struct weston_wm *wm)
{
	/* FIXME: Free windows in hash. */
	hash_table_destroy(wm->window_hash);
	weston_wm_destroy_cursors(wm);
	xcb_disconnect(wm->conn);
	wl_event_source_remove(wm->source);
	wl_list_remove(&wm->selection_listener.link);
	wl_list_remove(&wm->activate_listener.link);
	wl_list_remove(&wm->kill_listener.link);
	wl_list_remove(&wm->transform_listener.link);

	free(wm);
}

static struct weston_wm_window *
get_wm_window(struct weston_surface *surface)
{
	struct wl_listener *listener;

	listener = wl_signal_get(&surface->destroy_signal, surface_destroy);
	if (listener)
		return container_of(listener, struct weston_wm_window,
				    surface_destroy_listener);

	return NULL;
}

static void
weston_wm_window_configure(void *data)
{
	struct weston_wm_window *window = data;
	struct weston_wm *wm = window->wm;
	uint32_t values[4];
	int x, y, width, height;

	weston_wm_window_get_child_position(window, &x, &y);
	values[0] = x;
	values[1] = y;
	values[2] = window->width;
	values[3] = window->height;
	xcb_configure_window(wm->conn,
			     window->id,
			     XCB_CONFIG_WINDOW_X |
			     XCB_CONFIG_WINDOW_Y |
			     XCB_CONFIG_WINDOW_WIDTH |
			     XCB_CONFIG_WINDOW_HEIGHT,
			     values);

	weston_wm_window_get_frame_size(window, &width, &height);
	values[0] = width;
	values[1] = height;
	xcb_configure_window(wm->conn,
			     window->frame_id,
			     XCB_CONFIG_WINDOW_WIDTH |
			     XCB_CONFIG_WINDOW_HEIGHT,
			     values);

	window->configure_source = NULL;

	weston_wm_window_schedule_repaint(window);
}

static void
send_configure(struct weston_surface *surface,
	       uint32_t edges, int32_t width, int32_t height)
{
	struct weston_wm_window *window = get_wm_window(surface);
	struct weston_wm *wm = window->wm;
	struct theme *t = window->wm->theme;
	int vborder, hborder;

	if (window->fullscreen) {
		hborder = 0;
		vborder = 0;
	} else if (window->decorate) {
		hborder = 2 * (t->margin + t->width);
		vborder = 2 * t->margin + t->titlebar_height + t->width;
	} else {
		hborder = 2 * t->margin;
		vborder = 2 * t->margin;
	}

	if (width > hborder)
		window->width = width - hborder;
	else
		window->width = 1;

	if (height > vborder)
		window->height = height - vborder;
	else
		window->height = 1;

	if (window->frame)
		frame_resize_inside(window->frame, window->width, window->height);

	if (window->configure_source)
		return;

	window->configure_source =
		wl_event_loop_add_idle(wm->server->loop,
				       weston_wm_window_configure, window);
}

static const struct weston_shell_client shell_client = {
	send_configure
};

static int
legacy_fullscreen(struct weston_wm *wm,
		  struct weston_wm_window *window,
		  struct weston_output **output_ret)
{
	struct weston_compositor *compositor = wm->server->compositor;
	struct weston_output *output;
	uint32_t minmax = PMinSize | PMaxSize;
	int matching_size;

	/* Heuristics for detecting legacy fullscreen windows... */

	wl_list_for_each(output, &compositor->output_list, link) {
		if (output->x == window->x &&
		    output->y == window->y &&
		    output->width == window->width &&
		    output->height == window->height &&
		    window->override_redirect) {
			*output_ret = output;
			return 1;
		}

		matching_size = 0;
		if ((window->size_hints.flags & (USSize |PSize)) &&
		    window->size_hints.width == output->width &&
		    window->size_hints.height == output->height)
			matching_size = 1;
		if ((window->size_hints.flags & minmax) == minmax &&
		    window->size_hints.min_width == output->width &&
		    window->size_hints.min_height == output->height &&
		    window->size_hints.max_width == output->width &&
		    window->size_hints.max_height == output->height)
			matching_size = 1;

		if (matching_size && !window->decorate &&
		    (window->size_hints.flags & (USPosition | PPosition)) &&
		    window->size_hints.x == output->x &&
		    window->size_hints.y == output->y) {
			*output_ret = output;
			return 1;
		}
	}

	return 0;
}

static void
xserver_map_shell_surface(struct weston_wm_window *window,
			  struct weston_surface *surface)
{
	struct weston_wm *wm = window->wm;
	struct weston_shell_interface *shell_interface =
		&wm->server->compositor->shell_interface;
	struct weston_output *output;
	struct weston_wm_window *parent;

	weston_wm_window_read_properties(window);

	/* A weston_wm_window may have many different surfaces assigned
	 * throughout its life, so we must make sure to remove the listener
	 * from the old surface signal list. */
	if (window->surface)
		wl_list_remove(&window->surface_destroy_listener.link);

	window->surface = surface;
	window->surface_destroy_listener.notify = surface_destroy;
	wl_signal_add(&window->surface->destroy_signal,
		      &window->surface_destroy_listener);

	weston_wm_window_schedule_repaint(window);

	if (!shell_interface->create_shell_surface)
		return;

	if (!shell_interface->get_primary_view)
		return;

	window->shsurf = 
		shell_interface->create_shell_surface(shell_interface->shell,
						      window->surface,
						      &shell_client);
	window->view = shell_interface->get_primary_view(shell_interface->shell,
							 window->shsurf);

	if (window->name)
		shell_interface->set_title(window->shsurf, window->name);

	if (window->fullscreen) {
		window->saved_width = window->width;
		window->saved_height = window->height;
		shell_interface->set_fullscreen(window->shsurf,
						WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
						0, NULL);
		return;
	} else if (legacy_fullscreen(wm, window, &output)) {
		window->fullscreen = 1;
		shell_interface->set_fullscreen(window->shsurf,
						WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
						0, output);
	} else if (window->override_redirect) {
		shell_interface->set_xwayland(window->shsurf,
					      window->x,
					      window->y,
					      WL_SHELL_SURFACE_TRANSIENT_INACTIVE);
	} else if (window->transient_for && window->transient_for->surface) {
		parent = window->transient_for;
		shell_interface->set_transient(window->shsurf,
					       parent->surface,
					       window->x - parent->x,
					       window->y - parent->y, 0);
	} else {
		shell_interface->set_toplevel(window->shsurf);
	}
}

/*
 * Copyright © 2011 Intel Corporation
 * Copyright © 2012 Scott Moreau
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

#include <stdlib.h>

#include "compositor.h"
#include "text-cursor-position-server-protocol.h"

struct text_cursor_position {
	struct wl_object base;
	struct weston_compositor *ec;
	struct wl_global *global;
	struct wl_listener destroy_listener;
};

static void
text_cursor_position_notify(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface_resource,
			    wl_fixed_t x, wl_fixed_t y)
{
	weston_text_cursor_position_notify((struct weston_surface *) surface_resource, x, y);
}

struct text_cursor_position_interface text_cursor_position_implementation = {
	text_cursor_position_notify
};

static void
bind_text_cursor_position(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	wl_client_add_object(client, &text_cursor_position_interface,
			     &text_cursor_position_implementation, id, data);
}

static void
text_cursor_position_notifier_destroy(struct wl_listener *listener, void *data)
{
	struct text_cursor_position *text_cursor_position =
		container_of(listener, struct text_cursor_position, destroy_listener);

	wl_display_remove_global(text_cursor_position->ec->wl_display, text_cursor_position->global);
	free(text_cursor_position);
}

void
text_cursor_position_notifier_create(struct weston_compositor *ec)
{
	struct text_cursor_position *text_cursor_position;

	text_cursor_position = malloc(sizeof *text_cursor_position);
	if (text_cursor_position == NULL)
		return;

	text_cursor_position->base.interface = &text_cursor_position_interface;
	text_cursor_position->base.implementation =
		(void(**)(void)) &text_cursor_position_implementation;
	text_cursor_position->ec = ec;

	text_cursor_position->global = wl_display_add_global(ec->wl_display,
						&text_cursor_position_interface,
						text_cursor_position, bind_text_cursor_position);

	text_cursor_position->destroy_listener.notify = text_cursor_position_notifier_destroy;
	wl_signal_add(&ec->destroy_signal, &text_cursor_position->destroy_listener);
}

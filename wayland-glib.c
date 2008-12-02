/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <glib/giochannel.h>
#include "wayland-client.h"
#include "wayland-glib.h"

struct _WaylandSource {
	GSource source;
	GPollFD pfd;
	uint32_t mask;
	struct wl_display *display;
};

static gboolean
wayland_source_prepare(GSource *base, gint *timeout)
{
	WaylandSource *source = (WaylandSource *) base;

	*timeout = -1;

	/* We have to add/remove the GPollFD if we want to update our
	 * poll event mask dynamically.  Instead, let's just flush all
	 * write on idle instead, which is what this amounts to. */

	while (source->mask & WL_DISPLAY_WRITABLE)
		wl_display_iterate(source->display,
				   WL_DISPLAY_WRITABLE);

	return FALSE;
}

static gboolean
wayland_source_check(GSource *base)
{
	WaylandSource *source = (WaylandSource *) base;

	return source->pfd.revents;
}

static gboolean
wayland_source_dispatch(GSource *base,
			GSourceFunc callback,
			gpointer data)
{
	WaylandSource *source = (WaylandSource *) base;

	wl_display_iterate(source->display,
			   WL_DISPLAY_READABLE);

	return TRUE;
}

static GSourceFuncs wayland_source_funcs = {
	wayland_source_prepare,
	wayland_source_check,
	wayland_source_dispatch,
	NULL
};

static int
wayland_source_update(uint32_t mask, void *data)
{
	WaylandSource *source = data;

	source->mask = mask;

	return 0;
}

GSource *
wayland_source_new(struct wl_display *display)
{
	WaylandSource *source;

	source = (WaylandSource *) g_source_new(&wayland_source_funcs,
						sizeof (WaylandSource));
	source->display = display;
	source->pfd.fd = wl_display_get_fd(display,
					   wayland_source_update, source);
	source->pfd.events = G_IO_IN | G_IO_ERR;
	g_source_add_poll(&source->source, &source->pfd);

	return &source->source;
}

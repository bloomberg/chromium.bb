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

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"
#include "wayland-protocol.h"

static const struct wl_message display_events[] = {
	{ "invalid_object", "u" },
	{ "invalid_method", "uu" },
	{ "no_memory", "" },
	{ "global", "osu" },
	{ "range", "u" },
};

WL_EXPORT const struct wl_interface wl_display_interface = {
	"display", 1,
	0, NULL,
	ARRAY_LENGTH(display_events), display_events,
};


static const struct wl_message compositor_methods[] = {
	{ "create_surface", "n" },
	{ "commit", "u" }
};

static const struct wl_message compositor_events[] = {
	{ "acknowledge", "uu" },
	{ "frame", "uu" }
};

WL_EXPORT const struct wl_interface wl_compositor_interface = {
	"compositor", 1,
	ARRAY_LENGTH(compositor_methods), compositor_methods,
	ARRAY_LENGTH(compositor_events), compositor_events,
};


static const struct wl_message surface_methods[] = {
	{ "destroy", "" },
	{ "attach", "uuuuo" },
	{ "map", "iiii" },
	{ "copy", "iiuuiiii" },
	{ "damage", "iiii" }
};

WL_EXPORT const struct wl_interface wl_surface_interface = {
	"surface", 1,
	ARRAY_LENGTH(surface_methods), surface_methods,
	0, NULL,
};


static const struct wl_message input_device_events[] = {
	{ "motion", "iiii" },
	{ "button", "uuiiii" },
	{ "key", "uu" },
};

WL_EXPORT const struct wl_interface wl_input_device_interface = {
	"input_device", 1,
	0, NULL,
	ARRAY_LENGTH(input_device_events), input_device_events,
};


static const struct wl_message output_events[] = {
	{ "presence", "uu" },
};

WL_EXPORT const struct wl_interface wl_output_interface = {
	"output", 1,
	0, NULL,
	ARRAY_LENGTH(output_events), output_events,
};

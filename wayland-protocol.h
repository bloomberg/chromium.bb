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

#ifndef WAYLAND_PROTOCOL_H
#define WAYLAND_PROTOCOL_H

#include <stdint.h>

struct wl_argument {
	uint32_t type;
	void *data;
};

struct wl_message {
	const char *name;
	const char *signature;
	const void **types;
};

struct wl_interface {
	const char *name;
	int version;
	int method_count;
	const struct wl_message *methods;
	int event_count;
	const struct wl_message *events;
};

#define WL_DISPLAY_INVALID_OBJECT	0
#define WL_DISPLAY_INVALID_METHOD	1
#define WL_DISPLAY_NO_MEMORY		2
#define WL_DISPLAY_GLOBAL		3
#define WL_DISPLAY_RANGE		4

extern const struct wl_interface wl_display_interface;


#define WL_COMPOSITOR_CREATE_SURFACE	0
#define WL_COMPOSITOR_COMMIT		1

#define WL_COMPOSITOR_ACKNOWLEDGE	0
#define WL_COMPOSITOR_FRAME		1

extern const struct wl_interface wl_compositor_interface;


#define WL_SURFACE_DESTROY	0
#define WL_SURFACE_ATTACH	1
#define WL_SURFACE_MAP		2
#define WL_SURFACE_COPY		3
#define WL_SURFACE_DAMAGE	4

extern const struct wl_interface wl_surface_interface;


#define WL_INPUT_MOTION	0
#define WL_INPUT_BUTTON	1
#define WL_INPUT_KEY	2

extern const struct wl_interface wl_input_device_interface;


#define WL_OUTPUT_PRESENCE	0

extern const struct wl_interface wl_output_interface;

#endif

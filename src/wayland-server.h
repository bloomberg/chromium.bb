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


/** Use of this header file is discouraged. Prefer including
 *  wayland-server-core.h instead, which does not include the
 *  client protocol header and as such only defines the library
 *  API, excluding the deprecated API below.
 */

#ifndef WAYLAND_SERVER_H
#define WAYLAND_SERVER_H

#include "wayland-server-core.h"

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef WL_HIDE_DEPRECATED

struct wl_object {
	const struct wl_interface *interface;
	const void *implementation;
	uint32_t id;
};

struct wl_resource {
	struct wl_object object;
	wl_resource_destroy_func_t destroy;
	struct wl_list link;
	struct wl_signal destroy_signal;
	struct wl_client *client;
	void *data;
};

struct wl_buffer {
	struct wl_resource resource;
	int32_t width, height;
	uint32_t busy_count;
} WL_DEPRECATED;


uint32_t
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource) WL_DEPRECATED;

struct wl_resource *
wl_client_add_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation,
		     uint32_t id, void *data) WL_DEPRECATED;
struct wl_resource *
wl_client_new_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation, void *data) WL_DEPRECATED;

struct wl_global *
wl_display_add_global(struct wl_display *display,
		      const struct wl_interface *interface,
		      void *data,
		      wl_global_bind_func_t bind) WL_DEPRECATED;

void
wl_display_remove_global(struct wl_display *display,
			 struct wl_global *global) WL_DEPRECATED;

#endif

#ifdef  __cplusplus
}
#endif

#include "wayland-server-protocol.h"

#endif

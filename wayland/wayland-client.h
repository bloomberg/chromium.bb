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

#ifndef _WAYLAND_CLIENT_H
#define _WAYLAND_CLIENT_H

#include "wayland-util.h"
#include "wayland-client-protocol.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define WL_DISPLAY_READABLE 0x01
#define WL_DISPLAY_WRITABLE 0x02

typedef int (*wl_display_update_func_t)(uint32_t mask, void *data);
typedef void (*wl_display_sync_func_t)(void *data);
typedef void (*wl_display_frame_func_t)(void *data, uint32_t time);

struct wl_display *wl_display_connect(const char *name, size_t name_size);
void wl_display_destroy(struct wl_display *display);
int wl_display_get_fd(struct wl_display *display,
		      wl_display_update_func_t update, void *data);
uint32_t wl_display_allocate_id(struct wl_display *display);
void wl_display_iterate(struct wl_display *display, uint32_t mask);
int wl_display_sync_callback(struct wl_display *display,
			     wl_display_sync_func_t func, void *data);
int wl_display_frame_callback(struct wl_display *display,
			      wl_display_frame_func_t func, void *data);

struct wl_global_listener;
typedef void (*wl_display_global_func_t)(struct wl_display *display,
					 uint32_t id,
					 const char *interface,
					 uint32_t version,
					 void *data);
void
wl_display_remove_global_listener(struct wl_display *display,
				  struct wl_global_listener *listener);

struct wl_global_listener *
wl_display_add_global_listener(struct wl_display *display,
			       wl_display_global_func_t handler, void *data);
struct wl_visual *
wl_display_get_argb_visual(struct wl_display *display);
struct wl_visual *
wl_display_get_premultiplied_argb_visual(struct wl_display *display);
struct wl_visual *
wl_display_get_rgb_visual(struct wl_display *display);

#ifdef  __cplusplus
}
#endif

#endif

/* 
 * Copyright © 2013-2016 Collabora, Ltd.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef VIEWPORTER_CLIENT_PROTOCOL_H
#define VIEWPORTER_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wp_viewporter;
struct wp_viewport;

extern const struct wl_interface wp_viewporter_interface;
extern const struct wl_interface wp_viewport_interface;

#ifndef WP_VIEWPORTER_ERROR_ENUM
#define WP_VIEWPORTER_ERROR_ENUM
enum wp_viewporter_error {
	WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS = 0,
};
#endif /* WP_VIEWPORTER_ERROR_ENUM */

#define WP_VIEWPORTER_DESTROY	0
#define WP_VIEWPORTER_GET_VIEWPORT	1

static inline void
wp_viewporter_set_user_data(struct wp_viewporter *wp_viewporter, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wp_viewporter, user_data);
}

static inline void *
wp_viewporter_get_user_data(struct wp_viewporter *wp_viewporter)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wp_viewporter);
}

static inline void
wp_viewporter_destroy(struct wp_viewporter *wp_viewporter)
{
	wl_proxy_marshal((struct wl_proxy *) wp_viewporter,
			 WP_VIEWPORTER_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wp_viewporter);
}

static inline struct wp_viewport *
wp_viewporter_get_viewport(struct wp_viewporter *wp_viewporter, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wp_viewporter,
			 WP_VIEWPORTER_GET_VIEWPORT, &wp_viewport_interface, NULL, surface);

	return (struct wp_viewport *) id;
}

#ifndef WP_VIEWPORT_ERROR_ENUM
#define WP_VIEWPORT_ERROR_ENUM
enum wp_viewport_error {
	WP_VIEWPORT_ERROR_BAD_VALUE = 0,
	WP_VIEWPORT_ERROR_BAD_SIZE = 1,
	WP_VIEWPORT_ERROR_OUT_OF_BUFFER = 2,
	WP_VIEWPORT_ERROR_NO_SURFACE = 3,
};
#endif /* WP_VIEWPORT_ERROR_ENUM */

#define WP_VIEWPORT_DESTROY	0
#define WP_VIEWPORT_SET_SOURCE	1
#define WP_VIEWPORT_SET_DESTINATION	2

static inline void
wp_viewport_set_user_data(struct wp_viewport *wp_viewport, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wp_viewport, user_data);
}

static inline void *
wp_viewport_get_user_data(struct wp_viewport *wp_viewport)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wp_viewport);
}

static inline void
wp_viewport_destroy(struct wp_viewport *wp_viewport)
{
	wl_proxy_marshal((struct wl_proxy *) wp_viewport,
			 WP_VIEWPORT_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wp_viewport);
}

static inline void
wp_viewport_set_source(struct wp_viewport *wp_viewport, wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wp_viewport,
			 WP_VIEWPORT_SET_SOURCE, x, y, width, height);
}

static inline void
wp_viewport_set_destination(struct wp_viewport *wp_viewport, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wp_viewport,
			 WP_VIEWPORT_SET_DESTINATION, width, height);
}

#ifdef  __cplusplus
}
#endif

#endif

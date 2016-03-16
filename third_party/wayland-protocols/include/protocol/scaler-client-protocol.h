/*
 * Copyright © 2013-2014 Collabora, Ltd.
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

#ifndef SCALER_CLIENT_PROTOCOL_H
#define SCALER_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_scaler;
struct wl_surface;
struct wl_viewport;

extern const struct wl_interface wl_scaler_interface;
extern const struct wl_interface wl_viewport_interface;

#ifndef WL_SCALER_ERROR_ENUM
#define WL_SCALER_ERROR_ENUM
enum wl_scaler_error {
	WL_SCALER_ERROR_VIEWPORT_EXISTS = 0,
};
#endif /* WL_SCALER_ERROR_ENUM */

#define WL_SCALER_DESTROY	0
#define WL_SCALER_GET_VIEWPORT	1

#define WL_SCALER_DESTROY_SINCE_VERSION	1
#define WL_SCALER_GET_VIEWPORT_SINCE_VERSION	1

static inline void
wl_scaler_set_user_data(struct wl_scaler *wl_scaler, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_scaler, user_data);
}

static inline void *
wl_scaler_get_user_data(struct wl_scaler *wl_scaler)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_scaler);
}

static inline uint32_t
wl_scaler_get_version(struct wl_scaler *wl_scaler)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_scaler);
}

static inline void
wl_scaler_destroy(struct wl_scaler *wl_scaler)
{
	wl_proxy_marshal((struct wl_proxy *) wl_scaler,
			 WL_SCALER_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_scaler);
}

static inline struct wl_viewport *
wl_scaler_get_viewport(struct wl_scaler *wl_scaler, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_scaler,
			 WL_SCALER_GET_VIEWPORT, &wl_viewport_interface, NULL, surface);

	return (struct wl_viewport *) id;
}

#ifndef WL_VIEWPORT_ERROR_ENUM
#define WL_VIEWPORT_ERROR_ENUM
enum wl_viewport_error {
	WL_VIEWPORT_ERROR_BAD_VALUE = 0,
};
#endif /* WL_VIEWPORT_ERROR_ENUM */

#define WL_VIEWPORT_DESTROY	0
#define WL_VIEWPORT_SET	1
#define WL_VIEWPORT_SET_SOURCE	2
#define WL_VIEWPORT_SET_DESTINATION	3

#define WL_VIEWPORT_DESTROY_SINCE_VERSION	1
#define WL_VIEWPORT_SET_SINCE_VERSION	1
#define WL_VIEWPORT_SET_SOURCE_SINCE_VERSION	2
#define WL_VIEWPORT_SET_DESTINATION_SINCE_VERSION	2

static inline void
wl_viewport_set_user_data(struct wl_viewport *wl_viewport, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_viewport, user_data);
}

static inline void *
wl_viewport_get_user_data(struct wl_viewport *wl_viewport)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_viewport);
}

static inline uint32_t
wl_viewport_get_version(struct wl_viewport *wl_viewport)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_viewport);
}

static inline void
wl_viewport_destroy(struct wl_viewport *wl_viewport)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_viewport);
}

static inline void
wl_viewport_set(struct wl_viewport *wl_viewport, wl_fixed_t src_x, wl_fixed_t src_y, wl_fixed_t src_width, wl_fixed_t src_height, int32_t dst_width, int32_t dst_height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_SET, src_x, src_y, src_width, src_height, dst_width, dst_height);
}

static inline void
wl_viewport_set_source(struct wl_viewport *wl_viewport, wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_SET_SOURCE, x, y, width, height);
}

static inline void
wl_viewport_set_destination(struct wl_viewport *wl_viewport, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_viewport,
			 WL_VIEWPORT_SET_DESTINATION, width, height);
}

#ifdef  __cplusplus
}
#endif

#endif

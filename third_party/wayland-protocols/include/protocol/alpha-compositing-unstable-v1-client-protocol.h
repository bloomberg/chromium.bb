/* 
 * Copyright 2016 The Chromium Authors.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef ALPHA_COMPOSITING_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define ALPHA_COMPOSITING_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct zwp_alpha_compositing_v1;
struct zwp_blending_v1;

extern const struct wl_interface zwp_alpha_compositing_v1_interface;
extern const struct wl_interface zwp_blending_v1_interface;

#ifndef ZWP_ALPHA_COMPOSITING_V1_ERROR_ENUM
#define ZWP_ALPHA_COMPOSITING_V1_ERROR_ENUM
enum zwp_alpha_compositing_v1_error {
	ZWP_ALPHA_COMPOSITING_V1_ERROR_BLENDING_EXISTS = 0,
};
#endif /* ZWP_ALPHA_COMPOSITING_V1_ERROR_ENUM */

#define ZWP_ALPHA_COMPOSITING_V1_DESTROY	0
#define ZWP_ALPHA_COMPOSITING_V1_GET_BLENDING	1

static inline void
zwp_alpha_compositing_v1_set_user_data(struct zwp_alpha_compositing_v1 *zwp_alpha_compositing_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_alpha_compositing_v1, user_data);
}

static inline void *
zwp_alpha_compositing_v1_get_user_data(struct zwp_alpha_compositing_v1 *zwp_alpha_compositing_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_alpha_compositing_v1);
}

static inline void
zwp_alpha_compositing_v1_destroy(struct zwp_alpha_compositing_v1 *zwp_alpha_compositing_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_alpha_compositing_v1,
			 ZWP_ALPHA_COMPOSITING_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_alpha_compositing_v1);
}

static inline struct zwp_blending_v1 *
zwp_alpha_compositing_v1_get_blending(struct zwp_alpha_compositing_v1 *zwp_alpha_compositing_v1, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_alpha_compositing_v1,
			 ZWP_ALPHA_COMPOSITING_V1_GET_BLENDING, &zwp_blending_v1_interface, NULL, surface);

	return (struct zwp_blending_v1 *) id;
}

#ifndef ZWP_BLENDING_V1_BLENDING_EQUATION_ENUM
#define ZWP_BLENDING_V1_BLENDING_EQUATION_ENUM
/**
 * zwp_blending_v1_blending_equation - different blending equations for
 *	compositing
 * @ZWP_BLENDING_V1_BLENDING_EQUATION_NONE: no blending
 * @ZWP_BLENDING_V1_BLENDING_EQUATION_PREMULT: one / one_minus_src_alpha
 * @ZWP_BLENDING_V1_BLENDING_EQUATION_COVERAGE: src_alpha /
 *	one_minus_src_alpha
 *
 * Blending equations that can be used when compositing a surface.
 */
enum zwp_blending_v1_blending_equation {
	ZWP_BLENDING_V1_BLENDING_EQUATION_NONE = 0,
	ZWP_BLENDING_V1_BLENDING_EQUATION_PREMULT = 1,
	ZWP_BLENDING_V1_BLENDING_EQUATION_COVERAGE = 2,
};
#endif /* ZWP_BLENDING_V1_BLENDING_EQUATION_ENUM */

#define ZWP_BLENDING_V1_DESTROY	0
#define ZWP_BLENDING_V1_SET_BLENDING	1
#define ZWP_BLENDING_V1_SET_ALPHA	2

static inline void
zwp_blending_v1_set_user_data(struct zwp_blending_v1 *zwp_blending_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_blending_v1, user_data);
}

static inline void *
zwp_blending_v1_get_user_data(struct zwp_blending_v1 *zwp_blending_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_blending_v1);
}

static inline void
zwp_blending_v1_destroy(struct zwp_blending_v1 *zwp_blending_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_blending_v1,
			 ZWP_BLENDING_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_blending_v1);
}

static inline void
zwp_blending_v1_set_blending(struct zwp_blending_v1 *zwp_blending_v1, uint32_t equation)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_blending_v1,
			 ZWP_BLENDING_V1_SET_BLENDING, equation);
}

static inline void
zwp_blending_v1_set_alpha(struct zwp_blending_v1 *zwp_blending_v1, wl_fixed_t value)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_blending_v1,
			 ZWP_BLENDING_V1_SET_ALPHA, value);
}

#ifdef  __cplusplus
}
#endif

#endif

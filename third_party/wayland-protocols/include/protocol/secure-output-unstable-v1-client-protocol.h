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

#ifndef SECURE_OUTPUT_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define SECURE_OUTPUT_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_surface;
struct zwp_secure_output_v1;
struct zwp_security_v1;

extern const struct wl_interface zwp_secure_output_v1_interface;
extern const struct wl_interface zwp_security_v1_interface;

#ifndef ZWP_SECURE_OUTPUT_V1_ERROR_ENUM
#define ZWP_SECURE_OUTPUT_V1_ERROR_ENUM
enum zwp_secure_output_v1_error {
	ZWP_SECURE_OUTPUT_V1_ERROR_SECURITY_EXISTS = 0,
};
#endif /* ZWP_SECURE_OUTPUT_V1_ERROR_ENUM */

#define ZWP_SECURE_OUTPUT_V1_DESTROY	0
#define ZWP_SECURE_OUTPUT_V1_GET_SECURITY	1

#define ZWP_SECURE_OUTPUT_V1_DESTROY_SINCE_VERSION	1
#define ZWP_SECURE_OUTPUT_V1_GET_SECURITY_SINCE_VERSION	1

static inline void
zwp_secure_output_v1_set_user_data(struct zwp_secure_output_v1 *zwp_secure_output_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_secure_output_v1, user_data);
}

static inline void *
zwp_secure_output_v1_get_user_data(struct zwp_secure_output_v1 *zwp_secure_output_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_secure_output_v1);
}

static inline uint32_t
zwp_secure_output_v1_get_version(struct zwp_secure_output_v1 *zwp_secure_output_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwp_secure_output_v1);
}

static inline void
zwp_secure_output_v1_destroy(struct zwp_secure_output_v1 *zwp_secure_output_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_secure_output_v1,
			 ZWP_SECURE_OUTPUT_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_secure_output_v1);
}

static inline struct zwp_security_v1 *
zwp_secure_output_v1_get_security(struct zwp_secure_output_v1 *zwp_secure_output_v1, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_secure_output_v1,
			 ZWP_SECURE_OUTPUT_V1_GET_SECURITY, &zwp_security_v1_interface, NULL, surface);

	return (struct zwp_security_v1 *) id;
}

#define ZWP_SECURITY_V1_DESTROY	0
#define ZWP_SECURITY_V1_ONLY_VISIBLE_ON_SECURE_OUTPUT	1

#define ZWP_SECURITY_V1_DESTROY_SINCE_VERSION	1
#define ZWP_SECURITY_V1_ONLY_VISIBLE_ON_SECURE_OUTPUT_SINCE_VERSION	1

static inline void
zwp_security_v1_set_user_data(struct zwp_security_v1 *zwp_security_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_security_v1, user_data);
}

static inline void *
zwp_security_v1_get_user_data(struct zwp_security_v1 *zwp_security_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_security_v1);
}

static inline uint32_t
zwp_security_v1_get_version(struct zwp_security_v1 *zwp_security_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwp_security_v1);
}

static inline void
zwp_security_v1_destroy(struct zwp_security_v1 *zwp_security_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_security_v1,
			 ZWP_SECURITY_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_security_v1);
}

static inline void
zwp_security_v1_only_visible_on_secure_output(struct zwp_security_v1 *zwp_security_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_security_v1,
			 ZWP_SECURITY_V1_ONLY_VISIBLE_ON_SECURE_OUTPUT);
}

#ifdef  __cplusplus
}
#endif

#endif

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

#ifndef SECURE_OUTPUT_UNSTABLE_V1_SERVER_PROTOCOL_H
#define SECURE_OUTPUT_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

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

/**
 * zwp_secure_output_v1 - secure output
 * @destroy: unbind from the secure output interface
 * @get_security: extend surface interface for security
 *
 * The global interface exposing secure output capabilities is used to
 * instantiate an interface extension for a wl_surface object. This
 * extended interface will then allow surfaces to be marked as as only
 * visible on secure outputs.
 */
struct zwp_secure_output_v1_interface {
	/**
	 * destroy - unbind from the secure output interface
	 *
	 * Informs the server that the client will not be using this
	 * protocol object anymore. This does not affect any other objects,
	 * security objects included.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * get_security - extend surface interface for security
	 * @id: the new security interface id
	 * @surface: the surface
	 *
	 * Instantiate an interface extension for the given wl_surface to
	 * provide surface security. If the given wl_surface already has a
	 * security object associated, the security_exists protocol error
	 * is raised.
	 */
	void (*get_security)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id,
			     struct wl_resource *surface);
};


/**
 * zwp_security_v1 - security interface to a wl_surface
 * @destroy: remove security from the surface
 * @only_visible_on_secure_output: set the only visible on secure output
 *	state
 *
 * An additional interface to a wl_surface object, which allows the
 * client to specify that a surface should not appear in screenshots or be
 * visible on non-secure outputs.
 *
 * If the wl_surface associated with the security object is destroyed, the
 * security object becomes inert.
 *
 * If the security object is destroyed, the security state is removed from
 * the wl_surface. The change will be applied on the next
 * wl_surface.commit.
 */
struct zwp_security_v1_interface {
	/**
	 * destroy - remove security from the surface
	 *
	 * The associated wl_surface's security state is removed. The
	 * change is applied on the next wl_surface.commit.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * only_visible_on_secure_output - set the only visible on secure
	 *	output state
	 *
	 * Constrain visibility of wl_surface contents to secure outputs.
	 * See wp_secure_output for the description.
	 *
	 * The only visible on secure output state is double-buffered
	 * state, and will be applied on the next wl_surface.commit.
	 */
	void (*only_visible_on_secure_output)(struct wl_client *client,
					      struct wl_resource *resource);
};


#ifdef  __cplusplus
}
#endif

#endif

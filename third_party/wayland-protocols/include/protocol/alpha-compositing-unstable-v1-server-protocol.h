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

#ifndef ALPHA_COMPOSITING_UNSTABLE_V1_SERVER_PROTOCOL_H
#define ALPHA_COMPOSITING_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

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

/**
 * zwp_alpha_compositing_v1 - alpha_compositing
 * @destroy: unbind from the blending interface
 * @get_blending: extend surface interface for blending
 *
 * The global interface exposing compositing and blending capabilities is
 * used to instantiate an interface extension for a wl_surface object. This
 * extended interface will then allow the client to specify the blending
 * equation and alpha value used for compositing the wl_surface.
 */
struct zwp_alpha_compositing_v1_interface {
	/**
	 * destroy - unbind from the blending interface
	 *
	 * Informs the server that the client will not be using this
	 * protocol object anymore. This does not affect any other objects,
	 * blending objects included.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * get_blending - extend surface interface for blending
	 * @id: the new blending interface id
	 * @surface: the surface
	 *
	 * Instantiate an interface extension for the given wl_surface to
	 * provide surface blending. If the given wl_surface already has a
	 * blending object associated, the blending_exists protocol error
	 * is raised.
	 */
	void (*get_blending)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id,
			     struct wl_resource *surface);
};

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

/**
 * zwp_blending_v1 - blending interface to a wl_surface
 * @destroy: remove blending from the surface
 * @set_blending: set the blending equation
 * @set_alpha: set the alpha value
 *
 * An additional interface to a wl_surface object, which allows the
 * client to specify the blending equation used for compositing and an
 * alpha value applied to the whole surface.
 *
 * If the wl_surface associated with the bledning object is destroyed, the
 * blending object becomes inert.
 *
 * If the blending object is destroyed, the blending state is removed from
 * the wl_surface. The change will be applied on the next
 * wl_surface.commit.
 */
struct zwp_blending_v1_interface {
	/**
	 * destroy - remove blending from the surface
	 *
	 * The associated wl_surface's blending state is removed. The
	 * change is applied on the next wl_surface.commit.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * set_blending - set the blending equation
	 * @equation: the new blending equation
	 *
	 * Set the blending equation for compositing the wl_surface. See
	 * wp_alpha_compositing for the description.
	 *
	 * The blending equation state is double-buffered state, and will
	 * be applied on the next wl_surface.commit.
	 */
	void (*set_blending)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t equation);
	/**
	 * set_alpha - set the alpha value
	 * @value: the new alpha value
	 *
	 * Set the alpha value applied to the whole surface for
	 * compositing. See wp_alpha_compositing for the description.
	 *
	 * The alpha value state is double-buffered state, and will be
	 * applied on the next wl_surface.commit.
	 */
	void (*set_alpha)(struct wl_client *client,
			  struct wl_resource *resource,
			  wl_fixed_t value);
};

#ifdef  __cplusplus
}
#endif

#endif

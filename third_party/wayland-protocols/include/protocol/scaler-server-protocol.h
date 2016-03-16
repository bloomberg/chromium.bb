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

#ifndef SCALER_SERVER_PROTOCOL_H
#define SCALER_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

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

/**
 * wl_scaler - surface cropping and scaling
 * @destroy: unbind from the cropping and scaling interface
 * @get_viewport: extend surface interface for crop and scale
 *
 * The global interface exposing surface cropping and scaling
 * capabilities is used to instantiate an interface extension for a
 * wl_surface object. This extended interface will then allow cropping and
 * scaling the surface contents, effectively disconnecting the direct
 * relationship between the buffer and the surface size.
 */
struct wl_scaler_interface {
	/**
	 * destroy - unbind from the cropping and scaling interface
	 *
	 * Informs the server that the client will not be using this
	 * protocol object anymore. This does not affect any other objects,
	 * wl_viewport objects included.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * get_viewport - extend surface interface for crop and scale
	 * @id: the new viewport interface id
	 * @surface: the surface
	 *
	 * Instantiate an interface extension for the given wl_surface to
	 * crop and scale its content. If the given wl_surface already has
	 * a wl_viewport object associated, the viewport_exists protocol
	 * error is raised.
	 */
	void (*get_viewport)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id,
			     struct wl_resource *surface);
};


#ifndef WL_VIEWPORT_ERROR_ENUM
#define WL_VIEWPORT_ERROR_ENUM
enum wl_viewport_error {
	WL_VIEWPORT_ERROR_BAD_VALUE = 0,
};
#endif /* WL_VIEWPORT_ERROR_ENUM */

/**
 * wl_viewport - crop and scale interface to a wl_surface
 * @destroy: remove scaling and cropping from the surface
 * @set: set the crop and scale state
 * @set_source: set the source rectangle for cropping
 * @set_destination: set the surface size for scaling
 *
 * An additional interface to a wl_surface object, which allows the
 * client to specify the cropping and scaling of the surface contents.
 *
 * This interface allows to define the source rectangle (src_x, src_y,
 * src_width, src_height) from where to take the wl_buffer contents, and
 * scale that to destination size (dst_width, dst_height). This state is
 * double-buffered, and is applied on the next wl_surface.commit.
 *
 * The two parts of crop and scale state are independent: the source
 * rectangle, and the destination size. Initially both are unset, that is,
 * no scaling is applied. The whole of the current wl_buffer is used as the
 * source, and the surface size is as defined in wl_surface.attach.
 *
 * If the destination size is set, it causes the surface size to become
 * dst_width, dst_height. The source (rectangle) is scaled to exactly this
 * size. This overrides whatever the attached wl_buffer size is, unless the
 * wl_buffer is NULL. If the wl_buffer is NULL, the surface has no content
 * and therefore no size. Otherwise, the size is always at least 1x1 in
 * surface coordinates.
 *
 * If the source rectangle is set, it defines what area of the wl_buffer is
 * taken as the source. If the source rectangle is set and the destination
 * size is not set, the surface size becomes the source rectangle size
 * rounded up to the nearest integer. If the source size is already exactly
 * integers, this results in cropping without scaling.
 *
 * The coordinate transformations from buffer pixel coordinates up to the
 * surface-local coordinates happen in the following order: 1.
 * buffer_transform (wl_surface.set_buffer_transform) 2. buffer_scale
 * (wl_surface.set_buffer_scale) 3. crop and scale (wl_viewport.set*) This
 * means, that the source rectangle coordinates of crop and scale are given
 * in the coordinates after the buffer transform and scale, i.e. in the
 * coordinates that would be the surface-local coordinates if the crop and
 * scale was not applied.
 *
 * If the source rectangle is partially or completely outside of the
 * wl_buffer, then the surface contents are undefined (not void), and the
 * surface size is still dst_width, dst_height.
 *
 * The x, y arguments of wl_surface.attach are applied as normal to the
 * surface. They indicate how many pixels to remove from the surface size
 * from the left and the top. In other words, they are still in the
 * surface-local coordinate system, just like dst_width and dst_height are.
 *
 * If the wl_surface associated with the wl_viewport is destroyed, the
 * wl_viewport object becomes inert.
 *
 * If the wl_viewport object is destroyed, the crop and scale state is
 * removed from the wl_surface. The change will be applied on the next
 * wl_surface.commit.
 */
struct wl_viewport_interface {
	/**
	 * destroy - remove scaling and cropping from the surface
	 *
	 * The associated wl_surface's crop and scale state is removed.
	 * The change is applied on the next wl_surface.commit.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * set - set the crop and scale state
	 * @src_x: source rectangle x
	 * @src_y: source rectangle y
	 * @src_width: source rectangle width
	 * @src_height: source rectangle height
	 * @dst_width: surface width
	 * @dst_height: surface height
	 *
	 * Set both source rectangle and destination size of the
	 * associated wl_surface. See wl_viewport for the description, and
	 * relation to the wl_buffer size.
	 *
	 * The bad_value protocol error is raised if src_width or
	 * src_height is negative, or if dst_width or dst_height is not
	 * positive.
	 *
	 * The crop and scale state is double-buffered state, and will be
	 * applied on the next wl_surface.commit.
	 *
	 * Arguments dst_x and dst_y do not exist here, use the x and y
	 * arguments to wl_surface.attach. The x, y, dst_width, and
	 * dst_height define the surface-local coordinate system
	 * irrespective of the attached wl_buffer size.
	 */
	void (*set)(struct wl_client *client,
		    struct wl_resource *resource,
		    wl_fixed_t src_x,
		    wl_fixed_t src_y,
		    wl_fixed_t src_width,
		    wl_fixed_t src_height,
		    int32_t dst_width,
		    int32_t dst_height);
	/**
	 * set_source - set the source rectangle for cropping
	 * @x: source rectangle x
	 * @y: source rectangle y
	 * @width: source rectangle width
	 * @height: source rectangle height
	 *
	 * Set the source rectangle of the associated wl_surface. See
	 * wl_viewport for the description, and relation to the wl_buffer
	 * size.
	 *
	 * If width is -1.0 and height is -1.0, the source rectangle is
	 * unset instead. Any other pair of values for width and height
	 * that contains zero or negative values raises the bad_value
	 * protocol error.
	 *
	 * The crop and scale state is double-buffered state, and will be
	 * applied on the next wl_surface.commit.
	 * @since: 2
	 */
	void (*set_source)(struct wl_client *client,
			   struct wl_resource *resource,
			   wl_fixed_t x,
			   wl_fixed_t y,
			   wl_fixed_t width,
			   wl_fixed_t height);
	/**
	 * set_destination - set the surface size for scaling
	 * @width: surface width
	 * @height: surface height
	 *
	 * Set the destination size of the associated wl_surface. See
	 * wl_viewport for the description, and relation to the wl_buffer
	 * size.
	 *
	 * If width is -1 and height is -1, the destination size is unset
	 * instead. Any other pair of values for width and height that
	 * contains zero or negative values raises the bad_value protocol
	 * error.
	 *
	 * The crop and scale state is double-buffered state, and will be
	 * applied on the next wl_surface.commit.
	 *
	 * Arguments x and y do not exist here, use the x and y arguments
	 * to wl_surface.attach. The x, y, width, and height define the
	 * surface-local coordinate system irrespective of the attached
	 * wl_buffer size.
	 * @since: 2
	 */
	void (*set_destination)(struct wl_client *client,
				struct wl_resource *resource,
				int32_t width,
				int32_t height);
};


#ifdef  __cplusplus
}
#endif

#endif

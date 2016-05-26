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

#ifndef REMOTE_SHELL_UNSTABLE_V1_SERVER_PROTOCOL_H
#define REMOTE_SHELL_UNSTABLE_V1_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct zwp_remote_shell_v1;
struct zwp_remote_surface_v1;

extern const struct wl_interface zwp_remote_shell_v1_interface;
extern const struct wl_interface zwp_remote_surface_v1_interface;

#ifndef ZWP_REMOTE_SHELL_V1_CONTAINER_ENUM
#define ZWP_REMOTE_SHELL_V1_CONTAINER_ENUM
/**
 * zwp_remote_shell_v1_container - containers for remote surfaces
 * @ZWP_REMOTE_SHELL_V1_CONTAINER_DEFAULT: default container
 * @ZWP_REMOTE_SHELL_V1_CONTAINER_OVERLAY: system modal container
 *
 * Determine how a remote surface should be stacked relative to other
 * shell surfaces.
 */
enum zwp_remote_shell_v1_container {
	ZWP_REMOTE_SHELL_V1_CONTAINER_DEFAULT = 1,
	ZWP_REMOTE_SHELL_V1_CONTAINER_OVERLAY = 2,
};
#endif /* ZWP_REMOTE_SHELL_V1_CONTAINER_ENUM */

#ifndef ZWP_REMOTE_SHELL_V1_ERROR_ENUM
#define ZWP_REMOTE_SHELL_V1_ERROR_ENUM
enum zwp_remote_shell_v1_error {
	ZWP_REMOTE_SHELL_V1_ERROR_ROLE = 0,
};
#endif /* ZWP_REMOTE_SHELL_V1_ERROR_ENUM */

/**
 * zwp_remote_shell_v1 - remote_shell
 * @destroy: destroy remote_shell
 * @get_remote_surface: create a remote shell surface from a surface
 *
 * The global interface that allows clients to turn a wl_surface into a
 * "real window" which is remotely managed but can be stacked, activated
 * and made fullscreen by the user.
 */
struct zwp_remote_shell_v1_interface {
	/**
	 * destroy - destroy remote_shell
	 *
	 * Destroy this remote_shell object.
	 *
	 * Destroying a bound remote_shell object while there are surfaces
	 * still alive created by this remote_shell object instance is
	 * illegal and will result in a protocol error.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * get_remote_surface - create a remote shell surface from a
	 *	surface
	 * @id: (none)
	 * @surface: (none)
	 * @container: (none)
	 *
	 * This creates an remote_surface for the given surface and gives
	 * it the remote_surface role. A wl_surface can only be given a
	 * remote_surface role once. If get_remote_surface is called with a
	 * wl_surface that already has an active remote_surface associated
	 * with it, or if it had any other role, an error is raised.
	 *
	 * See the documentation of remote_surface for more details about
	 * what an remote_surface is and how it is used.
	 */
	void (*get_remote_surface)(struct wl_client *client,
				   struct wl_resource *resource,
				   uint32_t id,
				   struct wl_resource *surface,
				   uint32_t container);
};

#define ZWP_REMOTE_SHELL_V1_CONFIGURE	0
#define ZWP_REMOTE_SHELL_V1_ACTIVATED	1

static inline void
zwp_remote_shell_v1_send_configure(struct wl_resource *resource_, int32_t width, int32_t height, int32_t work_area_inset_left, int32_t work_area_inset_top, int32_t work_area_inset_right, int32_t work_area_inset_bottom)
{
	wl_resource_post_event(resource_, ZWP_REMOTE_SHELL_V1_CONFIGURE, width, height, work_area_inset_left, work_area_inset_top, work_area_inset_right, work_area_inset_bottom);
}

static inline void
zwp_remote_shell_v1_send_activated(struct wl_resource *resource_, struct wl_resource *gained_active, struct wl_resource *lost_active)
{
	wl_resource_post_event(resource_, ZWP_REMOTE_SHELL_V1_ACTIVATED, gained_active, lost_active);
}

/**
 * zwp_remote_surface_v1 - A desktop window
 * @destroy: Destroy the remote_surface
 * @set_app_id: set application ID
 * @set_window_geometry: set the new window geometry
 * @set_scale: set scale
 *
 * An interface that may be implemented by a wl_surface, for
 * implementations that provide a desktop-style user interface and allows
 * for remotely managed windows.
 *
 * It provides requests to treat surfaces like windows, allowing to set
 * properties like app id and geometry.
 *
 * The client must call wl_surface.commit on the corresponding wl_surface
 * for the remote_surface state to take effect.
 *
 * For a surface to be mapped by the compositor the client must have
 * committed both an remote_surface state and a buffer.
 */
struct zwp_remote_surface_v1_interface {
	/**
	 * destroy - Destroy the remote_surface
	 *
	 * Unmap and destroy the window. The window will be effectively
	 * hidden from the user's point of view, and all state will be
	 * lost.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * set_app_id - set application ID
	 * @app_id: (none)
	 *
	 * Set an application identifier for the surface.
	 */
	void (*set_app_id)(struct wl_client *client,
			   struct wl_resource *resource,
			   const char *app_id);
	/**
	 * set_window_geometry - set the new window geometry
	 * @x: (none)
	 * @y: (none)
	 * @width: (none)
	 * @height: (none)
	 *
	 * The window geometry of a window is its "visible bounds" from
	 * the user's perspective. Client-side decorations often have
	 * invisible portions like drop-shadows which should be ignored for
	 * the purposes of aligning, placing and constraining windows.
	 *
	 * The window geometry is double buffered, and will be applied at
	 * the time wl_surface.commit of the corresponding wl_surface is
	 * called.
	 *
	 * Once the window geometry of the surface is set once, it is not
	 * possible to unset it, and it will remain the same until
	 * set_window_geometry is called again, even if a new subsurface or
	 * buffer is attached.
	 *
	 * If never set, the value is the full bounds of the output. This
	 * updates dynamically on every commit.
	 *
	 * The arguments are given in the output coordinate space.
	 *
	 * The width and height must be greater than zero.
	 */
	void (*set_window_geometry)(struct wl_client *client,
				    struct wl_resource *resource,
				    int32_t x,
				    int32_t y,
				    int32_t width,
				    int32_t height);
	/**
	 * set_scale - set scale
	 * @scale: (none)
	 *
	 * Set a scale factor that will be applied to surface and all
	 * descendants.
	 */
	void (*set_scale)(struct wl_client *client,
			  struct wl_resource *resource,
			  wl_fixed_t scale);
};

#define ZWP_REMOTE_SURFACE_V1_SET_FULLSCREEN	0
#define ZWP_REMOTE_SURFACE_V1_UNSET_FULLSCREEN	1
#define ZWP_REMOTE_SURFACE_V1_CLOSE	2

static inline void
zwp_remote_surface_v1_send_set_fullscreen(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, ZWP_REMOTE_SURFACE_V1_SET_FULLSCREEN);
}

static inline void
zwp_remote_surface_v1_send_unset_fullscreen(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, ZWP_REMOTE_SURFACE_V1_UNSET_FULLSCREEN);
}

static inline void
zwp_remote_surface_v1_send_close(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, ZWP_REMOTE_SURFACE_V1_CLOSE);
}

#ifdef  __cplusplus
}
#endif

#endif

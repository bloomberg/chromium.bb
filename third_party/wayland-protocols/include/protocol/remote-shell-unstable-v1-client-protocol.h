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

#ifndef REMOTE_SHELL_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define REMOTE_SHELL_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

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
 * @configure: suggests a re-layout of remote shell
 * @activated: activated surface changed
 *
 * The global interface that allows clients to turn a wl_surface into a
 * "real window" which is remotely managed but can be stacked, activated
 * and made fullscreen by the user.
 */
struct zwp_remote_shell_v1_listener {
	/**
	 * configure - suggests a re-layout of remote shell
	 * @width: (none)
	 * @height: (none)
	 * @work_area_inset_left: (none)
	 * @work_area_inset_top: (none)
	 * @work_area_inset_right: (none)
	 * @work_area_inset_bottom: (none)
	 *
	 * Suggests a re-layout of remote surface geometry.
	 */
	void (*configure)(void *data,
			  struct zwp_remote_shell_v1 *zwp_remote_shell_v1,
			  int32_t width,
			  int32_t height,
			  int32_t work_area_inset_left,
			  int32_t work_area_inset_top,
			  int32_t work_area_inset_right,
			  int32_t work_area_inset_bottom);
	/**
	 * activated - activated surface changed
	 * @gained_active: (none)
	 * @lost_active: (none)
	 *
	 * Notifies client that the activated surface changed.
	 */
	void (*activated)(void *data,
			  struct zwp_remote_shell_v1 *zwp_remote_shell_v1,
			  struct wl_surface *gained_active,
			  struct wl_surface *lost_active);
};

static inline int
zwp_remote_shell_v1_add_listener(struct zwp_remote_shell_v1 *zwp_remote_shell_v1,
				 const struct zwp_remote_shell_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_remote_shell_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_REMOTE_SHELL_V1_DESTROY	0
#define ZWP_REMOTE_SHELL_V1_GET_REMOTE_SURFACE	1

static inline void
zwp_remote_shell_v1_set_user_data(struct zwp_remote_shell_v1 *zwp_remote_shell_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_remote_shell_v1, user_data);
}

static inline void *
zwp_remote_shell_v1_get_user_data(struct zwp_remote_shell_v1 *zwp_remote_shell_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_remote_shell_v1);
}

static inline void
zwp_remote_shell_v1_destroy(struct zwp_remote_shell_v1 *zwp_remote_shell_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_shell_v1,
			 ZWP_REMOTE_SHELL_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_remote_shell_v1);
}

static inline struct zwp_remote_surface_v1 *
zwp_remote_shell_v1_get_remote_surface(struct zwp_remote_shell_v1 *zwp_remote_shell_v1, struct wl_surface *surface, uint32_t container)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_remote_shell_v1,
			 ZWP_REMOTE_SHELL_V1_GET_REMOTE_SURFACE, &zwp_remote_surface_v1_interface, NULL, surface, container);

	return (struct zwp_remote_surface_v1 *) id;
}

/**
 * zwp_remote_surface_v1 - A desktop window
 * @set_fullscreen: surface wants to be fullscreen
 * @unset_fullscreen: surface wants to be non-fullscreen
 * @close: surface wants to be closed
 * @set_maximized: surface wants to be maximized
 * @unset_maximized: surface wants to be restored
 * @set_minimized: surface wants to be minimized
 * @unset_minimized: surface wants to be restored
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
struct zwp_remote_surface_v1_listener {
	/**
	 * set_fullscreen - surface wants to be fullscreen
	 *
	 * The set_fullscreen event is sent by the compositor when the
	 * user wants the surface to be made fullscreen.
	 *
	 * This is only a request that the user intends to make your window
	 * fullscreen. The client may choose to ignore this request.
	 */
	void (*set_fullscreen)(void *data,
			       struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
	/**
	 * unset_fullscreen - surface wants to be non-fullscreen
	 *
	 * The unset_fullscreen event is sent by the compositor when the
	 * user wants the surface to be made non-fullscreen.
	 *
	 * This is only a request that the user intends to make your window
	 * non-fullscreen. The client may choose to ignore this request.
	 */
	void (*unset_fullscreen)(void *data,
				 struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
	/**
	 * close - surface wants to be closed
	 *
	 * The close event is sent by the compositor when the user wants
	 * the surface to be closed. This should be equivalent to the user
	 * clicking the close button in client-side decorations, if your
	 * application has any...
	 *
	 * This is only a request that the user intends to close your
	 * window. The client may choose to ignore this request, or show a
	 * dialog to ask the user to save their data...
	 */
	void (*close)(void *data,
		      struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
	/**
	 * set_maximized - surface wants to be maximized
	 *
	 * The set_maximized event is sent by the compositor when the
	 * user wants the surface to be maximized.
	 *
	 * This is only a request that the user intends to maximized the
	 * window. The client may choose to ignore this request.
	 * @since: 2
	 */
	void (*set_maximized)(void *data,
			      struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
	/**
	 * unset_maximized - surface wants to be restored
	 *
	 * The unset_maximized event is sent by the compositor when the
	 * user wants the surface to be made visible.
	 *
	 * This is only a request that the user intends to make your window
	 * visible. The client may choose to ignore this request.
	 * @since: 2
	 */
	void (*unset_maximized)(void *data,
				struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
	/**
	 * set_minimized - surface wants to be minimized
	 *
	 * The set_minimized event is sent by the compositor when the
	 * user wants the surface to be minimized.
	 *
	 * This is only a request that the user intends to minimize the
	 * window. The client may choose to ignore this request.
	 * @since: 2
	 */
	void (*set_minimized)(void *data,
			      struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
	/**
	 * unset_minimized - surface wants to be restored
	 *
	 * The unset_minimized event is sent by the compositor when the
	 * user wants the surface to be made visible.
	 *
	 * This is only a request that the user intends to make your window
	 * visible. The client may choose to ignore this request.
	 * @since: 2
	 */
	void (*unset_minimized)(void *data,
				struct zwp_remote_surface_v1 *zwp_remote_surface_v1);
};

static inline int
zwp_remote_surface_v1_add_listener(struct zwp_remote_surface_v1 *zwp_remote_surface_v1,
				   const struct zwp_remote_surface_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_remote_surface_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_REMOTE_SURFACE_V1_DESTROY	0
#define ZWP_REMOTE_SURFACE_V1_SET_APP_ID	1
#define ZWP_REMOTE_SURFACE_V1_SET_WINDOW_GEOMETRY	2
#define ZWP_REMOTE_SURFACE_V1_SET_SCALE	3
#define ZWP_REMOTE_SURFACE_V1_FULLSCREEN	4
#define ZWP_REMOTE_SURFACE_V1_MAXIMIZE	5
#define ZWP_REMOTE_SURFACE_V1_MINIMIZE	6
#define ZWP_REMOTE_SURFACE_V1_RESTORE	7

static inline void
zwp_remote_surface_v1_set_user_data(struct zwp_remote_surface_v1 *zwp_remote_surface_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_remote_surface_v1, user_data);
}

static inline void *
zwp_remote_surface_v1_get_user_data(struct zwp_remote_surface_v1 *zwp_remote_surface_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_remote_surface_v1);
}

static inline void
zwp_remote_surface_v1_destroy(struct zwp_remote_surface_v1 *zwp_remote_surface_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_remote_surface_v1);
}

static inline void
zwp_remote_surface_v1_set_app_id(struct zwp_remote_surface_v1 *zwp_remote_surface_v1, const char *app_id)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_SET_APP_ID, app_id);
}

static inline void
zwp_remote_surface_v1_set_window_geometry(struct zwp_remote_surface_v1 *zwp_remote_surface_v1, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_SET_WINDOW_GEOMETRY, x, y, width, height);
}

static inline void
zwp_remote_surface_v1_set_scale(struct zwp_remote_surface_v1 *zwp_remote_surface_v1, wl_fixed_t scale)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_SET_SCALE, scale);
}

static inline void
zwp_remote_surface_v1_fullscreen(struct zwp_remote_surface_v1 *zwp_remote_surface_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_FULLSCREEN);
}

static inline void
zwp_remote_surface_v1_maximize(struct zwp_remote_surface_v1 *zwp_remote_surface_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_MAXIMIZE);
}

static inline void
zwp_remote_surface_v1_minimize(struct zwp_remote_surface_v1 *zwp_remote_surface_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_MINIMIZE);
}

static inline void
zwp_remote_surface_v1_restore(struct zwp_remote_surface_v1 *zwp_remote_surface_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_remote_surface_v1,
			 ZWP_REMOTE_SURFACE_V1_RESTORE);
}

#ifdef  __cplusplus
}
#endif

#endif

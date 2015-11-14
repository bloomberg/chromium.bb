/* 
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2012-2013 Collabora, Ltd.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WAYLAND_CLIENT_PROTOCOL_H
#define WAYLAND_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_buffer;
struct wl_callback;
struct wl_compositor;
struct wl_data_device;
struct wl_data_device_manager;
struct wl_data_offer;
struct wl_data_source;
struct wl_display;
struct wl_keyboard;
struct wl_output;
struct wl_pointer;
struct wl_region;
struct wl_registry;
struct wl_seat;
struct wl_shell;
struct wl_shell_surface;
struct wl_shm;
struct wl_shm_pool;
struct wl_subcompositor;
struct wl_subsurface;
struct wl_surface;
struct wl_touch;

extern const struct wl_interface wl_display_interface;
extern const struct wl_interface wl_registry_interface;
extern const struct wl_interface wl_callback_interface;
extern const struct wl_interface wl_compositor_interface;
extern const struct wl_interface wl_shm_pool_interface;
extern const struct wl_interface wl_shm_interface;
extern const struct wl_interface wl_buffer_interface;
extern const struct wl_interface wl_data_offer_interface;
extern const struct wl_interface wl_data_source_interface;
extern const struct wl_interface wl_data_device_interface;
extern const struct wl_interface wl_data_device_manager_interface;
extern const struct wl_interface wl_shell_interface;
extern const struct wl_interface wl_shell_surface_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_pointer_interface;
extern const struct wl_interface wl_keyboard_interface;
extern const struct wl_interface wl_touch_interface;
extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_region_interface;
extern const struct wl_interface wl_subcompositor_interface;
extern const struct wl_interface wl_subsurface_interface;

#ifndef WL_DISPLAY_ERROR_ENUM
#define WL_DISPLAY_ERROR_ENUM
/**
 * wl_display_error - global error values
 * @WL_DISPLAY_ERROR_INVALID_OBJECT: server couldn't find object
 * @WL_DISPLAY_ERROR_INVALID_METHOD: method doesn't exist on the
 *	specified interface
 * @WL_DISPLAY_ERROR_NO_MEMORY: server is out of memory
 *
 * These errors are global and can be emitted in response to any server
 * request.
 */
enum wl_display_error {
	WL_DISPLAY_ERROR_INVALID_OBJECT = 0,
	WL_DISPLAY_ERROR_INVALID_METHOD = 1,
	WL_DISPLAY_ERROR_NO_MEMORY = 2,
};
#endif /* WL_DISPLAY_ERROR_ENUM */

/**
 * wl_display - core global object
 * @error: fatal error event
 * @delete_id: acknowledge object ID deletion
 *
 * The core global object. This is a special singleton object. It is used
 * for internal Wayland protocol features.
 */
struct wl_display_listener {
	/**
	 * error - fatal error event
	 * @object_id: (none)
	 * @code: (none)
	 * @message: (none)
	 *
	 * The error event is sent out when a fatal (non-recoverable)
	 * error has occurred. The object_id argument is the object where
	 * the error occurred, most often in response to a request to that
	 * object. The code identifies the error and is defined by the
	 * object interface. As such, each interface defines its own set of
	 * error codes. The message is an brief description of the error,
	 * for (debugging) convenience.
	 */
	void (*error)(void *data,
		      struct wl_display *wl_display,
		      void *object_id,
		      uint32_t code,
		      const char *message);
	/**
	 * delete_id - acknowledge object ID deletion
	 * @id: (none)
	 *
	 * This event is used internally by the object ID management
	 * logic. When a client deletes an object, the server will send
	 * this event to acknowledge that it has seen the delete request.
	 * When the client receive this event, it will know that it can
	 * safely reuse the object ID.
	 */
	void (*delete_id)(void *data,
			  struct wl_display *wl_display,
			  uint32_t id);
};

static inline int
wl_display_add_listener(struct wl_display *wl_display,
			const struct wl_display_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_display,
				     (void (**)(void)) listener, data);
}

#define WL_DISPLAY_SYNC	0
#define WL_DISPLAY_GET_REGISTRY	1

static inline void
wl_display_set_user_data(struct wl_display *wl_display, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_display, user_data);
}

static inline void *
wl_display_get_user_data(struct wl_display *wl_display)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_display);
}

static inline struct wl_callback *
wl_display_sync(struct wl_display *wl_display)
{
	struct wl_proxy *callback;

	callback = wl_proxy_marshal_constructor((struct wl_proxy *) wl_display,
			 WL_DISPLAY_SYNC, &wl_callback_interface, NULL);

	return (struct wl_callback *) callback;
}

static inline struct wl_registry *
wl_display_get_registry(struct wl_display *wl_display)
{
	struct wl_proxy *registry;

	registry = wl_proxy_marshal_constructor((struct wl_proxy *) wl_display,
			 WL_DISPLAY_GET_REGISTRY, &wl_registry_interface, NULL);

	return (struct wl_registry *) registry;
}

/**
 * wl_registry - global registry object
 * @global: announce global object
 * @global_remove: announce removal of global object
 *
 * The global registry object. The server has a number of global objects
 * that are available to all clients. These objects typically represent an
 * actual object in the server (for example, an input device) or they are
 * singleton objects that provide extension functionality.
 *
 * When a client creates a registry object, the registry object will emit a
 * global event for each global currently in the registry. Globals come and
 * go as a result of device or monitor hotplugs, reconfiguration or other
 * events, and the registry will send out global and global_remove events
 * to keep the client up to date with the changes. To mark the end of the
 * initial burst of events, the client can use the wl_display.sync request
 * immediately after calling wl_display.get_registry.
 *
 * A client can bind to a global object by using the bind request. This
 * creates a client-side handle that lets the object emit events to the
 * client and lets the client invoke requests on the object.
 */
struct wl_registry_listener {
	/**
	 * global - announce global object
	 * @name: (none)
	 * @interface: (none)
	 * @version: (none)
	 *
	 * Notify the client of global objects.
	 *
	 * The event notifies the client that a global object with the
	 * given name is now available, and it implements the given version
	 * of the given interface.
	 */
	void (*global)(void *data,
		       struct wl_registry *wl_registry,
		       uint32_t name,
		       const char *interface,
		       uint32_t version);
	/**
	 * global_remove - announce removal of global object
	 * @name: (none)
	 *
	 * Notify the client of removed global objects.
	 *
	 * This event notifies the client that the global identified by
	 * name is no longer available. If the client bound to the global
	 * using the bind request, the client should now destroy that
	 * object.
	 *
	 * The object remains valid and requests to the object will be
	 * ignored until the client destroys it, to avoid races between the
	 * global going away and a client sending a request to it.
	 */
	void (*global_remove)(void *data,
			      struct wl_registry *wl_registry,
			      uint32_t name);
};

static inline int
wl_registry_add_listener(struct wl_registry *wl_registry,
			 const struct wl_registry_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_registry,
				     (void (**)(void)) listener, data);
}

#define WL_REGISTRY_BIND	0

static inline void
wl_registry_set_user_data(struct wl_registry *wl_registry, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_registry, user_data);
}

static inline void *
wl_registry_get_user_data(struct wl_registry *wl_registry)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_registry);
}

static inline void
wl_registry_destroy(struct wl_registry *wl_registry)
{
	wl_proxy_destroy((struct wl_proxy *) wl_registry);
}

static inline void *
wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_registry,
			 WL_REGISTRY_BIND, interface, name, interface->name, version, NULL);

	return (void *) id;
}

/**
 * wl_callback - callback object
 * @done: done event
 *
 * Clients can handle the 'done' event to get notified when the related
 * request is done.
 */
struct wl_callback_listener {
	/**
	 * done - done event
	 * @callback_data: request-specific data for the wl_callback
	 *
	 * Notify the client when the related request is done.
	 */
	void (*done)(void *data,
		     struct wl_callback *wl_callback,
		     uint32_t callback_data);
};

static inline int
wl_callback_add_listener(struct wl_callback *wl_callback,
			 const struct wl_callback_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_callback,
				     (void (**)(void)) listener, data);
}

static inline void
wl_callback_set_user_data(struct wl_callback *wl_callback, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_callback, user_data);
}

static inline void *
wl_callback_get_user_data(struct wl_callback *wl_callback)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_callback);
}

static inline void
wl_callback_destroy(struct wl_callback *wl_callback)
{
	wl_proxy_destroy((struct wl_proxy *) wl_callback);
}

#define WL_COMPOSITOR_CREATE_SURFACE	0
#define WL_COMPOSITOR_CREATE_REGION	1

static inline void
wl_compositor_set_user_data(struct wl_compositor *wl_compositor, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_compositor, user_data);
}

static inline void *
wl_compositor_get_user_data(struct wl_compositor *wl_compositor)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_compositor);
}

static inline void
wl_compositor_destroy(struct wl_compositor *wl_compositor)
{
	wl_proxy_destroy((struct wl_proxy *) wl_compositor);
}

static inline struct wl_surface *
wl_compositor_create_surface(struct wl_compositor *wl_compositor)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_compositor,
			 WL_COMPOSITOR_CREATE_SURFACE, &wl_surface_interface, NULL);

	return (struct wl_surface *) id;
}

static inline struct wl_region *
wl_compositor_create_region(struct wl_compositor *wl_compositor)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_compositor,
			 WL_COMPOSITOR_CREATE_REGION, &wl_region_interface, NULL);

	return (struct wl_region *) id;
}

#define WL_SHM_POOL_CREATE_BUFFER	0
#define WL_SHM_POOL_DESTROY	1
#define WL_SHM_POOL_RESIZE	2

static inline void
wl_shm_pool_set_user_data(struct wl_shm_pool *wl_shm_pool, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_shm_pool, user_data);
}

static inline void *
wl_shm_pool_get_user_data(struct wl_shm_pool *wl_shm_pool)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_shm_pool);
}

static inline struct wl_buffer *
wl_shm_pool_create_buffer(struct wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_shm_pool,
			 WL_SHM_POOL_CREATE_BUFFER, &wl_buffer_interface, NULL, offset, width, height, stride, format);

	return (struct wl_buffer *) id;
}

static inline void
wl_shm_pool_destroy(struct wl_shm_pool *wl_shm_pool)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shm_pool,
			 WL_SHM_POOL_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_shm_pool);
}

static inline void
wl_shm_pool_resize(struct wl_shm_pool *wl_shm_pool, int32_t size)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shm_pool,
			 WL_SHM_POOL_RESIZE, size);
}

#ifndef WL_SHM_ERROR_ENUM
#define WL_SHM_ERROR_ENUM
/**
 * wl_shm_error - wl_shm error values
 * @WL_SHM_ERROR_INVALID_FORMAT: buffer format is not known
 * @WL_SHM_ERROR_INVALID_STRIDE: invalid size or stride during pool or
 *	buffer creation
 * @WL_SHM_ERROR_INVALID_FD: mmapping the file descriptor failed
 *
 * These errors can be emitted in response to wl_shm requests.
 */
enum wl_shm_error {
	WL_SHM_ERROR_INVALID_FORMAT = 0,
	WL_SHM_ERROR_INVALID_STRIDE = 1,
	WL_SHM_ERROR_INVALID_FD = 2,
};
#endif /* WL_SHM_ERROR_ENUM */

#ifndef WL_SHM_FORMAT_ENUM
#define WL_SHM_FORMAT_ENUM
/**
 * wl_shm_format - pixel formats
 * @WL_SHM_FORMAT_ARGB8888: 32-bit ARGB format
 * @WL_SHM_FORMAT_XRGB8888: 32-bit RGB format
 * @WL_SHM_FORMAT_C8: (none)
 * @WL_SHM_FORMAT_RGB332: (none)
 * @WL_SHM_FORMAT_BGR233: (none)
 * @WL_SHM_FORMAT_XRGB4444: (none)
 * @WL_SHM_FORMAT_XBGR4444: (none)
 * @WL_SHM_FORMAT_RGBX4444: (none)
 * @WL_SHM_FORMAT_BGRX4444: (none)
 * @WL_SHM_FORMAT_ARGB4444: (none)
 * @WL_SHM_FORMAT_ABGR4444: (none)
 * @WL_SHM_FORMAT_RGBA4444: (none)
 * @WL_SHM_FORMAT_BGRA4444: (none)
 * @WL_SHM_FORMAT_XRGB1555: (none)
 * @WL_SHM_FORMAT_XBGR1555: (none)
 * @WL_SHM_FORMAT_RGBX5551: (none)
 * @WL_SHM_FORMAT_BGRX5551: (none)
 * @WL_SHM_FORMAT_ARGB1555: (none)
 * @WL_SHM_FORMAT_ABGR1555: (none)
 * @WL_SHM_FORMAT_RGBA5551: (none)
 * @WL_SHM_FORMAT_BGRA5551: (none)
 * @WL_SHM_FORMAT_RGB565: (none)
 * @WL_SHM_FORMAT_BGR565: (none)
 * @WL_SHM_FORMAT_RGB888: (none)
 * @WL_SHM_FORMAT_BGR888: (none)
 * @WL_SHM_FORMAT_XBGR8888: (none)
 * @WL_SHM_FORMAT_RGBX8888: (none)
 * @WL_SHM_FORMAT_BGRX8888: (none)
 * @WL_SHM_FORMAT_ABGR8888: (none)
 * @WL_SHM_FORMAT_RGBA8888: (none)
 * @WL_SHM_FORMAT_BGRA8888: (none)
 * @WL_SHM_FORMAT_XRGB2101010: (none)
 * @WL_SHM_FORMAT_XBGR2101010: (none)
 * @WL_SHM_FORMAT_RGBX1010102: (none)
 * @WL_SHM_FORMAT_BGRX1010102: (none)
 * @WL_SHM_FORMAT_ARGB2101010: (none)
 * @WL_SHM_FORMAT_ABGR2101010: (none)
 * @WL_SHM_FORMAT_RGBA1010102: (none)
 * @WL_SHM_FORMAT_BGRA1010102: (none)
 * @WL_SHM_FORMAT_YUYV: (none)
 * @WL_SHM_FORMAT_YVYU: (none)
 * @WL_SHM_FORMAT_UYVY: (none)
 * @WL_SHM_FORMAT_VYUY: (none)
 * @WL_SHM_FORMAT_AYUV: (none)
 * @WL_SHM_FORMAT_NV12: (none)
 * @WL_SHM_FORMAT_NV21: (none)
 * @WL_SHM_FORMAT_NV16: (none)
 * @WL_SHM_FORMAT_NV61: (none)
 * @WL_SHM_FORMAT_YUV410: (none)
 * @WL_SHM_FORMAT_YVU410: (none)
 * @WL_SHM_FORMAT_YUV411: (none)
 * @WL_SHM_FORMAT_YVU411: (none)
 * @WL_SHM_FORMAT_YUV420: (none)
 * @WL_SHM_FORMAT_YVU420: (none)
 * @WL_SHM_FORMAT_YUV422: (none)
 * @WL_SHM_FORMAT_YVU422: (none)
 * @WL_SHM_FORMAT_YUV444: (none)
 * @WL_SHM_FORMAT_YVU444: (none)
 *
 * This describes the memory layout of an individual pixel.
 *
 * All renderers should support argb8888 and xrgb8888 but any other formats
 * are optional and may not be supported by the particular renderer in use.
 */
enum wl_shm_format {
	WL_SHM_FORMAT_ARGB8888 = 0,
	WL_SHM_FORMAT_XRGB8888 = 1,
	WL_SHM_FORMAT_C8 = 0x20203843,
	WL_SHM_FORMAT_RGB332 = 0x38424752,
	WL_SHM_FORMAT_BGR233 = 0x38524742,
	WL_SHM_FORMAT_XRGB4444 = 0x32315258,
	WL_SHM_FORMAT_XBGR4444 = 0x32314258,
	WL_SHM_FORMAT_RGBX4444 = 0x32315852,
	WL_SHM_FORMAT_BGRX4444 = 0x32315842,
	WL_SHM_FORMAT_ARGB4444 = 0x32315241,
	WL_SHM_FORMAT_ABGR4444 = 0x32314241,
	WL_SHM_FORMAT_RGBA4444 = 0x32314152,
	WL_SHM_FORMAT_BGRA4444 = 0x32314142,
	WL_SHM_FORMAT_XRGB1555 = 0x35315258,
	WL_SHM_FORMAT_XBGR1555 = 0x35314258,
	WL_SHM_FORMAT_RGBX5551 = 0x35315852,
	WL_SHM_FORMAT_BGRX5551 = 0x35315842,
	WL_SHM_FORMAT_ARGB1555 = 0x35315241,
	WL_SHM_FORMAT_ABGR1555 = 0x35314241,
	WL_SHM_FORMAT_RGBA5551 = 0x35314152,
	WL_SHM_FORMAT_BGRA5551 = 0x35314142,
	WL_SHM_FORMAT_RGB565 = 0x36314752,
	WL_SHM_FORMAT_BGR565 = 0x36314742,
	WL_SHM_FORMAT_RGB888 = 0x34324752,
	WL_SHM_FORMAT_BGR888 = 0x34324742,
	WL_SHM_FORMAT_XBGR8888 = 0x34324258,
	WL_SHM_FORMAT_RGBX8888 = 0x34325852,
	WL_SHM_FORMAT_BGRX8888 = 0x34325842,
	WL_SHM_FORMAT_ABGR8888 = 0x34324241,
	WL_SHM_FORMAT_RGBA8888 = 0x34324152,
	WL_SHM_FORMAT_BGRA8888 = 0x34324142,
	WL_SHM_FORMAT_XRGB2101010 = 0x30335258,
	WL_SHM_FORMAT_XBGR2101010 = 0x30334258,
	WL_SHM_FORMAT_RGBX1010102 = 0x30335852,
	WL_SHM_FORMAT_BGRX1010102 = 0x30335842,
	WL_SHM_FORMAT_ARGB2101010 = 0x30335241,
	WL_SHM_FORMAT_ABGR2101010 = 0x30334241,
	WL_SHM_FORMAT_RGBA1010102 = 0x30334152,
	WL_SHM_FORMAT_BGRA1010102 = 0x30334142,
	WL_SHM_FORMAT_YUYV = 0x56595559,
	WL_SHM_FORMAT_YVYU = 0x55595659,
	WL_SHM_FORMAT_UYVY = 0x59565955,
	WL_SHM_FORMAT_VYUY = 0x59555956,
	WL_SHM_FORMAT_AYUV = 0x56555941,
	WL_SHM_FORMAT_NV12 = 0x3231564e,
	WL_SHM_FORMAT_NV21 = 0x3132564e,
	WL_SHM_FORMAT_NV16 = 0x3631564e,
	WL_SHM_FORMAT_NV61 = 0x3136564e,
	WL_SHM_FORMAT_YUV410 = 0x39565559,
	WL_SHM_FORMAT_YVU410 = 0x39555659,
	WL_SHM_FORMAT_YUV411 = 0x31315559,
	WL_SHM_FORMAT_YVU411 = 0x31315659,
	WL_SHM_FORMAT_YUV420 = 0x32315559,
	WL_SHM_FORMAT_YVU420 = 0x32315659,
	WL_SHM_FORMAT_YUV422 = 0x36315559,
	WL_SHM_FORMAT_YVU422 = 0x36315659,
	WL_SHM_FORMAT_YUV444 = 0x34325559,
	WL_SHM_FORMAT_YVU444 = 0x34325659,
};
#endif /* WL_SHM_FORMAT_ENUM */

/**
 * wl_shm - shared memory support
 * @format: pixel format description
 *
 * A global singleton object that provides support for shared memory.
 *
 * Clients can create wl_shm_pool objects using the create_pool request.
 *
 * At connection setup time, the wl_shm object emits one or more format
 * events to inform clients about the valid pixel formats that can be used
 * for buffers.
 */
struct wl_shm_listener {
	/**
	 * format - pixel format description
	 * @format: (none)
	 *
	 * Informs the client about a valid pixel format that can be used
	 * for buffers. Known formats include argb8888 and xrgb8888.
	 */
	void (*format)(void *data,
		       struct wl_shm *wl_shm,
		       uint32_t format);
};

static inline int
wl_shm_add_listener(struct wl_shm *wl_shm,
		    const struct wl_shm_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_shm,
				     (void (**)(void)) listener, data);
}

#define WL_SHM_CREATE_POOL	0

static inline void
wl_shm_set_user_data(struct wl_shm *wl_shm, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_shm, user_data);
}

static inline void *
wl_shm_get_user_data(struct wl_shm *wl_shm)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_shm);
}

static inline void
wl_shm_destroy(struct wl_shm *wl_shm)
{
	wl_proxy_destroy((struct wl_proxy *) wl_shm);
}

static inline struct wl_shm_pool *
wl_shm_create_pool(struct wl_shm *wl_shm, int32_t fd, int32_t size)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_shm,
			 WL_SHM_CREATE_POOL, &wl_shm_pool_interface, NULL, fd, size);

	return (struct wl_shm_pool *) id;
}

/**
 * wl_buffer - content for a wl_surface
 * @release: compositor releases buffer
 *
 * A buffer provides the content for a wl_surface. Buffers are created
 * through factory interfaces such as wl_drm, wl_shm or similar. It has a
 * width and a height and can be attached to a wl_surface, but the
 * mechanism by which a client provides and updates the contents is defined
 * by the buffer factory interface.
 */
struct wl_buffer_listener {
	/**
	 * release - compositor releases buffer
	 *
	 * Sent when this wl_buffer is no longer used by the compositor.
	 * The client is now free to re-use or destroy this buffer and its
	 * backing storage.
	 *
	 * If a client receives a release event before the frame callback
	 * requested in the same wl_surface.commit that attaches this
	 * wl_buffer to a surface, then the client is immediately free to
	 * re-use the buffer and its backing storage, and does not need a
	 * second buffer for the next surface content update. Typically
	 * this is possible, when the compositor maintains a copy of the
	 * wl_surface contents, e.g. as a GL texture. This is an important
	 * optimization for GL(ES) compositors with wl_shm clients.
	 */
	void (*release)(void *data,
			struct wl_buffer *wl_buffer);
};

static inline int
wl_buffer_add_listener(struct wl_buffer *wl_buffer,
		       const struct wl_buffer_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_buffer,
				     (void (**)(void)) listener, data);
}

#define WL_BUFFER_DESTROY	0

static inline void
wl_buffer_set_user_data(struct wl_buffer *wl_buffer, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_buffer, user_data);
}

static inline void *
wl_buffer_get_user_data(struct wl_buffer *wl_buffer)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_buffer);
}

static inline void
wl_buffer_destroy(struct wl_buffer *wl_buffer)
{
	wl_proxy_marshal((struct wl_proxy *) wl_buffer,
			 WL_BUFFER_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_buffer);
}

/**
 * wl_data_offer - offer to transfer data
 * @offer: advertise offered mime type
 *
 * A wl_data_offer represents a piece of data offered for transfer by
 * another client (the source client). It is used by the copy-and-paste and
 * drag-and-drop mechanisms. The offer describes the different mime types
 * that the data can be converted to and provides the mechanism for
 * transferring the data directly from the source client.
 */
struct wl_data_offer_listener {
	/**
	 * offer - advertise offered mime type
	 * @mime_type: (none)
	 *
	 * Sent immediately after creating the wl_data_offer object. One
	 * event per offered mime type.
	 */
	void (*offer)(void *data,
		      struct wl_data_offer *wl_data_offer,
		      const char *mime_type);
};

static inline int
wl_data_offer_add_listener(struct wl_data_offer *wl_data_offer,
			   const struct wl_data_offer_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_data_offer,
				     (void (**)(void)) listener, data);
}

#define WL_DATA_OFFER_ACCEPT	0
#define WL_DATA_OFFER_RECEIVE	1
#define WL_DATA_OFFER_DESTROY	2

static inline void
wl_data_offer_set_user_data(struct wl_data_offer *wl_data_offer, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_data_offer, user_data);
}

static inline void *
wl_data_offer_get_user_data(struct wl_data_offer *wl_data_offer)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_data_offer);
}

static inline void
wl_data_offer_accept(struct wl_data_offer *wl_data_offer, uint32_t serial, const char *mime_type)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_offer,
			 WL_DATA_OFFER_ACCEPT, serial, mime_type);
}

static inline void
wl_data_offer_receive(struct wl_data_offer *wl_data_offer, const char *mime_type, int32_t fd)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_offer,
			 WL_DATA_OFFER_RECEIVE, mime_type, fd);
}

static inline void
wl_data_offer_destroy(struct wl_data_offer *wl_data_offer)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_offer,
			 WL_DATA_OFFER_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_data_offer);
}

/**
 * wl_data_source - offer to transfer data
 * @target: a target accepts an offered mime type
 * @send: send the data
 * @cancelled: selection was cancelled
 *
 * The wl_data_source object is the source side of a wl_data_offer. It is
 * created by the source client in a data transfer and provides a way to
 * describe the offered data and a way to respond to requests to transfer
 * the data.
 */
struct wl_data_source_listener {
	/**
	 * target - a target accepts an offered mime type
	 * @mime_type: (none)
	 *
	 * Sent when a target accepts pointer_focus or motion events. If
	 * a target does not accept any of the offered types, type is NULL.
	 *
	 * Used for feedback during drag-and-drop.
	 */
	void (*target)(void *data,
		       struct wl_data_source *wl_data_source,
		       const char *mime_type);
	/**
	 * send - send the data
	 * @mime_type: (none)
	 * @fd: (none)
	 *
	 * Request for data from the client. Send the data as the
	 * specified mime type over the passed file descriptor, then close
	 * it.
	 */
	void (*send)(void *data,
		     struct wl_data_source *wl_data_source,
		     const char *mime_type,
		     int32_t fd);
	/**
	 * cancelled - selection was cancelled
	 *
	 * This data source has been replaced by another data source. The
	 * client should clean up and destroy this data source.
	 */
	void (*cancelled)(void *data,
			  struct wl_data_source *wl_data_source);
};

static inline int
wl_data_source_add_listener(struct wl_data_source *wl_data_source,
			    const struct wl_data_source_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_data_source,
				     (void (**)(void)) listener, data);
}

#define WL_DATA_SOURCE_OFFER	0
#define WL_DATA_SOURCE_DESTROY	1

static inline void
wl_data_source_set_user_data(struct wl_data_source *wl_data_source, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_data_source, user_data);
}

static inline void *
wl_data_source_get_user_data(struct wl_data_source *wl_data_source)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_data_source);
}

static inline void
wl_data_source_offer(struct wl_data_source *wl_data_source, const char *mime_type)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_source,
			 WL_DATA_SOURCE_OFFER, mime_type);
}

static inline void
wl_data_source_destroy(struct wl_data_source *wl_data_source)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_source,
			 WL_DATA_SOURCE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_data_source);
}

#ifndef WL_DATA_DEVICE_ERROR_ENUM
#define WL_DATA_DEVICE_ERROR_ENUM
enum wl_data_device_error {
	WL_DATA_DEVICE_ERROR_ROLE = 0,
};
#endif /* WL_DATA_DEVICE_ERROR_ENUM */

/**
 * wl_data_device - data transfer device
 * @data_offer: introduce a new wl_data_offer
 * @enter: initiate drag-and-drop session
 * @leave: end drag-and-drop session
 * @motion: drag-and-drop session motion
 * @drop: end drag-and-drag session successfully
 * @selection: advertise new selection
 *
 * There is one wl_data_device per seat which can be obtained from the
 * global wl_data_device_manager singleton.
 *
 * A wl_data_device provides access to inter-client data transfer
 * mechanisms such as copy-and-paste and drag-and-drop.
 */
struct wl_data_device_listener {
	/**
	 * data_offer - introduce a new wl_data_offer
	 * @id: (none)
	 *
	 * The data_offer event introduces a new wl_data_offer object,
	 * which will subsequently be used in either the data_device.enter
	 * event (for drag-and-drop) or the data_device.selection event
	 * (for selections). Immediately following the
	 * data_device_data_offer event, the new data_offer object will
	 * send out data_offer.offer events to describe the mime types it
	 * offers.
	 */
	void (*data_offer)(void *data,
			   struct wl_data_device *wl_data_device,
			   struct wl_data_offer *id);
	/**
	 * enter - initiate drag-and-drop session
	 * @serial: (none)
	 * @surface: (none)
	 * @x: (none)
	 * @y: (none)
	 * @id: (none)
	 *
	 * This event is sent when an active drag-and-drop pointer enters
	 * a surface owned by the client. The position of the pointer at
	 * enter time is provided by the x and y arguments, in surface
	 * local coordinates.
	 */
	void (*enter)(void *data,
		      struct wl_data_device *wl_data_device,
		      uint32_t serial,
		      struct wl_surface *surface,
		      wl_fixed_t x,
		      wl_fixed_t y,
		      struct wl_data_offer *id);
	/**
	 * leave - end drag-and-drop session
	 *
	 * This event is sent when the drag-and-drop pointer leaves the
	 * surface and the session ends. The client must destroy the
	 * wl_data_offer introduced at enter time at this point.
	 */
	void (*leave)(void *data,
		      struct wl_data_device *wl_data_device);
	/**
	 * motion - drag-and-drop session motion
	 * @time: timestamp with millisecond granularity
	 * @x: (none)
	 * @y: (none)
	 *
	 * This event is sent when the drag-and-drop pointer moves within
	 * the currently focused surface. The new position of the pointer
	 * is provided by the x and y arguments, in surface local
	 * coordinates.
	 */
	void (*motion)(void *data,
		       struct wl_data_device *wl_data_device,
		       uint32_t time,
		       wl_fixed_t x,
		       wl_fixed_t y);
	/**
	 * drop - end drag-and-drag session successfully
	 *
	 * The event is sent when a drag-and-drop operation is ended
	 * because the implicit grab is removed.
	 */
	void (*drop)(void *data,
		     struct wl_data_device *wl_data_device);
	/**
	 * selection - advertise new selection
	 * @id: (none)
	 *
	 * The selection event is sent out to notify the client of a new
	 * wl_data_offer for the selection for this device. The
	 * data_device.data_offer and the data_offer.offer events are sent
	 * out immediately before this event to introduce the data offer
	 * object. The selection event is sent to a client immediately
	 * before receiving keyboard focus and when a new selection is set
	 * while the client has keyboard focus. The data_offer is valid
	 * until a new data_offer or NULL is received or until the client
	 * loses keyboard focus. The client must destroy the previous
	 * selection data_offer, if any, upon receiving this event.
	 */
	void (*selection)(void *data,
			  struct wl_data_device *wl_data_device,
			  struct wl_data_offer *id);
};

static inline int
wl_data_device_add_listener(struct wl_data_device *wl_data_device,
			    const struct wl_data_device_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_data_device,
				     (void (**)(void)) listener, data);
}

#define WL_DATA_DEVICE_START_DRAG	0
#define WL_DATA_DEVICE_SET_SELECTION	1
#define WL_DATA_DEVICE_RELEASE	2

static inline void
wl_data_device_set_user_data(struct wl_data_device *wl_data_device, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_data_device, user_data);
}

static inline void *
wl_data_device_get_user_data(struct wl_data_device *wl_data_device)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_data_device);
}

static inline void
wl_data_device_destroy(struct wl_data_device *wl_data_device)
{
	wl_proxy_destroy((struct wl_proxy *) wl_data_device);
}

static inline void
wl_data_device_start_drag(struct wl_data_device *wl_data_device, struct wl_data_source *source, struct wl_surface *origin, struct wl_surface *icon, uint32_t serial)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_device,
			 WL_DATA_DEVICE_START_DRAG, source, origin, icon, serial);
}

static inline void
wl_data_device_set_selection(struct wl_data_device *wl_data_device, struct wl_data_source *source, uint32_t serial)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_device,
			 WL_DATA_DEVICE_SET_SELECTION, source, serial);
}

static inline void
wl_data_device_release(struct wl_data_device *wl_data_device)
{
	wl_proxy_marshal((struct wl_proxy *) wl_data_device,
			 WL_DATA_DEVICE_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) wl_data_device);
}

#define WL_DATA_DEVICE_MANAGER_CREATE_DATA_SOURCE	0
#define WL_DATA_DEVICE_MANAGER_GET_DATA_DEVICE	1

static inline void
wl_data_device_manager_set_user_data(struct wl_data_device_manager *wl_data_device_manager, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_data_device_manager, user_data);
}

static inline void *
wl_data_device_manager_get_user_data(struct wl_data_device_manager *wl_data_device_manager)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_data_device_manager);
}

static inline void
wl_data_device_manager_destroy(struct wl_data_device_manager *wl_data_device_manager)
{
	wl_proxy_destroy((struct wl_proxy *) wl_data_device_manager);
}

static inline struct wl_data_source *
wl_data_device_manager_create_data_source(struct wl_data_device_manager *wl_data_device_manager)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_data_device_manager,
			 WL_DATA_DEVICE_MANAGER_CREATE_DATA_SOURCE, &wl_data_source_interface, NULL);

	return (struct wl_data_source *) id;
}

static inline struct wl_data_device *
wl_data_device_manager_get_data_device(struct wl_data_device_manager *wl_data_device_manager, struct wl_seat *seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_data_device_manager,
			 WL_DATA_DEVICE_MANAGER_GET_DATA_DEVICE, &wl_data_device_interface, NULL, seat);

	return (struct wl_data_device *) id;
}

#ifndef WL_SHELL_ERROR_ENUM
#define WL_SHELL_ERROR_ENUM
enum wl_shell_error {
	WL_SHELL_ERROR_ROLE = 0,
};
#endif /* WL_SHELL_ERROR_ENUM */

#define WL_SHELL_GET_SHELL_SURFACE	0

static inline void
wl_shell_set_user_data(struct wl_shell *wl_shell, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_shell, user_data);
}

static inline void *
wl_shell_get_user_data(struct wl_shell *wl_shell)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_shell);
}

static inline void
wl_shell_destroy(struct wl_shell *wl_shell)
{
	wl_proxy_destroy((struct wl_proxy *) wl_shell);
}

static inline struct wl_shell_surface *
wl_shell_get_shell_surface(struct wl_shell *wl_shell, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_shell,
			 WL_SHELL_GET_SHELL_SURFACE, &wl_shell_surface_interface, NULL, surface);

	return (struct wl_shell_surface *) id;
}

#ifndef WL_SHELL_SURFACE_RESIZE_ENUM
#define WL_SHELL_SURFACE_RESIZE_ENUM
/**
 * wl_shell_surface_resize - edge values for resizing
 * @WL_SHELL_SURFACE_RESIZE_NONE: (none)
 * @WL_SHELL_SURFACE_RESIZE_TOP: (none)
 * @WL_SHELL_SURFACE_RESIZE_BOTTOM: (none)
 * @WL_SHELL_SURFACE_RESIZE_LEFT: (none)
 * @WL_SHELL_SURFACE_RESIZE_TOP_LEFT: (none)
 * @WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT: (none)
 * @WL_SHELL_SURFACE_RESIZE_RIGHT: (none)
 * @WL_SHELL_SURFACE_RESIZE_TOP_RIGHT: (none)
 * @WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT: (none)
 *
 * These values are used to indicate which edge of a surface is being
 * dragged in a resize operation. The server may use this information to
 * adapt its behavior, e.g. choose an appropriate cursor image.
 */
enum wl_shell_surface_resize {
	WL_SHELL_SURFACE_RESIZE_NONE = 0,
	WL_SHELL_SURFACE_RESIZE_TOP = 1,
	WL_SHELL_SURFACE_RESIZE_BOTTOM = 2,
	WL_SHELL_SURFACE_RESIZE_LEFT = 4,
	WL_SHELL_SURFACE_RESIZE_TOP_LEFT = 5,
	WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT = 6,
	WL_SHELL_SURFACE_RESIZE_RIGHT = 8,
	WL_SHELL_SURFACE_RESIZE_TOP_RIGHT = 9,
	WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT = 10,
};
#endif /* WL_SHELL_SURFACE_RESIZE_ENUM */

#ifndef WL_SHELL_SURFACE_TRANSIENT_ENUM
#define WL_SHELL_SURFACE_TRANSIENT_ENUM
/**
 * wl_shell_surface_transient - details of transient behaviour
 * @WL_SHELL_SURFACE_TRANSIENT_INACTIVE: do not set keyboard focus
 *
 * These flags specify details of the expected behaviour of transient
 * surfaces. Used in the set_transient request.
 */
enum wl_shell_surface_transient {
	WL_SHELL_SURFACE_TRANSIENT_INACTIVE = 0x1,
};
#endif /* WL_SHELL_SURFACE_TRANSIENT_ENUM */

#ifndef WL_SHELL_SURFACE_FULLSCREEN_METHOD_ENUM
#define WL_SHELL_SURFACE_FULLSCREEN_METHOD_ENUM
/**
 * wl_shell_surface_fullscreen_method - different method to set the
 *	surface fullscreen
 * @WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT: no preference, apply
 *	default policy
 * @WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE: scale, preserve the
 *	surface's aspect ratio and center on output
 * @WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER: switch output mode to the
 *	smallest mode that can fit the surface, add black borders to compensate
 *	size mismatch
 * @WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL: no upscaling, center on
 *	output and add black borders to compensate size mismatch
 *
 * Hints to indicate to the compositor how to deal with a conflict
 * between the dimensions of the surface and the dimensions of the output.
 * The compositor is free to ignore this parameter.
 */
enum wl_shell_surface_fullscreen_method {
	WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT = 0,
	WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE = 1,
	WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER = 2,
	WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL = 3,
};
#endif /* WL_SHELL_SURFACE_FULLSCREEN_METHOD_ENUM */

/**
 * wl_shell_surface - desktop-style metadata interface
 * @ping: ping client
 * @configure: suggest resize
 * @popup_done: popup interaction is done
 *
 * An interface that may be implemented by a wl_surface, for
 * implementations that provide a desktop-style user interface.
 *
 * It provides requests to treat surfaces like toplevel, fullscreen or
 * popup windows, move, resize or maximize them, associate metadata like
 * title and class, etc.
 *
 * On the server side the object is automatically destroyed when the
 * related wl_surface is destroyed. On client side,
 * wl_shell_surface_destroy() must be called before destroying the
 * wl_surface object.
 */
struct wl_shell_surface_listener {
	/**
	 * ping - ping client
	 * @serial: (none)
	 *
	 * Ping a client to check if it is receiving events and sending
	 * requests. A client is expected to reply with a pong request.
	 */
	void (*ping)(void *data,
		     struct wl_shell_surface *wl_shell_surface,
		     uint32_t serial);
	/**
	 * configure - suggest resize
	 * @edges: (none)
	 * @width: (none)
	 * @height: (none)
	 *
	 * The configure event asks the client to resize its surface.
	 *
	 * The size is a hint, in the sense that the client is free to
	 * ignore it if it doesn't resize, pick a smaller size (to satisfy
	 * aspect ratio or resize in steps of NxM pixels).
	 *
	 * The edges parameter provides a hint about how the surface was
	 * resized. The client may use this information to decide how to
	 * adjust its content to the new size (e.g. a scrolling area might
	 * adjust its content position to leave the viewable content
	 * unmoved).
	 *
	 * The client is free to dismiss all but the last configure event
	 * it received.
	 *
	 * The width and height arguments specify the size of the window in
	 * surface local coordinates.
	 */
	void (*configure)(void *data,
			  struct wl_shell_surface *wl_shell_surface,
			  uint32_t edges,
			  int32_t width,
			  int32_t height);
	/**
	 * popup_done - popup interaction is done
	 *
	 * The popup_done event is sent out when a popup grab is broken,
	 * that is, when the user clicks a surface that doesn't belong to
	 * the client owning the popup surface.
	 */
	void (*popup_done)(void *data,
			   struct wl_shell_surface *wl_shell_surface);
};

static inline int
wl_shell_surface_add_listener(struct wl_shell_surface *wl_shell_surface,
			      const struct wl_shell_surface_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_shell_surface,
				     (void (**)(void)) listener, data);
}

#define WL_SHELL_SURFACE_PONG	0
#define WL_SHELL_SURFACE_MOVE	1
#define WL_SHELL_SURFACE_RESIZE	2
#define WL_SHELL_SURFACE_SET_TOPLEVEL	3
#define WL_SHELL_SURFACE_SET_TRANSIENT	4
#define WL_SHELL_SURFACE_SET_FULLSCREEN	5
#define WL_SHELL_SURFACE_SET_POPUP	6
#define WL_SHELL_SURFACE_SET_MAXIMIZED	7
#define WL_SHELL_SURFACE_SET_TITLE	8
#define WL_SHELL_SURFACE_SET_CLASS	9

static inline void
wl_shell_surface_set_user_data(struct wl_shell_surface *wl_shell_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_shell_surface, user_data);
}

static inline void *
wl_shell_surface_get_user_data(struct wl_shell_surface *wl_shell_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_shell_surface);
}

static inline void
wl_shell_surface_destroy(struct wl_shell_surface *wl_shell_surface)
{
	wl_proxy_destroy((struct wl_proxy *) wl_shell_surface);
}

static inline void
wl_shell_surface_pong(struct wl_shell_surface *wl_shell_surface, uint32_t serial)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_PONG, serial);
}

static inline void
wl_shell_surface_move(struct wl_shell_surface *wl_shell_surface, struct wl_seat *seat, uint32_t serial)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_MOVE, seat, serial);
}

static inline void
wl_shell_surface_resize(struct wl_shell_surface *wl_shell_surface, struct wl_seat *seat, uint32_t serial, uint32_t edges)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_RESIZE, seat, serial, edges);
}

static inline void
wl_shell_surface_set_toplevel(struct wl_shell_surface *wl_shell_surface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_TOPLEVEL);
}

static inline void
wl_shell_surface_set_transient(struct wl_shell_surface *wl_shell_surface, struct wl_surface *parent, int32_t x, int32_t y, uint32_t flags)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_TRANSIENT, parent, x, y, flags);
}

static inline void
wl_shell_surface_set_fullscreen(struct wl_shell_surface *wl_shell_surface, uint32_t method, uint32_t framerate, struct wl_output *output)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_FULLSCREEN, method, framerate, output);
}

static inline void
wl_shell_surface_set_popup(struct wl_shell_surface *wl_shell_surface, struct wl_seat *seat, uint32_t serial, struct wl_surface *parent, int32_t x, int32_t y, uint32_t flags)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_POPUP, seat, serial, parent, x, y, flags);
}

static inline void
wl_shell_surface_set_maximized(struct wl_shell_surface *wl_shell_surface, struct wl_output *output)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_MAXIMIZED, output);
}

static inline void
wl_shell_surface_set_title(struct wl_shell_surface *wl_shell_surface, const char *title)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_TITLE, title);
}

static inline void
wl_shell_surface_set_class(struct wl_shell_surface *wl_shell_surface, const char *class_)
{
	wl_proxy_marshal((struct wl_proxy *) wl_shell_surface,
			 WL_SHELL_SURFACE_SET_CLASS, class_);
}

#ifndef WL_SURFACE_ERROR_ENUM
#define WL_SURFACE_ERROR_ENUM
/**
 * wl_surface_error - wl_surface error values
 * @WL_SURFACE_ERROR_INVALID_SCALE: buffer scale value is invalid
 * @WL_SURFACE_ERROR_INVALID_TRANSFORM: buffer transform value is invalid
 *
 * These errors can be emitted in response to wl_surface requests.
 */
enum wl_surface_error {
	WL_SURFACE_ERROR_INVALID_SCALE = 0,
	WL_SURFACE_ERROR_INVALID_TRANSFORM = 1,
};
#endif /* WL_SURFACE_ERROR_ENUM */

/**
 * wl_surface - an onscreen surface
 * @enter: surface enters an output
 * @leave: surface leaves an output
 *
 * A surface is a rectangular area that is displayed on the screen. It
 * has a location, size and pixel contents.
 *
 * The size of a surface (and relative positions on it) is described in
 * surface local coordinates, which may differ from the buffer local
 * coordinates of the pixel content, in case a buffer_transform or a
 * buffer_scale is used.
 *
 * A surface without a "role" is fairly useless, a compositor does not know
 * where, when or how to present it. The role is the purpose of a
 * wl_surface. Examples of roles are a cursor for a pointer (as set by
 * wl_pointer.set_cursor), a drag icon (wl_data_device.start_drag), a
 * sub-surface (wl_subcompositor.get_subsurface), and a window as defined
 * by a shell protocol (e.g. wl_shell.get_shell_surface).
 *
 * A surface can have only one role at a time. Initially a wl_surface does
 * not have a role. Once a wl_surface is given a role, it is set
 * permanently for the whole lifetime of the wl_surface object. Giving the
 * current role again is allowed, unless explicitly forbidden by the
 * relevant interface specification.
 *
 * Surface roles are given by requests in other interfaces such as
 * wl_pointer.set_cursor. The request should explicitly mention that this
 * request gives a role to a wl_surface. Often, this request also creates a
 * new protocol object that represents the role and adds additional
 * functionality to wl_surface. When a client wants to destroy a
 * wl_surface, they must destroy this 'role object' before the wl_surface.
 *
 * Destroying the role object does not remove the role from the wl_surface,
 * but it may stop the wl_surface from "playing the role". For instance, if
 * a wl_subsurface object is destroyed, the wl_surface it was created for
 * will be unmapped and forget its position and z-order. It is allowed to
 * create a wl_subsurface for the same wl_surface again, but it is not
 * allowed to use the wl_surface as a cursor (cursor is a different role
 * than sub-surface, and role switching is not allowed).
 */
struct wl_surface_listener {
	/**
	 * enter - surface enters an output
	 * @output: (none)
	 *
	 * This is emitted whenever a surface's creation, movement, or
	 * resizing results in some part of it being within the scanout
	 * region of an output.
	 *
	 * Note that a surface may be overlapping with zero or more
	 * outputs.
	 */
	void (*enter)(void *data,
		      struct wl_surface *wl_surface,
		      struct wl_output *output);
	/**
	 * leave - surface leaves an output
	 * @output: (none)
	 *
	 * This is emitted whenever a surface's creation, movement, or
	 * resizing results in it no longer having any part of it within
	 * the scanout region of an output.
	 */
	void (*leave)(void *data,
		      struct wl_surface *wl_surface,
		      struct wl_output *output);
};

static inline int
wl_surface_add_listener(struct wl_surface *wl_surface,
			const struct wl_surface_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_surface,
				     (void (**)(void)) listener, data);
}

#define WL_SURFACE_DESTROY	0
#define WL_SURFACE_ATTACH	1
#define WL_SURFACE_DAMAGE	2
#define WL_SURFACE_FRAME	3
#define WL_SURFACE_SET_OPAQUE_REGION	4
#define WL_SURFACE_SET_INPUT_REGION	5
#define WL_SURFACE_COMMIT	6
#define WL_SURFACE_SET_BUFFER_TRANSFORM	7
#define WL_SURFACE_SET_BUFFER_SCALE	8

static inline void
wl_surface_set_user_data(struct wl_surface *wl_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_surface, user_data);
}

static inline void *
wl_surface_get_user_data(struct wl_surface *wl_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_surface);
}

static inline void
wl_surface_destroy(struct wl_surface *wl_surface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_surface);
}

static inline void
wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_ATTACH, buffer, x, y);
}

static inline void
wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_DAMAGE, x, y, width, height);
}

static inline struct wl_callback *
wl_surface_frame(struct wl_surface *wl_surface)
{
	struct wl_proxy *callback;

	callback = wl_proxy_marshal_constructor((struct wl_proxy *) wl_surface,
			 WL_SURFACE_FRAME, &wl_callback_interface, NULL);

	return (struct wl_callback *) callback;
}

static inline void
wl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_OPAQUE_REGION, region);
}

static inline void
wl_surface_set_input_region(struct wl_surface *wl_surface, struct wl_region *region)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_INPUT_REGION, region);
}

static inline void
wl_surface_commit(struct wl_surface *wl_surface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_COMMIT);
}

static inline void
wl_surface_set_buffer_transform(struct wl_surface *wl_surface, int32_t transform)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_BUFFER_TRANSFORM, transform);
}

static inline void
wl_surface_set_buffer_scale(struct wl_surface *wl_surface, int32_t scale)
{
	wl_proxy_marshal((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_BUFFER_SCALE, scale);
}

#ifndef WL_SEAT_CAPABILITY_ENUM
#define WL_SEAT_CAPABILITY_ENUM
/**
 * wl_seat_capability - seat capability bitmask
 * @WL_SEAT_CAPABILITY_POINTER: The seat has pointer devices
 * @WL_SEAT_CAPABILITY_KEYBOARD: The seat has one or more keyboards
 * @WL_SEAT_CAPABILITY_TOUCH: The seat has touch devices
 *
 * This is a bitmask of capabilities this seat has; if a member is set,
 * then it is present on the seat.
 */
enum wl_seat_capability {
	WL_SEAT_CAPABILITY_POINTER = 1,
	WL_SEAT_CAPABILITY_KEYBOARD = 2,
	WL_SEAT_CAPABILITY_TOUCH = 4,
};
#endif /* WL_SEAT_CAPABILITY_ENUM */

/**
 * wl_seat - group of input devices
 * @capabilities: seat capabilities changed
 * @name: unique identifier for this seat
 *
 * A seat is a group of keyboards, pointer and touch devices. This object
 * is published as a global during start up, or when such a device is hot
 * plugged. A seat typically has a pointer and maintains a keyboard focus
 * and a pointer focus.
 */
struct wl_seat_listener {
	/**
	 * capabilities - seat capabilities changed
	 * @capabilities: (none)
	 *
	 * This is emitted whenever a seat gains or loses the pointer,
	 * keyboard or touch capabilities. The argument is a capability
	 * enum containing the complete set of capabilities this seat has.
	 */
	void (*capabilities)(void *data,
			     struct wl_seat *wl_seat,
			     uint32_t capabilities);
	/**
	 * name - unique identifier for this seat
	 * @name: (none)
	 *
	 * In a multiseat configuration this can be used by the client to
	 * help identify which physical devices the seat represents. Based
	 * on the seat configuration used by the compositor.
	 * @since: 2
	 */
	void (*name)(void *data,
		     struct wl_seat *wl_seat,
		     const char *name);
};

static inline int
wl_seat_add_listener(struct wl_seat *wl_seat,
		     const struct wl_seat_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_seat,
				     (void (**)(void)) listener, data);
}

#define WL_SEAT_GET_POINTER	0
#define WL_SEAT_GET_KEYBOARD	1
#define WL_SEAT_GET_TOUCH	2

static inline void
wl_seat_set_user_data(struct wl_seat *wl_seat, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_seat, user_data);
}

static inline void *
wl_seat_get_user_data(struct wl_seat *wl_seat)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_seat);
}

static inline void
wl_seat_destroy(struct wl_seat *wl_seat)
{
	wl_proxy_destroy((struct wl_proxy *) wl_seat);
}

static inline struct wl_pointer *
wl_seat_get_pointer(struct wl_seat *wl_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_seat,
			 WL_SEAT_GET_POINTER, &wl_pointer_interface, NULL);

	return (struct wl_pointer *) id;
}

static inline struct wl_keyboard *
wl_seat_get_keyboard(struct wl_seat *wl_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_seat,
			 WL_SEAT_GET_KEYBOARD, &wl_keyboard_interface, NULL);

	return (struct wl_keyboard *) id;
}

static inline struct wl_touch *
wl_seat_get_touch(struct wl_seat *wl_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_seat,
			 WL_SEAT_GET_TOUCH, &wl_touch_interface, NULL);

	return (struct wl_touch *) id;
}

#ifndef WL_POINTER_ERROR_ENUM
#define WL_POINTER_ERROR_ENUM
enum wl_pointer_error {
	WL_POINTER_ERROR_ROLE = 0,
};
#endif /* WL_POINTER_ERROR_ENUM */

#ifndef WL_POINTER_BUTTON_STATE_ENUM
#define WL_POINTER_BUTTON_STATE_ENUM
/**
 * wl_pointer_button_state - physical button state
 * @WL_POINTER_BUTTON_STATE_RELEASED: The button is not pressed
 * @WL_POINTER_BUTTON_STATE_PRESSED: The button is pressed
 *
 * Describes the physical state of a button which provoked the button
 * event.
 */
enum wl_pointer_button_state {
	WL_POINTER_BUTTON_STATE_RELEASED = 0,
	WL_POINTER_BUTTON_STATE_PRESSED = 1,
};
#endif /* WL_POINTER_BUTTON_STATE_ENUM */

#ifndef WL_POINTER_AXIS_ENUM
#define WL_POINTER_AXIS_ENUM
/**
 * wl_pointer_axis - axis types
 * @WL_POINTER_AXIS_VERTICAL_SCROLL: (none)
 * @WL_POINTER_AXIS_HORIZONTAL_SCROLL: (none)
 *
 * Describes the axis types of scroll events.
 */
enum wl_pointer_axis {
	WL_POINTER_AXIS_VERTICAL_SCROLL = 0,
	WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1,
};
#endif /* WL_POINTER_AXIS_ENUM */

/**
 * wl_pointer - pointer input device
 * @enter: enter event
 * @leave: leave event
 * @motion: pointer motion event
 * @button: pointer button event
 * @axis: axis event
 *
 * The wl_pointer interface represents one or more input devices, such as
 * mice, which control the pointer location and pointer_focus of a seat.
 *
 * The wl_pointer interface generates motion, enter and leave events for
 * the surfaces that the pointer is located over, and button and axis
 * events for button presses, button releases and scrolling.
 */
struct wl_pointer_listener {
	/**
	 * enter - enter event
	 * @serial: (none)
	 * @surface: (none)
	 * @surface_x: x coordinate in surface-relative coordinates
	 * @surface_y: y coordinate in surface-relative coordinates
	 *
	 * Notification that this seat's pointer is focused on a certain
	 * surface.
	 *
	 * When an seat's focus enters a surface, the pointer image is
	 * undefined and a client should respond to this event by setting
	 * an appropriate pointer image with the set_cursor request.
	 */
	void (*enter)(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      wl_fixed_t surface_x,
		      wl_fixed_t surface_y);
	/**
	 * leave - leave event
	 * @serial: (none)
	 * @surface: (none)
	 *
	 * Notification that this seat's pointer is no longer focused on
	 * a certain surface.
	 *
	 * The leave notification is sent before the enter notification for
	 * the new focus.
	 */
	void (*leave)(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface);
	/**
	 * motion - pointer motion event
	 * @time: timestamp with millisecond granularity
	 * @surface_x: x coordinate in surface-relative coordinates
	 * @surface_y: y coordinate in surface-relative coordinates
	 *
	 * Notification of pointer location change. The arguments
	 * surface_x and surface_y are the location relative to the focused
	 * surface.
	 */
	void (*motion)(void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t time,
		       wl_fixed_t surface_x,
		       wl_fixed_t surface_y);
	/**
	 * button - pointer button event
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @button: (none)
	 * @state: (none)
	 *
	 * Mouse button click and release notifications.
	 *
	 * The location of the click is given by the last motion or enter
	 * event. The time argument is a timestamp with millisecond
	 * granularity, with an undefined base.
	 */
	void (*button)(void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t serial,
		       uint32_t time,
		       uint32_t button,
		       uint32_t state);
	/**
	 * axis - axis event
	 * @time: timestamp with millisecond granularity
	 * @axis: (none)
	 * @value: (none)
	 *
	 * Scroll and other axis notifications.
	 *
	 * For scroll events (vertical and horizontal scroll axes), the
	 * value parameter is the length of a vector along the specified
	 * axis in a coordinate space identical to those of motion events,
	 * representing a relative movement along the specified axis.
	 *
	 * For devices that support movements non-parallel to axes multiple
	 * axis events will be emitted.
	 *
	 * When applicable, for example for touch pads, the server can
	 * choose to emit scroll events where the motion vector is
	 * equivalent to a motion event vector.
	 *
	 * When applicable, clients can transform its view relative to the
	 * scroll distance.
	 */
	void (*axis)(void *data,
		     struct wl_pointer *wl_pointer,
		     uint32_t time,
		     uint32_t axis,
		     wl_fixed_t value);
};

static inline int
wl_pointer_add_listener(struct wl_pointer *wl_pointer,
			const struct wl_pointer_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_pointer,
				     (void (**)(void)) listener, data);
}

#define WL_POINTER_SET_CURSOR	0
#define WL_POINTER_RELEASE	1

static inline void
wl_pointer_set_user_data(struct wl_pointer *wl_pointer, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_pointer, user_data);
}

static inline void *
wl_pointer_get_user_data(struct wl_pointer *wl_pointer)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_pointer);
}

static inline void
wl_pointer_destroy(struct wl_pointer *wl_pointer)
{
	wl_proxy_destroy((struct wl_proxy *) wl_pointer);
}

static inline void
wl_pointer_set_cursor(struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, int32_t hotspot_x, int32_t hotspot_y)
{
	wl_proxy_marshal((struct wl_proxy *) wl_pointer,
			 WL_POINTER_SET_CURSOR, serial, surface, hotspot_x, hotspot_y);
}

static inline void
wl_pointer_release(struct wl_pointer *wl_pointer)
{
	wl_proxy_marshal((struct wl_proxy *) wl_pointer,
			 WL_POINTER_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) wl_pointer);
}

#ifndef WL_KEYBOARD_KEYMAP_FORMAT_ENUM
#define WL_KEYBOARD_KEYMAP_FORMAT_ENUM
/**
 * wl_keyboard_keymap_format - keyboard mapping format
 * @WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP: no keymap; client must
 *	understand how to interpret the raw keycode
 * @WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1: libxkbcommon compatible; to
 *	determine the xkb keycode, clients must add 8 to the key event keycode
 *
 * This specifies the format of the keymap provided to the client with
 * the wl_keyboard.keymap event.
 */
enum wl_keyboard_keymap_format {
	WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP = 0,
	WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 = 1,
};
#endif /* WL_KEYBOARD_KEYMAP_FORMAT_ENUM */

#ifndef WL_KEYBOARD_KEY_STATE_ENUM
#define WL_KEYBOARD_KEY_STATE_ENUM
/**
 * wl_keyboard_key_state - physical key state
 * @WL_KEYBOARD_KEY_STATE_RELEASED: key is not pressed
 * @WL_KEYBOARD_KEY_STATE_PRESSED: key is pressed
 *
 * Describes the physical state of a key which provoked the key event.
 */
enum wl_keyboard_key_state {
	WL_KEYBOARD_KEY_STATE_RELEASED = 0,
	WL_KEYBOARD_KEY_STATE_PRESSED = 1,
};
#endif /* WL_KEYBOARD_KEY_STATE_ENUM */

/**
 * wl_keyboard - keyboard input device
 * @keymap: keyboard mapping
 * @enter: enter event
 * @leave: leave event
 * @key: key event
 * @modifiers: modifier and group state
 * @repeat_info: repeat rate and delay
 *
 * The wl_keyboard interface represents one or more keyboards associated
 * with a seat.
 */
struct wl_keyboard_listener {
	/**
	 * keymap - keyboard mapping
	 * @format: (none)
	 * @fd: (none)
	 * @size: (none)
	 *
	 * This event provides a file descriptor to the client which can
	 * be memory-mapped to provide a keyboard mapping description.
	 */
	void (*keymap)(void *data,
		       struct wl_keyboard *wl_keyboard,
		       uint32_t format,
		       int32_t fd,
		       uint32_t size);
	/**
	 * enter - enter event
	 * @serial: (none)
	 * @surface: (none)
	 * @keys: the currently pressed keys
	 *
	 * Notification that this seat's keyboard focus is on a certain
	 * surface.
	 */
	void (*enter)(void *data,
		      struct wl_keyboard *wl_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface,
		      struct wl_array *keys);
	/**
	 * leave - leave event
	 * @serial: (none)
	 * @surface: (none)
	 *
	 * Notification that this seat's keyboard focus is no longer on a
	 * certain surface.
	 *
	 * The leave notification is sent before the enter notification for
	 * the new focus.
	 */
	void (*leave)(void *data,
		      struct wl_keyboard *wl_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface);
	/**
	 * key - key event
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @key: (none)
	 * @state: (none)
	 *
	 * A key was pressed or released. The time argument is a
	 * timestamp with millisecond granularity, with an undefined base.
	 */
	void (*key)(void *data,
		    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t key,
		    uint32_t state);
	/**
	 * modifiers - modifier and group state
	 * @serial: (none)
	 * @mods_depressed: (none)
	 * @mods_latched: (none)
	 * @mods_locked: (none)
	 * @group: (none)
	 *
	 * Notifies clients that the modifier and/or group state has
	 * changed, and it should update its local state.
	 */
	void (*modifiers)(void *data,
			  struct wl_keyboard *wl_keyboard,
			  uint32_t serial,
			  uint32_t mods_depressed,
			  uint32_t mods_latched,
			  uint32_t mods_locked,
			  uint32_t group);
	/**
	 * repeat_info - repeat rate and delay
	 * @rate: the rate of repeating keys in characters per second
	 * @delay: delay in milliseconds since key down until repeating
	 *	starts
	 *
	 * Informs the client about the keyboard's repeat rate and delay.
	 *
	 * This event is sent as soon as the wl_keyboard object has been
	 * created, and is guaranteed to be received by the client before
	 * any key press event.
	 *
	 * Negative values for either rate or delay are illegal. A rate of
	 * zero will disable any repeating (regardless of the value of
	 * delay).
	 *
	 * This event can be sent later on as well with a new value if
	 * necessary, so clients should continue listening for the event
	 * past the creation of wl_keyboard.
	 * @since: 4
	 */
	void (*repeat_info)(void *data,
			    struct wl_keyboard *wl_keyboard,
			    int32_t rate,
			    int32_t delay);
};

static inline int
wl_keyboard_add_listener(struct wl_keyboard *wl_keyboard,
			 const struct wl_keyboard_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_keyboard,
				     (void (**)(void)) listener, data);
}

#define WL_KEYBOARD_RELEASE	0

static inline void
wl_keyboard_set_user_data(struct wl_keyboard *wl_keyboard, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_keyboard, user_data);
}

static inline void *
wl_keyboard_get_user_data(struct wl_keyboard *wl_keyboard)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_keyboard);
}

static inline void
wl_keyboard_destroy(struct wl_keyboard *wl_keyboard)
{
	wl_proxy_destroy((struct wl_proxy *) wl_keyboard);
}

static inline void
wl_keyboard_release(struct wl_keyboard *wl_keyboard)
{
	wl_proxy_marshal((struct wl_proxy *) wl_keyboard,
			 WL_KEYBOARD_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) wl_keyboard);
}

/**
 * wl_touch - touchscreen input device
 * @down: touch down event and beginning of a touch sequence
 * @up: end of a touch event sequence
 * @motion: update of touch point coordinates
 * @frame: end of touch frame event
 * @cancel: touch session cancelled
 *
 * The wl_touch interface represents a touchscreen associated with a
 * seat.
 *
 * Touch interactions can consist of one or more contacts. For each
 * contact, a series of events is generated, starting with a down event,
 * followed by zero or more motion events, and ending with an up event.
 * Events relating to the same contact point can be identified by the ID of
 * the sequence.
 */
struct wl_touch_listener {
	/**
	 * down - touch down event and beginning of a touch sequence
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @surface: (none)
	 * @id: the unique ID of this touch point
	 * @x: x coordinate in surface-relative coordinates
	 * @y: y coordinate in surface-relative coordinates
	 *
	 * A new touch point has appeared on the surface. This touch
	 * point is assigned a unique @id. Future events from this
	 * touchpoint reference this ID. The ID ceases to be valid after a
	 * touch up event and may be re-used in the future.
	 */
	void (*down)(void *data,
		     struct wl_touch *wl_touch,
		     uint32_t serial,
		     uint32_t time,
		     struct wl_surface *surface,
		     int32_t id,
		     wl_fixed_t x,
		     wl_fixed_t y);
	/**
	 * up - end of a touch event sequence
	 * @serial: (none)
	 * @time: timestamp with millisecond granularity
	 * @id: the unique ID of this touch point
	 *
	 * The touch point has disappeared. No further events will be
	 * sent for this touchpoint and the touch point's ID is released
	 * and may be re-used in a future touch down event.
	 */
	void (*up)(void *data,
		   struct wl_touch *wl_touch,
		   uint32_t serial,
		   uint32_t time,
		   int32_t id);
	/**
	 * motion - update of touch point coordinates
	 * @time: timestamp with millisecond granularity
	 * @id: the unique ID of this touch point
	 * @x: x coordinate in surface-relative coordinates
	 * @y: y coordinate in surface-relative coordinates
	 *
	 * A touchpoint has changed coordinates.
	 */
	void (*motion)(void *data,
		       struct wl_touch *wl_touch,
		       uint32_t time,
		       int32_t id,
		       wl_fixed_t x,
		       wl_fixed_t y);
	/**
	 * frame - end of touch frame event
	 *
	 * Indicates the end of a contact point list.
	 */
	void (*frame)(void *data,
		      struct wl_touch *wl_touch);
	/**
	 * cancel - touch session cancelled
	 *
	 * Sent if the compositor decides the touch stream is a global
	 * gesture. No further events are sent to the clients from that
	 * particular gesture. Touch cancellation applies to all touch
	 * points currently active on this client's surface. The client is
	 * responsible for finalizing the touch points, future touch points
	 * on this surface may re-use the touch point ID.
	 */
	void (*cancel)(void *data,
		       struct wl_touch *wl_touch);
};

static inline int
wl_touch_add_listener(struct wl_touch *wl_touch,
		      const struct wl_touch_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_touch,
				     (void (**)(void)) listener, data);
}

#define WL_TOUCH_RELEASE	0

static inline void
wl_touch_set_user_data(struct wl_touch *wl_touch, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_touch, user_data);
}

static inline void *
wl_touch_get_user_data(struct wl_touch *wl_touch)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_touch);
}

static inline void
wl_touch_destroy(struct wl_touch *wl_touch)
{
	wl_proxy_destroy((struct wl_proxy *) wl_touch);
}

static inline void
wl_touch_release(struct wl_touch *wl_touch)
{
	wl_proxy_marshal((struct wl_proxy *) wl_touch,
			 WL_TOUCH_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) wl_touch);
}

#ifndef WL_OUTPUT_SUBPIXEL_ENUM
#define WL_OUTPUT_SUBPIXEL_ENUM
/**
 * wl_output_subpixel - subpixel geometry information
 * @WL_OUTPUT_SUBPIXEL_UNKNOWN: (none)
 * @WL_OUTPUT_SUBPIXEL_NONE: (none)
 * @WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB: (none)
 * @WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR: (none)
 * @WL_OUTPUT_SUBPIXEL_VERTICAL_RGB: (none)
 * @WL_OUTPUT_SUBPIXEL_VERTICAL_BGR: (none)
 *
 * This enumeration describes how the physical pixels on an output are
 * laid out.
 */
enum wl_output_subpixel {
	WL_OUTPUT_SUBPIXEL_UNKNOWN = 0,
	WL_OUTPUT_SUBPIXEL_NONE = 1,
	WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB = 2,
	WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR = 3,
	WL_OUTPUT_SUBPIXEL_VERTICAL_RGB = 4,
	WL_OUTPUT_SUBPIXEL_VERTICAL_BGR = 5,
};
#endif /* WL_OUTPUT_SUBPIXEL_ENUM */

#ifndef WL_OUTPUT_TRANSFORM_ENUM
#define WL_OUTPUT_TRANSFORM_ENUM
/**
 * wl_output_transform - transform from framebuffer to output
 * @WL_OUTPUT_TRANSFORM_NORMAL: (none)
 * @WL_OUTPUT_TRANSFORM_90: (none)
 * @WL_OUTPUT_TRANSFORM_180: (none)
 * @WL_OUTPUT_TRANSFORM_270: (none)
 * @WL_OUTPUT_TRANSFORM_FLIPPED: (none)
 * @WL_OUTPUT_TRANSFORM_FLIPPED_90: (none)
 * @WL_OUTPUT_TRANSFORM_FLIPPED_180: (none)
 * @WL_OUTPUT_TRANSFORM_FLIPPED_270: (none)
 *
 * This describes the transform that a compositor will apply to a surface
 * to compensate for the rotation or mirroring of an output device.
 *
 * The flipped values correspond to an initial flip around a vertical axis
 * followed by rotation.
 *
 * The purpose is mainly to allow clients render accordingly and tell the
 * compositor, so that for fullscreen surfaces, the compositor will still
 * be able to scan out directly from client surfaces.
 */
enum wl_output_transform {
	WL_OUTPUT_TRANSFORM_NORMAL = 0,
	WL_OUTPUT_TRANSFORM_90 = 1,
	WL_OUTPUT_TRANSFORM_180 = 2,
	WL_OUTPUT_TRANSFORM_270 = 3,
	WL_OUTPUT_TRANSFORM_FLIPPED = 4,
	WL_OUTPUT_TRANSFORM_FLIPPED_90 = 5,
	WL_OUTPUT_TRANSFORM_FLIPPED_180 = 6,
	WL_OUTPUT_TRANSFORM_FLIPPED_270 = 7,
};
#endif /* WL_OUTPUT_TRANSFORM_ENUM */

#ifndef WL_OUTPUT_MODE_ENUM
#define WL_OUTPUT_MODE_ENUM
/**
 * wl_output_mode - mode information
 * @WL_OUTPUT_MODE_CURRENT: indicates this is the current mode
 * @WL_OUTPUT_MODE_PREFERRED: indicates this is the preferred mode
 *
 * These flags describe properties of an output mode. They are used in
 * the flags bitfield of the mode event.
 */
enum wl_output_mode {
	WL_OUTPUT_MODE_CURRENT = 0x1,
	WL_OUTPUT_MODE_PREFERRED = 0x2,
};
#endif /* WL_OUTPUT_MODE_ENUM */

/**
 * wl_output - compositor output region
 * @geometry: properties of the output
 * @mode: advertise available modes for the output
 * @done: sent all information about output
 * @scale: output scaling properties
 *
 * An output describes part of the compositor geometry. The compositor
 * works in the 'compositor coordinate system' and an output corresponds to
 * rectangular area in that space that is actually visible. This typically
 * corresponds to a monitor that displays part of the compositor space.
 * This object is published as global during start up, or when a monitor is
 * hotplugged.
 */
struct wl_output_listener {
	/**
	 * geometry - properties of the output
	 * @x: x position within the global compositor space
	 * @y: y position within the global compositor space
	 * @physical_width: width in millimeters of the output
	 * @physical_height: height in millimeters of the output
	 * @subpixel: subpixel orientation of the output
	 * @make: textual description of the manufacturer
	 * @model: textual description of the model
	 * @transform: transform that maps framebuffer to output
	 *
	 * The geometry event describes geometric properties of the
	 * output. The event is sent when binding to the output object and
	 * whenever any of the properties change.
	 */
	void (*geometry)(void *data,
			 struct wl_output *wl_output,
			 int32_t x,
			 int32_t y,
			 int32_t physical_width,
			 int32_t physical_height,
			 int32_t subpixel,
			 const char *make,
			 const char *model,
			 int32_t transform);
	/**
	 * mode - advertise available modes for the output
	 * @flags: bitfield of mode flags
	 * @width: width of the mode in hardware units
	 * @height: height of the mode in hardware units
	 * @refresh: vertical refresh rate in mHz
	 *
	 * The mode event describes an available mode for the output.
	 *
	 * The event is sent when binding to the output object and there
	 * will always be one mode, the current mode. The event is sent
	 * again if an output changes mode, for the mode that is now
	 * current. In other words, the current mode is always the last
	 * mode that was received with the current flag set.
	 *
	 * The size of a mode is given in physical hardware units of the
	 * output device. This is not necessarily the same as the output
	 * size in the global compositor space. For instance, the output
	 * may be scaled, as described in wl_output.scale, or transformed ,
	 * as described in wl_output.transform.
	 */
	void (*mode)(void *data,
		     struct wl_output *wl_output,
		     uint32_t flags,
		     int32_t width,
		     int32_t height,
		     int32_t refresh);
	/**
	 * done - sent all information about output
	 *
	 * This event is sent after all other properties has been sent
	 * after binding to the output object and after any other property
	 * changes done after that. This allows changes to the output
	 * properties to be seen as atomic, even if they happen via
	 * multiple events.
	 * @since: 2
	 */
	void (*done)(void *data,
		     struct wl_output *wl_output);
	/**
	 * scale - output scaling properties
	 * @factor: scaling factor of output
	 *
	 * This event contains scaling geometry information that is not
	 * in the geometry event. It may be sent after binding the output
	 * object or if the output scale changes later. If it is not sent,
	 * the client should assume a scale of 1.
	 *
	 * A scale larger than 1 means that the compositor will
	 * automatically scale surface buffers by this amount when
	 * rendering. This is used for very high resolution displays where
	 * applications rendering at the native resolution would be too
	 * small to be legible.
	 *
	 * It is intended that scaling aware clients track the current
	 * output of a surface, and if it is on a scaled output it should
	 * use wl_surface.set_buffer_scale with the scale of the output.
	 * That way the compositor can avoid scaling the surface, and the
	 * client can supply a higher detail image.
	 * @since: 2
	 */
	void (*scale)(void *data,
		      struct wl_output *wl_output,
		      int32_t factor);
};

static inline int
wl_output_add_listener(struct wl_output *wl_output,
		       const struct wl_output_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_output,
				     (void (**)(void)) listener, data);
}

static inline void
wl_output_set_user_data(struct wl_output *wl_output, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_output, user_data);
}

static inline void *
wl_output_get_user_data(struct wl_output *wl_output)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_output);
}

static inline void
wl_output_destroy(struct wl_output *wl_output)
{
	wl_proxy_destroy((struct wl_proxy *) wl_output);
}

#define WL_REGION_DESTROY	0
#define WL_REGION_ADD	1
#define WL_REGION_SUBTRACT	2

static inline void
wl_region_set_user_data(struct wl_region *wl_region, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_region, user_data);
}

static inline void *
wl_region_get_user_data(struct wl_region *wl_region)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_region);
}

static inline void
wl_region_destroy(struct wl_region *wl_region)
{
	wl_proxy_marshal((struct wl_proxy *) wl_region,
			 WL_REGION_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_region);
}

static inline void
wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_region,
			 WL_REGION_ADD, x, y, width, height);
}

static inline void
wl_region_subtract(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) wl_region,
			 WL_REGION_SUBTRACT, x, y, width, height);
}

#ifndef WL_SUBCOMPOSITOR_ERROR_ENUM
#define WL_SUBCOMPOSITOR_ERROR_ENUM
enum wl_subcompositor_error {
	WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE = 0,
};
#endif /* WL_SUBCOMPOSITOR_ERROR_ENUM */

#define WL_SUBCOMPOSITOR_DESTROY	0
#define WL_SUBCOMPOSITOR_GET_SUBSURFACE	1

static inline void
wl_subcompositor_set_user_data(struct wl_subcompositor *wl_subcompositor, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_subcompositor, user_data);
}

static inline void *
wl_subcompositor_get_user_data(struct wl_subcompositor *wl_subcompositor)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_subcompositor);
}

static inline void
wl_subcompositor_destroy(struct wl_subcompositor *wl_subcompositor)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subcompositor,
			 WL_SUBCOMPOSITOR_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_subcompositor);
}

static inline struct wl_subsurface *
wl_subcompositor_get_subsurface(struct wl_subcompositor *wl_subcompositor, struct wl_surface *surface, struct wl_surface *parent)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) wl_subcompositor,
			 WL_SUBCOMPOSITOR_GET_SUBSURFACE, &wl_subsurface_interface, NULL, surface, parent);

	return (struct wl_subsurface *) id;
}

#ifndef WL_SUBSURFACE_ERROR_ENUM
#define WL_SUBSURFACE_ERROR_ENUM
enum wl_subsurface_error {
	WL_SUBSURFACE_ERROR_BAD_SURFACE = 0,
};
#endif /* WL_SUBSURFACE_ERROR_ENUM */

#define WL_SUBSURFACE_DESTROY	0
#define WL_SUBSURFACE_SET_POSITION	1
#define WL_SUBSURFACE_PLACE_ABOVE	2
#define WL_SUBSURFACE_PLACE_BELOW	3
#define WL_SUBSURFACE_SET_SYNC	4
#define WL_SUBSURFACE_SET_DESYNC	5

static inline void
wl_subsurface_set_user_data(struct wl_subsurface *wl_subsurface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_subsurface, user_data);
}

static inline void *
wl_subsurface_get_user_data(struct wl_subsurface *wl_subsurface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_subsurface);
}

static inline void
wl_subsurface_destroy(struct wl_subsurface *wl_subsurface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subsurface,
			 WL_SUBSURFACE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) wl_subsurface);
}

static inline void
wl_subsurface_set_position(struct wl_subsurface *wl_subsurface, int32_t x, int32_t y)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subsurface,
			 WL_SUBSURFACE_SET_POSITION, x, y);
}

static inline void
wl_subsurface_place_above(struct wl_subsurface *wl_subsurface, struct wl_surface *sibling)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subsurface,
			 WL_SUBSURFACE_PLACE_ABOVE, sibling);
}

static inline void
wl_subsurface_place_below(struct wl_subsurface *wl_subsurface, struct wl_surface *sibling)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subsurface,
			 WL_SUBSURFACE_PLACE_BELOW, sibling);
}

static inline void
wl_subsurface_set_sync(struct wl_subsurface *wl_subsurface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subsurface,
			 WL_SUBSURFACE_SET_SYNC);
}

static inline void
wl_subsurface_set_desync(struct wl_subsurface *wl_subsurface)
{
	wl_proxy_marshal((struct wl_proxy *) wl_subsurface,
			 WL_SUBSURFACE_SET_DESYNC);
}

#ifdef  __cplusplus
}
#endif

#endif

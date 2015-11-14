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

#ifndef WAYLAND_SERVER_PROTOCOL_H
#define WAYLAND_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server-core.h"

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
 * @sync: asynchronous roundtrip
 * @get_registry: get global registry object
 *
 * The core global object. This is a special singleton object. It is used
 * for internal Wayland protocol features.
 */
struct wl_display_interface {
	/**
	 * sync - asynchronous roundtrip
	 * @callback: (none)
	 *
	 * The sync request asks the server to emit the 'done' event on
	 * the returned wl_callback object. Since requests are handled
	 * in-order and events are delivered in-order, this can be used as
	 * a barrier to ensure all previous requests and the resulting
	 * events have been handled.
	 *
	 * The object returned by this request will be destroyed by the
	 * compositor after the callback is fired and as such the client
	 * must not attempt to use it after that point.
	 *
	 * The callback_data passed in the callback is the event serial.
	 */
	void (*sync)(struct wl_client *client,
		     struct wl_resource *resource,
		     uint32_t callback);
	/**
	 * get_registry - get global registry object
	 * @registry: (none)
	 *
	 * This request creates a registry object that allows the client
	 * to list and bind the global objects available from the
	 * compositor.
	 */
	void (*get_registry)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t registry);
};

#define WL_DISPLAY_ERROR	0
#define WL_DISPLAY_DELETE_ID	1

#define WL_DISPLAY_ERROR_SINCE_VERSION	1
#define WL_DISPLAY_DELETE_ID_SINCE_VERSION	1

/**
 * wl_registry - global registry object
 * @bind: bind an object to the display
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
struct wl_registry_interface {
	/**
	 * bind - bind an object to the display
	 * @name: unique name for the object
	 * @interface: name of the objects interface
	 * @version: version of the objects interface
	 * @id: (none)
	 *
	 * Binds a new, client-created object to the server using the
	 * specified name as the identifier.
	 */
	void (*bind)(struct wl_client *client,
		     struct wl_resource *resource,
		     uint32_t name,
		     const char *interface, uint32_t version, uint32_t id);
};

#define WL_REGISTRY_GLOBAL	0
#define WL_REGISTRY_GLOBAL_REMOVE	1

#define WL_REGISTRY_GLOBAL_SINCE_VERSION	1
#define WL_REGISTRY_GLOBAL_REMOVE_SINCE_VERSION	1

static inline void
wl_registry_send_global(struct wl_resource *resource_, uint32_t name, const char *interface, uint32_t version)
{
	wl_resource_post_event(resource_, WL_REGISTRY_GLOBAL, name, interface, version);
}

static inline void
wl_registry_send_global_remove(struct wl_resource *resource_, uint32_t name)
{
	wl_resource_post_event(resource_, WL_REGISTRY_GLOBAL_REMOVE, name);
}

#define WL_CALLBACK_DONE	0

#define WL_CALLBACK_DONE_SINCE_VERSION	1

static inline void
wl_callback_send_done(struct wl_resource *resource_, uint32_t callback_data)
{
	wl_resource_post_event(resource_, WL_CALLBACK_DONE, callback_data);
}

/**
 * wl_compositor - the compositor singleton
 * @create_surface: create new surface
 * @create_region: create new region
 *
 * A compositor. This object is a singleton global. The compositor is in
 * charge of combining the contents of multiple surfaces into one
 * displayable output.
 */
struct wl_compositor_interface {
	/**
	 * create_surface - create new surface
	 * @id: (none)
	 *
	 * Ask the compositor to create a new surface.
	 */
	void (*create_surface)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t id);
	/**
	 * create_region - create new region
	 * @id: (none)
	 *
	 * Ask the compositor to create a new region.
	 */
	void (*create_region)(struct wl_client *client,
			      struct wl_resource *resource,
			      uint32_t id);
};


/**
 * wl_shm_pool - a shared memory pool
 * @create_buffer: create a buffer from the pool
 * @destroy: destroy the pool
 * @resize: change the size of the pool mapping
 *
 * The wl_shm_pool object encapsulates a piece of memory shared between
 * the compositor and client. Through the wl_shm_pool object, the client
 * can allocate shared memory wl_buffer objects. All objects created
 * through the same pool share the same underlying mapped memory. Reusing
 * the mapped memory avoids the setup/teardown overhead and is useful when
 * interactively resizing a surface or for many small buffers.
 */
struct wl_shm_pool_interface {
	/**
	 * create_buffer - create a buffer from the pool
	 * @id: (none)
	 * @offset: (none)
	 * @width: (none)
	 * @height: (none)
	 * @stride: (none)
	 * @format: (none)
	 *
	 * Create a wl_buffer object from the pool.
	 *
	 * The buffer is created offset bytes into the pool and has width
	 * and height as specified. The stride arguments specifies the
	 * number of bytes from beginning of one row to the beginning of
	 * the next. The format is the pixel format of the buffer and must
	 * be one of those advertised through the wl_shm.format event.
	 *
	 * A buffer will keep a reference to the pool it was created from
	 * so it is valid to destroy the pool immediately after creating a
	 * buffer from it.
	 */
	void (*create_buffer)(struct wl_client *client,
			      struct wl_resource *resource,
			      uint32_t id,
			      int32_t offset,
			      int32_t width,
			      int32_t height,
			      int32_t stride,
			      uint32_t format);
	/**
	 * destroy - destroy the pool
	 *
	 * Destroy the shared memory pool.
	 *
	 * The mmapped memory will be released when all buffers that have
	 * been created from this pool are gone.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * resize - change the size of the pool mapping
	 * @size: (none)
	 *
	 * This request will cause the server to remap the backing memory
	 * for the pool from the file descriptor passed when the pool was
	 * created, but using the new size. This request can only be used
	 * to make the pool bigger.
	 */
	void (*resize)(struct wl_client *client,
		       struct wl_resource *resource,
		       int32_t size);
};


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
 * @create_pool: create a shm pool
 *
 * A global singleton object that provides support for shared memory.
 *
 * Clients can create wl_shm_pool objects using the create_pool request.
 *
 * At connection setup time, the wl_shm object emits one or more format
 * events to inform clients about the valid pixel formats that can be used
 * for buffers.
 */
struct wl_shm_interface {
	/**
	 * create_pool - create a shm pool
	 * @id: (none)
	 * @fd: (none)
	 * @size: (none)
	 *
	 * Create a new wl_shm_pool object.
	 *
	 * The pool can be used to create shared memory based buffer
	 * objects. The server will mmap size bytes of the passed file
	 * descriptor, to use as backing memory for the pool.
	 */
	void (*create_pool)(struct wl_client *client,
			    struct wl_resource *resource,
			    uint32_t id,
			    int32_t fd,
			    int32_t size);
};

#define WL_SHM_FORMAT	0

#define WL_SHM_FORMAT_SINCE_VERSION	1

static inline void
wl_shm_send_format(struct wl_resource *resource_, uint32_t format)
{
	wl_resource_post_event(resource_, WL_SHM_FORMAT, format);
}

/**
 * wl_buffer - content for a wl_surface
 * @destroy: destroy a buffer
 *
 * A buffer provides the content for a wl_surface. Buffers are created
 * through factory interfaces such as wl_drm, wl_shm or similar. It has a
 * width and a height and can be attached to a wl_surface, but the
 * mechanism by which a client provides and updates the contents is defined
 * by the buffer factory interface.
 */
struct wl_buffer_interface {
	/**
	 * destroy - destroy a buffer
	 *
	 * Destroy a buffer. If and how you need to release the backing
	 * storage is defined by the buffer factory interface.
	 *
	 * For possible side-effects to a surface, see wl_surface.attach.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_BUFFER_RELEASE	0

#define WL_BUFFER_RELEASE_SINCE_VERSION	1

static inline void
wl_buffer_send_release(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_BUFFER_RELEASE);
}

/**
 * wl_data_offer - offer to transfer data
 * @accept: accept one of the offered mime types
 * @receive: request that the data is transferred
 * @destroy: destroy data offer
 *
 * A wl_data_offer represents a piece of data offered for transfer by
 * another client (the source client). It is used by the copy-and-paste and
 * drag-and-drop mechanisms. The offer describes the different mime types
 * that the data can be converted to and provides the mechanism for
 * transferring the data directly from the source client.
 */
struct wl_data_offer_interface {
	/**
	 * accept - accept one of the offered mime types
	 * @serial: (none)
	 * @mime_type: (none)
	 *
	 * Indicate that the client can accept the given mime type, or
	 * NULL for not accepted.
	 *
	 * Used for feedback during drag-and-drop.
	 */
	void (*accept)(struct wl_client *client,
		       struct wl_resource *resource,
		       uint32_t serial,
		       const char *mime_type);
	/**
	 * receive - request that the data is transferred
	 * @mime_type: (none)
	 * @fd: (none)
	 *
	 * To transfer the offered data, the client issues this request
	 * and indicates the mime type it wants to receive. The transfer
	 * happens through the passed file descriptor (typically created
	 * with the pipe system call). The source client writes the data in
	 * the mime type representation requested and then closes the file
	 * descriptor.
	 *
	 * The receiving client reads from the read end of the pipe until
	 * EOF and then closes its end, at which point the transfer is
	 * complete.
	 */
	void (*receive)(struct wl_client *client,
			struct wl_resource *resource,
			const char *mime_type,
			int32_t fd);
	/**
	 * destroy - destroy data offer
	 *
	 * Destroy the data offer.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_DATA_OFFER_OFFER	0

#define WL_DATA_OFFER_OFFER_SINCE_VERSION	1

static inline void
wl_data_offer_send_offer(struct wl_resource *resource_, const char *mime_type)
{
	wl_resource_post_event(resource_, WL_DATA_OFFER_OFFER, mime_type);
}

/**
 * wl_data_source - offer to transfer data
 * @offer: add an offered mime type
 * @destroy: destroy the data source
 *
 * The wl_data_source object is the source side of a wl_data_offer. It is
 * created by the source client in a data transfer and provides a way to
 * describe the offered data and a way to respond to requests to transfer
 * the data.
 */
struct wl_data_source_interface {
	/**
	 * offer - add an offered mime type
	 * @mime_type: (none)
	 *
	 * This request adds a mime type to the set of mime types
	 * advertised to targets. Can be called several times to offer
	 * multiple types.
	 */
	void (*offer)(struct wl_client *client,
		      struct wl_resource *resource,
		      const char *mime_type);
	/**
	 * destroy - destroy the data source
	 *
	 * Destroy the data source.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_DATA_SOURCE_TARGET	0
#define WL_DATA_SOURCE_SEND	1
#define WL_DATA_SOURCE_CANCELLED	2

#define WL_DATA_SOURCE_TARGET_SINCE_VERSION	1
#define WL_DATA_SOURCE_SEND_SINCE_VERSION	1
#define WL_DATA_SOURCE_CANCELLED_SINCE_VERSION	1

static inline void
wl_data_source_send_target(struct wl_resource *resource_, const char *mime_type)
{
	wl_resource_post_event(resource_, WL_DATA_SOURCE_TARGET, mime_type);
}

static inline void
wl_data_source_send_send(struct wl_resource *resource_, const char *mime_type, int32_t fd)
{
	wl_resource_post_event(resource_, WL_DATA_SOURCE_SEND, mime_type, fd);
}

static inline void
wl_data_source_send_cancelled(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_DATA_SOURCE_CANCELLED);
}

#ifndef WL_DATA_DEVICE_ERROR_ENUM
#define WL_DATA_DEVICE_ERROR_ENUM
enum wl_data_device_error {
	WL_DATA_DEVICE_ERROR_ROLE = 0,
};
#endif /* WL_DATA_DEVICE_ERROR_ENUM */

/**
 * wl_data_device - data transfer device
 * @start_drag: start drag-and-drop operation
 * @set_selection: copy data to the selection
 * @release: destroy data device
 *
 * There is one wl_data_device per seat which can be obtained from the
 * global wl_data_device_manager singleton.
 *
 * A wl_data_device provides access to inter-client data transfer
 * mechanisms such as copy-and-paste and drag-and-drop.
 */
struct wl_data_device_interface {
	/**
	 * start_drag - start drag-and-drop operation
	 * @source: (none)
	 * @origin: (none)
	 * @icon: (none)
	 * @serial: serial of the implicit grab on the origin
	 *
	 * This request asks the compositor to start a drag-and-drop
	 * operation on behalf of the client.
	 *
	 * The source argument is the data source that provides the data
	 * for the eventual data transfer. If source is NULL, enter, leave
	 * and motion events are sent only to the client that initiated the
	 * drag and the client is expected to handle the data passing
	 * internally.
	 *
	 * The origin surface is the surface where the drag originates and
	 * the client must have an active implicit grab that matches the
	 * serial.
	 *
	 * The icon surface is an optional (can be NULL) surface that
	 * provides an icon to be moved around with the cursor. Initially,
	 * the top-left corner of the icon surface is placed at the cursor
	 * hotspot, but subsequent wl_surface.attach request can move the
	 * relative position. Attach requests must be confirmed with
	 * wl_surface.commit as usual. The icon surface is given the role
	 * of a drag-and-drop icon. If the icon surface already has another
	 * role, it raises a protocol error.
	 *
	 * The current and pending input regions of the icon wl_surface are
	 * cleared, and wl_surface.set_input_region is ignored until the
	 * wl_surface is no longer used as the icon surface. When the use
	 * as an icon ends, the current and pending input regions become
	 * undefined, and the wl_surface is unmapped.
	 */
	void (*start_drag)(struct wl_client *client,
			   struct wl_resource *resource,
			   struct wl_resource *source,
			   struct wl_resource *origin,
			   struct wl_resource *icon,
			   uint32_t serial);
	/**
	 * set_selection - copy data to the selection
	 * @source: (none)
	 * @serial: serial of the event that triggered this request
	 *
	 * This request asks the compositor to set the selection to the
	 * data from the source on behalf of the client.
	 *
	 * To unset the selection, set the source to NULL.
	 */
	void (*set_selection)(struct wl_client *client,
			      struct wl_resource *resource,
			      struct wl_resource *source,
			      uint32_t serial);
	/**
	 * release - destroy data device
	 *
	 * This request destroys the data device.
	 * @since: 2
	 */
	void (*release)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_DATA_DEVICE_DATA_OFFER	0
#define WL_DATA_DEVICE_ENTER	1
#define WL_DATA_DEVICE_LEAVE	2
#define WL_DATA_DEVICE_MOTION	3
#define WL_DATA_DEVICE_DROP	4
#define WL_DATA_DEVICE_SELECTION	5

#define WL_DATA_DEVICE_DATA_OFFER_SINCE_VERSION	1
#define WL_DATA_DEVICE_ENTER_SINCE_VERSION	1
#define WL_DATA_DEVICE_LEAVE_SINCE_VERSION	1
#define WL_DATA_DEVICE_MOTION_SINCE_VERSION	1
#define WL_DATA_DEVICE_DROP_SINCE_VERSION	1
#define WL_DATA_DEVICE_SELECTION_SINCE_VERSION	1

static inline void
wl_data_device_send_data_offer(struct wl_resource *resource_, struct wl_resource *id)
{
	wl_resource_post_event(resource_, WL_DATA_DEVICE_DATA_OFFER, id);
}

static inline void
wl_data_device_send_enter(struct wl_resource *resource_, uint32_t serial, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y, struct wl_resource *id)
{
	wl_resource_post_event(resource_, WL_DATA_DEVICE_ENTER, serial, surface, x, y, id);
}

static inline void
wl_data_device_send_leave(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_DATA_DEVICE_LEAVE);
}

static inline void
wl_data_device_send_motion(struct wl_resource *resource_, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	wl_resource_post_event(resource_, WL_DATA_DEVICE_MOTION, time, x, y);
}

static inline void
wl_data_device_send_drop(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_DATA_DEVICE_DROP);
}

static inline void
wl_data_device_send_selection(struct wl_resource *resource_, struct wl_resource *id)
{
	wl_resource_post_event(resource_, WL_DATA_DEVICE_SELECTION, id);
}

/**
 * wl_data_device_manager - data transfer interface
 * @create_data_source: create a new data source
 * @get_data_device: create a new data device
 *
 * The wl_data_device_manager is a singleton global object that provides
 * access to inter-client data transfer mechanisms such as copy-and-paste
 * and drag-and-drop. These mechanisms are tied to a wl_seat and this
 * interface lets a client get a wl_data_device corresponding to a wl_seat.
 */
struct wl_data_device_manager_interface {
	/**
	 * create_data_source - create a new data source
	 * @id: (none)
	 *
	 * Create a new data source.
	 */
	void (*create_data_source)(struct wl_client *client,
				   struct wl_resource *resource,
				   uint32_t id);
	/**
	 * get_data_device - create a new data device
	 * @id: (none)
	 * @seat: (none)
	 *
	 * Create a new data device for a given seat.
	 */
	void (*get_data_device)(struct wl_client *client,
				struct wl_resource *resource,
				uint32_t id,
				struct wl_resource *seat);
};


#ifndef WL_SHELL_ERROR_ENUM
#define WL_SHELL_ERROR_ENUM
enum wl_shell_error {
	WL_SHELL_ERROR_ROLE = 0,
};
#endif /* WL_SHELL_ERROR_ENUM */

/**
 * wl_shell - create desktop-style surfaces
 * @get_shell_surface: create a shell surface from a surface
 *
 * This interface is implemented by servers that provide desktop-style
 * user interfaces.
 *
 * It allows clients to associate a wl_shell_surface with a basic surface.
 */
struct wl_shell_interface {
	/**
	 * get_shell_surface - create a shell surface from a surface
	 * @id: (none)
	 * @surface: (none)
	 *
	 * Create a shell surface for an existing surface. This gives the
	 * wl_surface the role of a shell surface. If the wl_surface
	 * already has another role, it raises a protocol error.
	 *
	 * Only one shell surface can be associated with a given surface.
	 */
	void (*get_shell_surface)(struct wl_client *client,
				  struct wl_resource *resource,
				  uint32_t id,
				  struct wl_resource *surface);
};


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
 * @pong: respond to a ping event
 * @move: start an interactive move
 * @resize: start an interactive resize
 * @set_toplevel: make the surface a toplevel surface
 * @set_transient: make the surface a transient surface
 * @set_fullscreen: make the surface a fullscreen surface
 * @set_popup: make the surface a popup surface
 * @set_maximized: make the surface a maximized surface
 * @set_title: set surface title
 * @set_class: set surface class
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
struct wl_shell_surface_interface {
	/**
	 * pong - respond to a ping event
	 * @serial: serial of the ping event
	 *
	 * A client must respond to a ping event with a pong request or
	 * the client may be deemed unresponsive.
	 */
	void (*pong)(struct wl_client *client,
		     struct wl_resource *resource,
		     uint32_t serial);
	/**
	 * move - start an interactive move
	 * @seat: the wl_seat whose pointer is used
	 * @serial: serial of the implicit grab on the pointer
	 *
	 * Start a pointer-driven move of the surface.
	 *
	 * This request must be used in response to a button press event.
	 * The server may ignore move requests depending on the state of
	 * the surface (e.g. fullscreen or maximized).
	 */
	void (*move)(struct wl_client *client,
		     struct wl_resource *resource,
		     struct wl_resource *seat,
		     uint32_t serial);
	/**
	 * resize - start an interactive resize
	 * @seat: the wl_seat whose pointer is used
	 * @serial: serial of the implicit grab on the pointer
	 * @edges: which edge or corner is being dragged
	 *
	 * Start a pointer-driven resizing of the surface.
	 *
	 * This request must be used in response to a button press event.
	 * The server may ignore resize requests depending on the state of
	 * the surface (e.g. fullscreen or maximized).
	 */
	void (*resize)(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *seat,
		       uint32_t serial,
		       uint32_t edges);
	/**
	 * set_toplevel - make the surface a toplevel surface
	 *
	 * Map the surface as a toplevel surface.
	 *
	 * A toplevel surface is not fullscreen, maximized or transient.
	 */
	void (*set_toplevel)(struct wl_client *client,
			     struct wl_resource *resource);
	/**
	 * set_transient - make the surface a transient surface
	 * @parent: (none)
	 * @x: (none)
	 * @y: (none)
	 * @flags: (none)
	 *
	 * Map the surface relative to an existing surface.
	 *
	 * The x and y arguments specify the locations of the upper left
	 * corner of the surface relative to the upper left corner of the
	 * parent surface, in surface local coordinates.
	 *
	 * The flags argument controls details of the transient behaviour.
	 */
	void (*set_transient)(struct wl_client *client,
			      struct wl_resource *resource,
			      struct wl_resource *parent,
			      int32_t x,
			      int32_t y,
			      uint32_t flags);
	/**
	 * set_fullscreen - make the surface a fullscreen surface
	 * @method: (none)
	 * @framerate: (none)
	 * @output: (none)
	 *
	 * Map the surface as a fullscreen surface.
	 *
	 * If an output parameter is given then the surface will be made
	 * fullscreen on that output. If the client does not specify the
	 * output then the compositor will apply its policy - usually
	 * choosing the output on which the surface has the biggest surface
	 * area.
	 *
	 * The client may specify a method to resolve a size conflict
	 * between the output size and the surface size - this is provided
	 * through the method parameter.
	 *
	 * The framerate parameter is used only when the method is set to
	 * "driver", to indicate the preferred framerate. A value of 0
	 * indicates that the app does not care about framerate. The
	 * framerate is specified in mHz, that is framerate of 60000 is
	 * 60Hz.
	 *
	 * A method of "scale" or "driver" implies a scaling operation of
	 * the surface, either via a direct scaling operation or a change
	 * of the output mode. This will override any kind of output
	 * scaling, so that mapping a surface with a buffer size equal to
	 * the mode can fill the screen independent of buffer_scale.
	 *
	 * A method of "fill" means we don't scale up the buffer, however
	 * any output scale is applied. This means that you may run into an
	 * edge case where the application maps a buffer with the same size
	 * of the output mode but buffer_scale 1 (thus making a surface
	 * larger than the output). In this case it is allowed to downscale
	 * the results to fit the screen.
	 *
	 * The compositor must reply to this request with a configure event
	 * with the dimensions for the output on which the surface will be
	 * made fullscreen.
	 */
	void (*set_fullscreen)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t method,
			       uint32_t framerate,
			       struct wl_resource *output);
	/**
	 * set_popup - make the surface a popup surface
	 * @seat: the wl_seat whose pointer is used
	 * @serial: serial of the implicit grab on the pointer
	 * @parent: (none)
	 * @x: (none)
	 * @y: (none)
	 * @flags: (none)
	 *
	 * Map the surface as a popup.
	 *
	 * A popup surface is a transient surface with an added pointer
	 * grab.
	 *
	 * An existing implicit grab will be changed to owner-events mode,
	 * and the popup grab will continue after the implicit grab ends
	 * (i.e. releasing the mouse button does not cause the popup to be
	 * unmapped).
	 *
	 * The popup grab continues until the window is destroyed or a
	 * mouse button is pressed in any other clients window. A click in
	 * any of the clients surfaces is reported as normal, however,
	 * clicks in other clients surfaces will be discarded and trigger
	 * the callback.
	 *
	 * The x and y arguments specify the locations of the upper left
	 * corner of the surface relative to the upper left corner of the
	 * parent surface, in surface local coordinates.
	 */
	void (*set_popup)(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *seat,
			  uint32_t serial,
			  struct wl_resource *parent,
			  int32_t x,
			  int32_t y,
			  uint32_t flags);
	/**
	 * set_maximized - make the surface a maximized surface
	 * @output: (none)
	 *
	 * Map the surface as a maximized surface.
	 *
	 * If an output parameter is given then the surface will be
	 * maximized on that output. If the client does not specify the
	 * output then the compositor will apply its policy - usually
	 * choosing the output on which the surface has the biggest surface
	 * area.
	 *
	 * The compositor will reply with a configure event telling the
	 * expected new surface size. The operation is completed on the
	 * next buffer attach to this surface.
	 *
	 * A maximized surface typically fills the entire output it is
	 * bound to, except for desktop element such as panels. This is the
	 * main difference between a maximized shell surface and a
	 * fullscreen shell surface.
	 *
	 * The details depend on the compositor implementation.
	 */
	void (*set_maximized)(struct wl_client *client,
			      struct wl_resource *resource,
			      struct wl_resource *output);
	/**
	 * set_title - set surface title
	 * @title: (none)
	 *
	 * Set a short title for the surface.
	 *
	 * This string may be used to identify the surface in a task bar,
	 * window list, or other user interface elements provided by the
	 * compositor.
	 *
	 * The string must be encoded in UTF-8.
	 */
	void (*set_title)(struct wl_client *client,
			  struct wl_resource *resource,
			  const char *title);
	/**
	 * set_class - set surface class
	 * @class_: (none)
	 *
	 * Set a class for the surface.
	 *
	 * The surface class identifies the general class of applications
	 * to which the surface belongs. A common convention is to use the
	 * file name (or the full path if it is a non-standard location) of
	 * the application's .desktop file as the class.
	 */
	void (*set_class)(struct wl_client *client,
			  struct wl_resource *resource,
			  const char *class_);
};

#define WL_SHELL_SURFACE_PING	0
#define WL_SHELL_SURFACE_CONFIGURE	1
#define WL_SHELL_SURFACE_POPUP_DONE	2

#define WL_SHELL_SURFACE_PING_SINCE_VERSION	1
#define WL_SHELL_SURFACE_CONFIGURE_SINCE_VERSION	1
#define WL_SHELL_SURFACE_POPUP_DONE_SINCE_VERSION	1

static inline void
wl_shell_surface_send_ping(struct wl_resource *resource_, uint32_t serial)
{
	wl_resource_post_event(resource_, WL_SHELL_SURFACE_PING, serial);
}

static inline void
wl_shell_surface_send_configure(struct wl_resource *resource_, uint32_t edges, int32_t width, int32_t height)
{
	wl_resource_post_event(resource_, WL_SHELL_SURFACE_CONFIGURE, edges, width, height);
}

static inline void
wl_shell_surface_send_popup_done(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_SHELL_SURFACE_POPUP_DONE);
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
 * @destroy: delete surface
 * @attach: set the surface contents
 * @damage: mark part of the surface damaged
 * @frame: request a frame throttling hint
 * @set_opaque_region: set opaque region
 * @set_input_region: set input region
 * @commit: commit pending surface state
 * @set_buffer_transform: sets the buffer transformation
 * @set_buffer_scale: sets the buffer scaling factor
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
struct wl_surface_interface {
	/**
	 * destroy - delete surface
	 *
	 * Deletes the surface and invalidates its object ID.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * attach - set the surface contents
	 * @buffer: (none)
	 * @x: (none)
	 * @y: (none)
	 *
	 * Set a buffer as the content of this surface.
	 *
	 * The new size of the surface is calculated based on the buffer
	 * size transformed by the inverse buffer_transform and the inverse
	 * buffer_scale. This means that the supplied buffer must be an
	 * integer multiple of the buffer_scale.
	 *
	 * The x and y arguments specify the location of the new pending
	 * buffer's upper left corner, relative to the current buffer's
	 * upper left corner, in surface local coordinates. In other words,
	 * the x and y, combined with the new surface size define in which
	 * directions the surface's size changes.
	 *
	 * Surface contents are double-buffered state, see
	 * wl_surface.commit.
	 *
	 * The initial surface contents are void; there is no content.
	 * wl_surface.attach assigns the given wl_buffer as the pending
	 * wl_buffer. wl_surface.commit makes the pending wl_buffer the new
	 * surface contents, and the size of the surface becomes the size
	 * calculated from the wl_buffer, as described above. After commit,
	 * there is no pending buffer until the next attach.
	 *
	 * Committing a pending wl_buffer allows the compositor to read the
	 * pixels in the wl_buffer. The compositor may access the pixels at
	 * any time after the wl_surface.commit request. When the
	 * compositor will not access the pixels anymore, it will send the
	 * wl_buffer.release event. Only after receiving wl_buffer.release,
	 * the client may re-use the wl_buffer. A wl_buffer that has been
	 * attached and then replaced by another attach instead of
	 * committed will not receive a release event, and is not used by
	 * the compositor.
	 *
	 * Destroying the wl_buffer after wl_buffer.release does not change
	 * the surface contents. However, if the client destroys the
	 * wl_buffer before receiving the wl_buffer.release event, the
	 * surface contents become undefined immediately.
	 *
	 * If wl_surface.attach is sent with a NULL wl_buffer, the
	 * following wl_surface.commit will remove the surface content.
	 */
	void (*attach)(struct wl_client *client,
		       struct wl_resource *resource,
		       struct wl_resource *buffer,
		       int32_t x,
		       int32_t y);
	/**
	 * damage - mark part of the surface damaged
	 * @x: (none)
	 * @y: (none)
	 * @width: (none)
	 * @height: (none)
	 *
	 * This request is used to describe the regions where the pending
	 * buffer is different from the current surface contents, and where
	 * the surface therefore needs to be repainted. The pending buffer
	 * must be set by wl_surface.attach before sending damage. The
	 * compositor ignores the parts of the damage that fall outside of
	 * the surface.
	 *
	 * Damage is double-buffered state, see wl_surface.commit.
	 *
	 * The damage rectangle is specified in surface local coordinates.
	 *
	 * The initial value for pending damage is empty: no damage.
	 * wl_surface.damage adds pending damage: the new pending damage is
	 * the union of old pending damage and the given rectangle.
	 *
	 * wl_surface.commit assigns pending damage as the current damage,
	 * and clears pending damage. The server will clear the current
	 * damage as it repaints the surface.
	 */
	void (*damage)(struct wl_client *client,
		       struct wl_resource *resource,
		       int32_t x,
		       int32_t y,
		       int32_t width,
		       int32_t height);
	/**
	 * frame - request a frame throttling hint
	 * @callback: (none)
	 *
	 * Request a notification when it is a good time start drawing a
	 * new frame, by creating a frame callback. This is useful for
	 * throttling redrawing operations, and driving animations.
	 *
	 * When a client is animating on a wl_surface, it can use the
	 * 'frame' request to get notified when it is a good time to draw
	 * and commit the next frame of animation. If the client commits an
	 * update earlier than that, it is likely that some updates will
	 * not make it to the display, and the client is wasting resources
	 * by drawing too often.
	 *
	 * The frame request will take effect on the next
	 * wl_surface.commit. The notification will only be posted for one
	 * frame unless requested again. For a wl_surface, the
	 * notifications are posted in the order the frame requests were
	 * committed.
	 *
	 * The server must send the notifications so that a client will not
	 * send excessive updates, while still allowing the highest
	 * possible update rate for clients that wait for the reply before
	 * drawing again. The server should give some time for the client
	 * to draw and commit after sending the frame callback events to
	 * let them hit the next output refresh.
	 *
	 * A server should avoid signalling the frame callbacks if the
	 * surface is not visible in any way, e.g. the surface is
	 * off-screen, or completely obscured by other opaque surfaces.
	 *
	 * The object returned by this request will be destroyed by the
	 * compositor after the callback is fired and as such the client
	 * must not attempt to use it after that point.
	 *
	 * The callback_data passed in the callback is the current time, in
	 * milliseconds, with an undefined base.
	 */
	void (*frame)(struct wl_client *client,
		      struct wl_resource *resource,
		      uint32_t callback);
	/**
	 * set_opaque_region - set opaque region
	 * @region: (none)
	 *
	 * This request sets the region of the surface that contains
	 * opaque content.
	 *
	 * The opaque region is an optimization hint for the compositor
	 * that lets it optimize out redrawing of content behind opaque
	 * regions. Setting an opaque region is not required for correct
	 * behaviour, but marking transparent content as opaque will result
	 * in repaint artifacts.
	 *
	 * The opaque region is specified in surface local coordinates.
	 *
	 * The compositor ignores the parts of the opaque region that fall
	 * outside of the surface.
	 *
	 * Opaque region is double-buffered state, see wl_surface.commit.
	 *
	 * wl_surface.set_opaque_region changes the pending opaque region.
	 * wl_surface.commit copies the pending region to the current
	 * region. Otherwise, the pending and current regions are never
	 * changed.
	 *
	 * The initial value for opaque region is empty. Setting the
	 * pending opaque region has copy semantics, and the wl_region
	 * object can be destroyed immediately. A NULL wl_region causes the
	 * pending opaque region to be set to empty.
	 */
	void (*set_opaque_region)(struct wl_client *client,
				  struct wl_resource *resource,
				  struct wl_resource *region);
	/**
	 * set_input_region - set input region
	 * @region: (none)
	 *
	 * This request sets the region of the surface that can receive
	 * pointer and touch events.
	 *
	 * Input events happening outside of this region will try the next
	 * surface in the server surface stack. The compositor ignores the
	 * parts of the input region that fall outside of the surface.
	 *
	 * The input region is specified in surface local coordinates.
	 *
	 * Input region is double-buffered state, see wl_surface.commit.
	 *
	 * wl_surface.set_input_region changes the pending input region.
	 * wl_surface.commit copies the pending region to the current
	 * region. Otherwise the pending and current regions are never
	 * changed, except cursor and icon surfaces are special cases, see
	 * wl_pointer.set_cursor and wl_data_device.start_drag.
	 *
	 * The initial value for input region is infinite. That means the
	 * whole surface will accept input. Setting the pending input
	 * region has copy semantics, and the wl_region object can be
	 * destroyed immediately. A NULL wl_region causes the input region
	 * to be set to infinite.
	 */
	void (*set_input_region)(struct wl_client *client,
				 struct wl_resource *resource,
				 struct wl_resource *region);
	/**
	 * commit - commit pending surface state
	 *
	 * Surface state (input, opaque, and damage regions, attached
	 * buffers, etc.) is double-buffered. Protocol requests modify the
	 * pending state, as opposed to current state in use by the
	 * compositor. Commit request atomically applies all pending state,
	 * replacing the current state. After commit, the new pending state
	 * is as documented for each related request.
	 *
	 * On commit, a pending wl_buffer is applied first, all other state
	 * second. This means that all coordinates in double-buffered state
	 * are relative to the new wl_buffer coming into use, except for
	 * wl_surface.attach itself. If there is no pending wl_buffer, the
	 * coordinates are relative to the current surface contents.
	 *
	 * All requests that need a commit to become effective are
	 * documented to affect double-buffered state.
	 *
	 * Other interfaces may add further double-buffered surface state.
	 */
	void (*commit)(struct wl_client *client,
		       struct wl_resource *resource);
	/**
	 * set_buffer_transform - sets the buffer transformation
	 * @transform: (none)
	 *
	 * This request sets an optional transformation on how the
	 * compositor interprets the contents of the buffer attached to the
	 * surface. The accepted values for the transform parameter are the
	 * values for wl_output.transform.
	 *
	 * Buffer transform is double-buffered state, see
	 * wl_surface.commit.
	 *
	 * A newly created surface has its buffer transformation set to
	 * normal.
	 *
	 * wl_surface.set_buffer_transform changes the pending buffer
	 * transformation. wl_surface.commit copies the pending buffer
	 * transformation to the current one. Otherwise, the pending and
	 * current values are never changed.
	 *
	 * The purpose of this request is to allow clients to render
	 * content according to the output transform, thus permiting the
	 * compositor to use certain optimizations even if the display is
	 * rotated. Using hardware overlays and scanning out a client
	 * buffer for fullscreen surfaces are examples of such
	 * optimizations. Those optimizations are highly dependent on the
	 * compositor implementation, so the use of this request should be
	 * considered on a case-by-case basis.
	 *
	 * Note that if the transform value includes 90 or 270 degree
	 * rotation, the width of the buffer will become the surface height
	 * and the height of the buffer will become the surface width.
	 *
	 * If transform is not one of the values from the
	 * wl_output.transform enum the invalid_transform protocol error is
	 * raised.
	 * @since: 2
	 */
	void (*set_buffer_transform)(struct wl_client *client,
				     struct wl_resource *resource,
				     int32_t transform);
	/**
	 * set_buffer_scale - sets the buffer scaling factor
	 * @scale: (none)
	 *
	 * This request sets an optional scaling factor on how the
	 * compositor interprets the contents of the buffer attached to the
	 * window.
	 *
	 * Buffer scale is double-buffered state, see wl_surface.commit.
	 *
	 * A newly created surface has its buffer scale set to 1.
	 *
	 * wl_surface.set_buffer_scale changes the pending buffer scale.
	 * wl_surface.commit copies the pending buffer scale to the current
	 * one. Otherwise, the pending and current values are never
	 * changed.
	 *
	 * The purpose of this request is to allow clients to supply higher
	 * resolution buffer data for use on high resolution outputs. Its
	 * intended that you pick the same buffer scale as the scale of the
	 * output that the surface is displayed on.This means the
	 * compositor can avoid scaling when rendering the surface on that
	 * output.
	 *
	 * Note that if the scale is larger than 1, then you have to attach
	 * a buffer that is larger (by a factor of scale in each dimension)
	 * than the desired surface size.
	 *
	 * If scale is not positive the invalid_scale protocol error is
	 * raised.
	 * @since: 3
	 */
	void (*set_buffer_scale)(struct wl_client *client,
				 struct wl_resource *resource,
				 int32_t scale);
};

#define WL_SURFACE_ENTER	0
#define WL_SURFACE_LEAVE	1

#define WL_SURFACE_ENTER_SINCE_VERSION	1
#define WL_SURFACE_LEAVE_SINCE_VERSION	1

static inline void
wl_surface_send_enter(struct wl_resource *resource_, struct wl_resource *output)
{
	wl_resource_post_event(resource_, WL_SURFACE_ENTER, output);
}

static inline void
wl_surface_send_leave(struct wl_resource *resource_, struct wl_resource *output)
{
	wl_resource_post_event(resource_, WL_SURFACE_LEAVE, output);
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
 * @get_pointer: return pointer object
 * @get_keyboard: return keyboard object
 * @get_touch: return touch object
 *
 * A seat is a group of keyboards, pointer and touch devices. This object
 * is published as a global during start up, or when such a device is hot
 * plugged. A seat typically has a pointer and maintains a keyboard focus
 * and a pointer focus.
 */
struct wl_seat_interface {
	/**
	 * get_pointer - return pointer object
	 * @id: (none)
	 *
	 * The ID provided will be initialized to the wl_pointer
	 * interface for this seat.
	 *
	 * This request only takes effect if the seat has the pointer
	 * capability.
	 */
	void (*get_pointer)(struct wl_client *client,
			    struct wl_resource *resource,
			    uint32_t id);
	/**
	 * get_keyboard - return keyboard object
	 * @id: (none)
	 *
	 * The ID provided will be initialized to the wl_keyboard
	 * interface for this seat.
	 *
	 * This request only takes effect if the seat has the keyboard
	 * capability.
	 */
	void (*get_keyboard)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id);
	/**
	 * get_touch - return touch object
	 * @id: (none)
	 *
	 * The ID provided will be initialized to the wl_touch interface
	 * for this seat.
	 *
	 * This request only takes effect if the seat has the touch
	 * capability.
	 */
	void (*get_touch)(struct wl_client *client,
			  struct wl_resource *resource,
			  uint32_t id);
};

#define WL_SEAT_CAPABILITIES	0
#define WL_SEAT_NAME	1

#define WL_SEAT_CAPABILITIES_SINCE_VERSION	1
#define WL_SEAT_NAME_SINCE_VERSION	2

static inline void
wl_seat_send_capabilities(struct wl_resource *resource_, uint32_t capabilities)
{
	wl_resource_post_event(resource_, WL_SEAT_CAPABILITIES, capabilities);
}

static inline void
wl_seat_send_name(struct wl_resource *resource_, const char *name)
{
	wl_resource_post_event(resource_, WL_SEAT_NAME, name);
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
 * @set_cursor: set the pointer surface
 * @release: release the pointer object
 *
 * The wl_pointer interface represents one or more input devices, such as
 * mice, which control the pointer location and pointer_focus of a seat.
 *
 * The wl_pointer interface generates motion, enter and leave events for
 * the surfaces that the pointer is located over, and button and axis
 * events for button presses, button releases and scrolling.
 */
struct wl_pointer_interface {
	/**
	 * set_cursor - set the pointer surface
	 * @serial: serial of the enter event
	 * @surface: (none)
	 * @hotspot_x: x coordinate in surface-relative coordinates
	 * @hotspot_y: y coordinate in surface-relative coordinates
	 *
	 * Set the pointer surface, i.e., the surface that contains the
	 * pointer image (cursor). This request gives the surface the role
	 * of a cursor. If the surface already has another role, it raises
	 * a protocol error.
	 *
	 * The cursor actually changes only if the pointer focus for this
	 * device is one of the requesting client's surfaces or the surface
	 * parameter is the current pointer surface. If there was a
	 * previous surface set with this request it is replaced. If
	 * surface is NULL, the pointer image is hidden.
	 *
	 * The parameters hotspot_x and hotspot_y define the position of
	 * the pointer surface relative to the pointer location. Its
	 * top-left corner is always at (x, y) - (hotspot_x, hotspot_y),
	 * where (x, y) are the coordinates of the pointer location, in
	 * surface local coordinates.
	 *
	 * On surface.attach requests to the pointer surface, hotspot_x and
	 * hotspot_y are decremented by the x and y parameters passed to
	 * the request. Attach must be confirmed by wl_surface.commit as
	 * usual.
	 *
	 * The hotspot can also be updated by passing the currently set
	 * pointer surface to this request with new values for hotspot_x
	 * and hotspot_y.
	 *
	 * The current and pending input regions of the wl_surface are
	 * cleared, and wl_surface.set_input_region is ignored until the
	 * wl_surface is no longer used as the cursor. When the use as a
	 * cursor ends, the current and pending input regions become
	 * undefined, and the wl_surface is unmapped.
	 */
	void (*set_cursor)(struct wl_client *client,
			   struct wl_resource *resource,
			   uint32_t serial,
			   struct wl_resource *surface,
			   int32_t hotspot_x,
			   int32_t hotspot_y);
	/**
	 * release - release the pointer object
	 *
	 * Using this request client can tell the server that it is not
	 * going to use the pointer object anymore.
	 *
	 * This request destroys the pointer proxy object, so user must not
	 * call wl_pointer_destroy() after using this request.
	 * @since: 3
	 */
	void (*release)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_POINTER_ENTER	0
#define WL_POINTER_LEAVE	1
#define WL_POINTER_MOTION	2
#define WL_POINTER_BUTTON	3
#define WL_POINTER_AXIS	4

#define WL_POINTER_ENTER_SINCE_VERSION	1
#define WL_POINTER_LEAVE_SINCE_VERSION	1
#define WL_POINTER_MOTION_SINCE_VERSION	1
#define WL_POINTER_BUTTON_SINCE_VERSION	1
#define WL_POINTER_AXIS_SINCE_VERSION	1

static inline void
wl_pointer_send_enter(struct wl_resource *resource_, uint32_t serial, struct wl_resource *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	wl_resource_post_event(resource_, WL_POINTER_ENTER, serial, surface, surface_x, surface_y);
}

static inline void
wl_pointer_send_leave(struct wl_resource *resource_, uint32_t serial, struct wl_resource *surface)
{
	wl_resource_post_event(resource_, WL_POINTER_LEAVE, serial, surface);
}

static inline void
wl_pointer_send_motion(struct wl_resource *resource_, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	wl_resource_post_event(resource_, WL_POINTER_MOTION, time, surface_x, surface_y);
}

static inline void
wl_pointer_send_button(struct wl_resource *resource_, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	wl_resource_post_event(resource_, WL_POINTER_BUTTON, serial, time, button, state);
}

static inline void
wl_pointer_send_axis(struct wl_resource *resource_, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	wl_resource_post_event(resource_, WL_POINTER_AXIS, time, axis, value);
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
 * @release: release the keyboard object
 *
 * The wl_keyboard interface represents one or more keyboards associated
 * with a seat.
 */
struct wl_keyboard_interface {
	/**
	 * release - release the keyboard object
	 *
	 * 
	 * @since: 3
	 */
	void (*release)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_KEYBOARD_KEYMAP	0
#define WL_KEYBOARD_ENTER	1
#define WL_KEYBOARD_LEAVE	2
#define WL_KEYBOARD_KEY	3
#define WL_KEYBOARD_MODIFIERS	4
#define WL_KEYBOARD_REPEAT_INFO	5

#define WL_KEYBOARD_KEYMAP_SINCE_VERSION	1
#define WL_KEYBOARD_ENTER_SINCE_VERSION	1
#define WL_KEYBOARD_LEAVE_SINCE_VERSION	1
#define WL_KEYBOARD_KEY_SINCE_VERSION	1
#define WL_KEYBOARD_MODIFIERS_SINCE_VERSION	1
#define WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION	4

static inline void
wl_keyboard_send_keymap(struct wl_resource *resource_, uint32_t format, int32_t fd, uint32_t size)
{
	wl_resource_post_event(resource_, WL_KEYBOARD_KEYMAP, format, fd, size);
}

static inline void
wl_keyboard_send_enter(struct wl_resource *resource_, uint32_t serial, struct wl_resource *surface, struct wl_array *keys)
{
	wl_resource_post_event(resource_, WL_KEYBOARD_ENTER, serial, surface, keys);
}

static inline void
wl_keyboard_send_leave(struct wl_resource *resource_, uint32_t serial, struct wl_resource *surface)
{
	wl_resource_post_event(resource_, WL_KEYBOARD_LEAVE, serial, surface);
}

static inline void
wl_keyboard_send_key(struct wl_resource *resource_, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	wl_resource_post_event(resource_, WL_KEYBOARD_KEY, serial, time, key, state);
}

static inline void
wl_keyboard_send_modifiers(struct wl_resource *resource_, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
	wl_resource_post_event(resource_, WL_KEYBOARD_MODIFIERS, serial, mods_depressed, mods_latched, mods_locked, group);
}

static inline void
wl_keyboard_send_repeat_info(struct wl_resource *resource_, int32_t rate, int32_t delay)
{
	wl_resource_post_event(resource_, WL_KEYBOARD_REPEAT_INFO, rate, delay);
}

/**
 * wl_touch - touchscreen input device
 * @release: release the touch object
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
struct wl_touch_interface {
	/**
	 * release - release the touch object
	 *
	 * 
	 * @since: 3
	 */
	void (*release)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WL_TOUCH_DOWN	0
#define WL_TOUCH_UP	1
#define WL_TOUCH_MOTION	2
#define WL_TOUCH_FRAME	3
#define WL_TOUCH_CANCEL	4

#define WL_TOUCH_DOWN_SINCE_VERSION	1
#define WL_TOUCH_UP_SINCE_VERSION	1
#define WL_TOUCH_MOTION_SINCE_VERSION	1
#define WL_TOUCH_FRAME_SINCE_VERSION	1
#define WL_TOUCH_CANCEL_SINCE_VERSION	1

static inline void
wl_touch_send_down(struct wl_resource *resource_, uint32_t serial, uint32_t time, struct wl_resource *surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	wl_resource_post_event(resource_, WL_TOUCH_DOWN, serial, time, surface, id, x, y);
}

static inline void
wl_touch_send_up(struct wl_resource *resource_, uint32_t serial, uint32_t time, int32_t id)
{
	wl_resource_post_event(resource_, WL_TOUCH_UP, serial, time, id);
}

static inline void
wl_touch_send_motion(struct wl_resource *resource_, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	wl_resource_post_event(resource_, WL_TOUCH_MOTION, time, id, x, y);
}

static inline void
wl_touch_send_frame(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_TOUCH_FRAME);
}

static inline void
wl_touch_send_cancel(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_TOUCH_CANCEL);
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

#define WL_OUTPUT_GEOMETRY	0
#define WL_OUTPUT_MODE	1
#define WL_OUTPUT_DONE	2
#define WL_OUTPUT_SCALE	3

#define WL_OUTPUT_GEOMETRY_SINCE_VERSION	1
#define WL_OUTPUT_MODE_SINCE_VERSION	1
#define WL_OUTPUT_DONE_SINCE_VERSION	2
#define WL_OUTPUT_SCALE_SINCE_VERSION	2

static inline void
wl_output_send_geometry(struct wl_resource *resource_, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{
	wl_resource_post_event(resource_, WL_OUTPUT_GEOMETRY, x, y, physical_width, physical_height, subpixel, make, model, transform);
}

static inline void
wl_output_send_mode(struct wl_resource *resource_, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
	wl_resource_post_event(resource_, WL_OUTPUT_MODE, flags, width, height, refresh);
}

static inline void
wl_output_send_done(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WL_OUTPUT_DONE);
}

static inline void
wl_output_send_scale(struct wl_resource *resource_, int32_t factor)
{
	wl_resource_post_event(resource_, WL_OUTPUT_SCALE, factor);
}

/**
 * wl_region - region interface
 * @destroy: destroy region
 * @add: add rectangle to region
 * @subtract: subtract rectangle from region
 *
 * A region object describes an area.
 *
 * Region objects are used to describe the opaque and input regions of a
 * surface.
 */
struct wl_region_interface {
	/**
	 * destroy - destroy region
	 *
	 * Destroy the region. This will invalidate the object ID.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * add - add rectangle to region
	 * @x: (none)
	 * @y: (none)
	 * @width: (none)
	 * @height: (none)
	 *
	 * Add the specified rectangle to the region.
	 */
	void (*add)(struct wl_client *client,
		    struct wl_resource *resource,
		    int32_t x,
		    int32_t y,
		    int32_t width,
		    int32_t height);
	/**
	 * subtract - subtract rectangle from region
	 * @x: (none)
	 * @y: (none)
	 * @width: (none)
	 * @height: (none)
	 *
	 * Subtract the specified rectangle from the region.
	 */
	void (*subtract)(struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t x,
			 int32_t y,
			 int32_t width,
			 int32_t height);
};


#ifndef WL_SUBCOMPOSITOR_ERROR_ENUM
#define WL_SUBCOMPOSITOR_ERROR_ENUM
enum wl_subcompositor_error {
	WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE = 0,
};
#endif /* WL_SUBCOMPOSITOR_ERROR_ENUM */

/**
 * wl_subcompositor - sub-surface compositing
 * @destroy: unbind from the subcompositor interface
 * @get_subsurface: give a surface the role sub-surface
 *
 * The global interface exposing sub-surface compositing capabilities. A
 * wl_surface, that has sub-surfaces associated, is called the parent
 * surface. Sub-surfaces can be arbitrarily nested and create a tree of
 * sub-surfaces.
 *
 * The root surface in a tree of sub-surfaces is the main surface. The main
 * surface cannot be a sub-surface, because sub-surfaces must always have a
 * parent.
 *
 * A main surface with its sub-surfaces forms a (compound) window. For
 * window management purposes, this set of wl_surface objects is to be
 * considered as a single window, and it should also behave as such.
 *
 * The aim of sub-surfaces is to offload some of the compositing work
 * within a window from clients to the compositor. A prime example is a
 * video player with decorations and video in separate wl_surface objects.
 * This should allow the compositor to pass YUV video buffer processing to
 * dedicated overlay hardware when possible.
 */
struct wl_subcompositor_interface {
	/**
	 * destroy - unbind from the subcompositor interface
	 *
	 * Informs the server that the client will not be using this
	 * protocol object anymore. This does not affect any other objects,
	 * wl_subsurface objects included.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * get_subsurface - give a surface the role sub-surface
	 * @id: the new subsurface object id
	 * @surface: the surface to be turned into a sub-surface
	 * @parent: the parent surface
	 *
	 * Create a sub-surface interface for the given surface, and
	 * associate it with the given parent surface. This turns a plain
	 * wl_surface into a sub-surface.
	 *
	 * The to-be sub-surface must not already have another role, and it
	 * must not have an existing wl_subsurface object. Otherwise a
	 * protocol error is raised.
	 */
	void (*get_subsurface)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t id,
			       struct wl_resource *surface,
			       struct wl_resource *parent);
};


#ifndef WL_SUBSURFACE_ERROR_ENUM
#define WL_SUBSURFACE_ERROR_ENUM
enum wl_subsurface_error {
	WL_SUBSURFACE_ERROR_BAD_SURFACE = 0,
};
#endif /* WL_SUBSURFACE_ERROR_ENUM */

/**
 * wl_subsurface - sub-surface interface to a wl_surface
 * @destroy: remove sub-surface interface
 * @set_position: reposition the sub-surface
 * @place_above: restack the sub-surface
 * @place_below: restack the sub-surface
 * @set_sync: set sub-surface to synchronized mode
 * @set_desync: set sub-surface to desynchronized mode
 *
 * An additional interface to a wl_surface object, which has been made a
 * sub-surface. A sub-surface has one parent surface. A sub-surface's size
 * and position are not limited to that of the parent. Particularly, a
 * sub-surface is not automatically clipped to its parent's area.
 *
 * A sub-surface becomes mapped, when a non-NULL wl_buffer is applied and
 * the parent surface is mapped. The order of which one happens first is
 * irrelevant. A sub-surface is hidden if the parent becomes hidden, or if
 * a NULL wl_buffer is applied. These rules apply recursively through the
 * tree of surfaces.
 *
 * The behaviour of wl_surface.commit request on a sub-surface depends on
 * the sub-surface's mode. The possible modes are synchronized and
 * desynchronized, see methods wl_subsurface.set_sync and
 * wl_subsurface.set_desync. Synchronized mode caches the wl_surface state
 * to be applied when the parent's state gets applied, and desynchronized
 * mode applies the pending wl_surface state directly. A sub-surface is
 * initially in the synchronized mode.
 *
 * Sub-surfaces have also other kind of state, which is managed by
 * wl_subsurface requests, as opposed to wl_surface requests. This state
 * includes the sub-surface position relative to the parent surface
 * (wl_subsurface.set_position), and the stacking order of the parent and
 * its sub-surfaces (wl_subsurface.place_above and .place_below). This
 * state is applied when the parent surface's wl_surface state is applied,
 * regardless of the sub-surface's mode. As the exception, set_sync and
 * set_desync are effective immediately.
 *
 * The main surface can be thought to be always in desynchronized mode,
 * since it does not have a parent in the sub-surfaces sense.
 *
 * Even if a sub-surface is in desynchronized mode, it will behave as in
 * synchronized mode, if its parent surface behaves as in synchronized
 * mode. This rule is applied recursively throughout the tree of surfaces.
 * This means, that one can set a sub-surface into synchronized mode, and
 * then assume that all its child and grand-child sub-surfaces are
 * synchronized, too, without explicitly setting them.
 *
 * If the wl_surface associated with the wl_subsurface is destroyed, the
 * wl_subsurface object becomes inert. Note, that destroying either object
 * takes effect immediately. If you need to synchronize the removal of a
 * sub-surface to the parent surface update, unmap the sub-surface first by
 * attaching a NULL wl_buffer, update parent, and then destroy the
 * sub-surface.
 *
 * If the parent wl_surface object is destroyed, the sub-surface is
 * unmapped.
 */
struct wl_subsurface_interface {
	/**
	 * destroy - remove sub-surface interface
	 *
	 * The sub-surface interface is removed from the wl_surface
	 * object that was turned into a sub-surface with
	 * wl_subcompositor.get_subsurface request. The wl_surface's
	 * association to the parent is deleted, and the wl_surface loses
	 * its role as a sub-surface. The wl_surface is unmapped.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * set_position - reposition the sub-surface
	 * @x: coordinate in the parent surface
	 * @y: coordinate in the parent surface
	 *
	 * This schedules a sub-surface position change. The sub-surface
	 * will be moved so, that its origin (top-left corner pixel) will
	 * be at the location x, y of the parent surface coordinate system.
	 * The coordinates are not restricted to the parent surface area.
	 * Negative values are allowed.
	 *
	 * The scheduled coordinates will take effect whenever the state of
	 * the parent surface is applied. When this happens depends on
	 * whether the parent surface is in synchronized mode or not. See
	 * wl_subsurface.set_sync and wl_subsurface.set_desync for details.
	 *
	 * If more than one set_position request is invoked by the client
	 * before the commit of the parent surface, the position of a new
	 * request always replaces the scheduled position from any previous
	 * request.
	 *
	 * The initial position is 0, 0.
	 */
	void (*set_position)(struct wl_client *client,
			     struct wl_resource *resource,
			     int32_t x,
			     int32_t y);
	/**
	 * place_above - restack the sub-surface
	 * @sibling: the reference surface
	 *
	 * This sub-surface is taken from the stack, and put back just
	 * above the reference surface, changing the z-order of the
	 * sub-surfaces. The reference surface must be one of the sibling
	 * surfaces, or the parent surface. Using any other surface,
	 * including this sub-surface, will cause a protocol error.
	 *
	 * The z-order is double-buffered. Requests are handled in order
	 * and applied immediately to a pending state. The final pending
	 * state is copied to the active state the next time the state of
	 * the parent surface is applied. When this happens depends on
	 * whether the parent surface is in synchronized mode or not. See
	 * wl_subsurface.set_sync and wl_subsurface.set_desync for details.
	 *
	 * A new sub-surface is initially added as the top-most in the
	 * stack of its siblings and parent.
	 */
	void (*place_above)(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *sibling);
	/**
	 * place_below - restack the sub-surface
	 * @sibling: the reference surface
	 *
	 * The sub-surface is placed just below of the reference surface.
	 * See wl_subsurface.place_above.
	 */
	void (*place_below)(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *sibling);
	/**
	 * set_sync - set sub-surface to synchronized mode
	 *
	 * Change the commit behaviour of the sub-surface to synchronized
	 * mode, also described as the parent dependent mode.
	 *
	 * In synchronized mode, wl_surface.commit on a sub-surface will
	 * accumulate the committed state in a cache, but the state will
	 * not be applied and hence will not change the compositor output.
	 * The cached state is applied to the sub-surface immediately after
	 * the parent surface's state is applied. This ensures atomic
	 * updates of the parent and all its synchronized sub-surfaces.
	 * Applying the cached state will invalidate the cache, so further
	 * parent surface commits do not (re-)apply old state.
	 *
	 * See wl_subsurface for the recursive effect of this mode.
	 */
	void (*set_sync)(struct wl_client *client,
			 struct wl_resource *resource);
	/**
	 * set_desync - set sub-surface to desynchronized mode
	 *
	 * Change the commit behaviour of the sub-surface to
	 * desynchronized mode, also described as independent or freely
	 * running mode.
	 *
	 * In desynchronized mode, wl_surface.commit on a sub-surface will
	 * apply the pending state directly, without caching, as happens
	 * normally with a wl_surface. Calling wl_surface.commit on the
	 * parent surface has no effect on the sub-surface's wl_surface
	 * state. This mode allows a sub-surface to be updated on its own.
	 *
	 * If cached state exists when wl_surface.commit is called in
	 * desynchronized mode, the pending state is added to the cached
	 * state, and applied as whole. This invalidates the cache.
	 *
	 * Note: even if a sub-surface is set to desynchronized, a parent
	 * sub-surface may override it to behave as synchronized. For
	 * details, see wl_subsurface.
	 *
	 * If a surface's parent surface behaves as desynchronized, then
	 * the cached state is applied on set_desync.
	 */
	void (*set_desync)(struct wl_client *client,
			   struct wl_resource *resource);
};


#ifdef  __cplusplus
}
#endif

#endif

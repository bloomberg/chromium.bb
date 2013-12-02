/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * This file provides just enough types and stubs, so that the rpi-backend
 * can be built without the real headers and libraries of the Raspberry Pi.
 *
 * This file CANNOT be used to build a working rpi-backend, it is intended
 * only for build-testing, when the proper headers are not available.
 */

#ifndef RPI_BCM_STUBS
#define RPI_BCM_STUBS

#include <stdint.h>

/* from /opt/vc/include/bcm_host.h */

static inline void bcm_host_init(void) {}
static inline void bcm_host_deinit(void) {}


/* from /opt/vc/include/interface/vmcs_host/vc_dispservice_defs.h */

#define TRANSFORM_HFLIP     (1<<0)
#define TRANSFORM_VFLIP     (1<<1)
#define TRANSFORM_TRANSPOSE (1<<2)


/* from /opt/vc/include/interface/vctypes/vc_display_types.h */

typedef enum
{
	VCOS_DISPLAY_INPUT_FORMAT_INVALID = 0,
} DISPLAY_INPUT_FORMAT_T;

/* from /opt/vc/include/interface/vctypes/vc_image_types.h */

typedef struct tag_VC_RECT_T {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
} VC_RECT_T;

typedef enum {
	VC_IMAGE_ROT0,
	/* these are not the right values: */
	VC_IMAGE_ROT90,
	VC_IMAGE_ROT180,
	VC_IMAGE_ROT270,
	VC_IMAGE_MIRROR_ROT0,
	VC_IMAGE_MIRROR_ROT90,
	VC_IMAGE_MIRROR_ROT180,
	VC_IMAGE_MIRROR_ROT270,
} VC_IMAGE_TRANSFORM_T;

typedef enum
{
	VC_IMAGE_MIN = 0,
	/* these are not the right values: */
	VC_IMAGE_ARGB8888,
	VC_IMAGE_XRGB8888,
	VC_IMAGE_RGB565,
} VC_IMAGE_TYPE_T;

/* from /opt/vc/include/interface/vmcs_host/vc_dispmanx_types.h */

typedef uint32_t DISPMANX_DISPLAY_HANDLE_T;
typedef uint32_t DISPMANX_UPDATE_HANDLE_T;
typedef uint32_t DISPMANX_ELEMENT_HANDLE_T;
typedef uint32_t DISPMANX_RESOURCE_HANDLE_T;
typedef uint32_t DISPMANX_PROTECTION_T;

#define DISPMANX_NO_HANDLE 0
#define DISPMANX_PROTECTION_NONE  0
#define DISPMANX_ID_HDMI      2

typedef enum {
	/* Bottom 2 bits sets the alpha mode */
	DISPMANX_FLAGS_ALPHA_FROM_SOURCE = 0,
	DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS = 1,
	DISPMANX_FLAGS_ALPHA_FIXED_NON_ZERO = 2,
	DISPMANX_FLAGS_ALPHA_FIXED_EXCEED_0X07 = 3,

	DISPMANX_FLAGS_ALPHA_PREMULT = 1 << 16,
	DISPMANX_FLAGS_ALPHA_MIX = 1 << 17
} DISPMANX_FLAGS_ALPHA_T;

typedef struct {
	DISPMANX_FLAGS_ALPHA_T flags;
	uint32_t opacity;
	DISPMANX_RESOURCE_HANDLE_T mask;
} VC_DISPMANX_ALPHA_T;

typedef struct {
	int32_t width;
	int32_t height;
	VC_IMAGE_TRANSFORM_T transform;
	DISPLAY_INPUT_FORMAT_T input_format;
} DISPMANX_MODEINFO_T;

typedef enum {
	DISPMANX_NO_ROTATE = 0,
	DISPMANX_ROTATE_90 = 1,
	DISPMANX_ROTATE_180 = 2,
	DISPMANX_ROTATE_270 = 3,

	DISPMANX_FLIP_HRIZ = 1 << 16,
	DISPMANX_FLIP_VERT = 1 << 17
} DISPMANX_TRANSFORM_T;

typedef struct {
	uint32_t dummy;
} DISPMANX_CLAMP_T;

typedef void (*DISPMANX_CALLBACK_FUNC_T)(DISPMANX_UPDATE_HANDLE_T u,
					 void *arg);

/* from /opt/vc/include/interface/vmcs_host/vc_dispmanx.h */

static inline int
vc_dispmanx_rect_set(VC_RECT_T *rect, uint32_t x_offset, uint32_t y_offset,
		     uint32_t width, uint32_t height)
{
	rect->x = x_offset;
	rect->y = y_offset;
	rect->width = width;
	rect->height = height;
	return 0;
}

static inline DISPMANX_RESOURCE_HANDLE_T
vc_dispmanx_resource_create(VC_IMAGE_TYPE_T type, uint32_t width,
			    uint32_t height, uint32_t *native_image_handle)
{
	return DISPMANX_NO_HANDLE;
}

static inline int
vc_dispmanx_resource_write_data(DISPMANX_RESOURCE_HANDLE_T res,
				VC_IMAGE_TYPE_T src_type,
				int src_pitch,
				void *src_address,
				const VC_RECT_T *rect)
{
	return -1;
}

static inline int
vc_dispmanx_resource_write_data_rect(DISPMANX_RESOURCE_HANDLE_T handle,
				     VC_IMAGE_TYPE_T src_type,
				     int src_pitch,
				     void *src_address,
				     const VC_RECT_T *src_rect,
				     uint32_t dst_x,
				     uint32_t dst_y)
{
	return -1;
}

static inline int
vc_dispmanx_resource_read_data(DISPMANX_RESOURCE_HANDLE_T handle,
			       const VC_RECT_T *p_rect,
			       void *dst_address,
			       uint32_t dst_pitch)
{
	return -1;
}

static inline int
vc_dispmanx_resource_delete(DISPMANX_RESOURCE_HANDLE_T res)
{
	return -1;
}

static inline DISPMANX_DISPLAY_HANDLE_T
vc_dispmanx_display_open(uint32_t device)
{
	return DISPMANX_NO_HANDLE;
}

static inline int
vc_dispmanx_display_close(DISPMANX_DISPLAY_HANDLE_T display)
{
	return -1;
}

static inline int
vc_dispmanx_display_get_info(DISPMANX_DISPLAY_HANDLE_T display,
			     DISPMANX_MODEINFO_T *pinfo)
{
	return -1;
}

static inline DISPMANX_UPDATE_HANDLE_T
vc_dispmanx_update_start(int32_t priority)
{
	return DISPMANX_NO_HANDLE;
}

static inline DISPMANX_ELEMENT_HANDLE_T
vc_dispmanx_element_add(DISPMANX_UPDATE_HANDLE_T update,
			DISPMANX_DISPLAY_HANDLE_T display,
			int32_t layer,
			const VC_RECT_T *dest_rect,
			DISPMANX_RESOURCE_HANDLE_T src,
			const VC_RECT_T *src_rect,
			DISPMANX_PROTECTION_T protection,
			VC_DISPMANX_ALPHA_T *alpha,
			DISPMANX_CLAMP_T *clamp,
			DISPMANX_TRANSFORM_T transform)
{
	return DISPMANX_NO_HANDLE;
}

static inline int
vc_dispmanx_element_change_source(DISPMANX_UPDATE_HANDLE_T update,
				  DISPMANX_ELEMENT_HANDLE_T element,
				  DISPMANX_RESOURCE_HANDLE_T src)
{
	return -1;
}

static inline int
vc_dispmanx_element_modified(DISPMANX_UPDATE_HANDLE_T update,
			     DISPMANX_ELEMENT_HANDLE_T element,
			     const VC_RECT_T *rect)
{
	return -1;
}

static inline int
vc_dispmanx_element_remove(DISPMANX_UPDATE_HANDLE_T update,
			   DISPMANX_ELEMENT_HANDLE_T element)
{
	return -1;
}

static inline int
vc_dispmanx_update_submit(DISPMANX_UPDATE_HANDLE_T update,
			  DISPMANX_CALLBACK_FUNC_T cb_func, void *cb_arg)
{
	return -1;
}

static inline int
vc_dispmanx_update_submit_sync(DISPMANX_UPDATE_HANDLE_T update)
{
	return -1;
}

static inline int
vc_dispmanx_element_change_attributes(DISPMANX_UPDATE_HANDLE_T update,
				      DISPMANX_ELEMENT_HANDLE_T element,
				      uint32_t change_flags,
				      int32_t layer,
				      uint8_t opacity,
				      const VC_RECT_T *dest_rect,
				      const VC_RECT_T *src_rect,
				      DISPMANX_RESOURCE_HANDLE_T mask,
				      VC_IMAGE_TRANSFORM_T transform)
{
	return -1;
}

static inline int
vc_dispmanx_snapshot(DISPMANX_DISPLAY_HANDLE_T display,
		     DISPMANX_RESOURCE_HANDLE_T snapshot_resource,
		     VC_IMAGE_TRANSFORM_T transform)
{
	return -1;
}

struct wl_resource;
static inline DISPMANX_RESOURCE_HANDLE_T
vc_dispmanx_get_handle_from_wl_buffer(struct wl_resource *_buffer)
{
	return DISPMANX_NO_HANDLE;
}

static inline void
vc_dispmanx_set_wl_buffer_in_use(struct wl_resource *_buffer, int in_use)
{
}

static inline int
vc_dispmanx_element_set_opaque_rect(DISPMANX_UPDATE_HANDLE_T update,
				    DISPMANX_ELEMENT_HANDLE_T element,
				    const VC_RECT_T *opaque_rect)
{
	return -1;
}

/* from /opt/vc/include/EGL/eglplatform.h */

typedef struct {
	DISPMANX_ELEMENT_HANDLE_T element;
	int width;
	int height;
} EGL_DISPMANX_WINDOW_T;

#endif /* RPI_BCM_STUBS */

//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_egl_ext.h : Defines the EGL extension entry points.

#ifndef LIBGLESV2_ENTRYPOINTSEGLEXT_H_
#define LIBGLESV2_ENTRYPOINTSEGLEXT_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <export.h>

namespace egl
{

// EGL_ANGLE_query_surface_pointer
ANGLE_EXPORT EGLBoolean EGLAPIENTRY QuerySurfacePointerANGLE(EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLint attribute,
                                                             void **value);

// EGL_NV_post_sub_buffer
ANGLE_EXPORT EGLBoolean EGLAPIENTRY PostSubBufferNV(EGLDisplay dpy,
                                                    EGLSurface surface,
                                                    EGLint x,
                                                    EGLint y,
                                                    EGLint width,
                                                    EGLint height);

// EGL_EXT_platform_base
ANGLE_EXPORT EGLDisplay EGLAPIENTRY GetPlatformDisplayEXT(EGLenum platform,
                                                          void *native_display,
                                                          const EGLint *attrib_list);
ANGLE_EXPORT EGLSurface EGLAPIENTRY CreatePlatformWindowSurfaceEXT(EGLDisplay dpy,
                                                                   EGLConfig config,
                                                                   void *native_window,
                                                                   const EGLint *attrib_list);
ANGLE_EXPORT EGLSurface EGLAPIENTRY CreatePlatformPixmapSurfaceEXT(EGLDisplay dpy,
                                                                   EGLConfig config,
                                                                   void *native_pixmap,
                                                                   const EGLint *attrib_list);

// EGL_EXT_device_query
ANGLE_EXPORT EGLBoolean EGLAPIENTRY QueryDisplayAttribEXT(EGLDisplay dpy,
                                                          EGLint attribute,
                                                          EGLAttrib *value);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY QueryDeviceAttribEXT(EGLDeviceEXT device,
                                                         EGLint attribute,
                                                         EGLAttrib *value);
ANGLE_EXPORT const char *EGLAPIENTRY QueryDeviceStringEXT(EGLDeviceEXT device, EGLint name);

// EGL_KHR_image_base/EGL_KHR_image
ANGLE_EXPORT EGLImageKHR EGLAPIENTRY CreateImageKHR(EGLDisplay dpy,
                                                    EGLContext ctx,
                                                    EGLenum target,
                                                    EGLClientBuffer buffer,
                                                    const EGLint *attrib_list);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY DestroyImageKHR(EGLDisplay dpy, EGLImageKHR image);

// EGL_EXT_device_creation
ANGLE_EXPORT EGLDeviceEXT EGLAPIENTRY CreateDeviceANGLE(EGLint device_type,
                                                        void *native_device,
                                                        const EGLAttrib *attrib_list);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY ReleaseDeviceANGLE(EGLDeviceEXT device);

// EGL_KHR_stream
ANGLE_EXPORT EGLStreamKHR EGLAPIENTRY CreateStreamKHR(EGLDisplay dpy, const EGLint *attrib_list);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY DestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY StreamAttribKHR(EGLDisplay dpy,
                                                    EGLStreamKHR stream,
                                                    EGLenum attribute,
                                                    EGLint value);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY QueryStreamKHR(EGLDisplay dpy,
                                                   EGLStreamKHR stream,
                                                   EGLenum attribute,
                                                   EGLint *value);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY QueryStreamu64KHR(EGLDisplay dpy,
                                                      EGLStreamKHR stream,
                                                      EGLenum attribute,
                                                      EGLuint64KHR *value);

// EGL_KHR_stream_consumer_gltexture
ANGLE_EXPORT EGLBoolean EGLAPIENTRY StreamConsumerGLTextureExternalKHR(EGLDisplay dpy,
                                                                       EGLStreamKHR stream);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY StreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY StreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY
StreamConsumerGLTextureExternalAttribsNV(EGLDisplay dpy,
                                         EGLStreamKHR stream,
                                         const EGLAttrib *attrib_list);

// EGL_ANGLE_stream_producer_d3d_texture
ANGLE_EXPORT EGLBoolean EGLAPIENTRY
CreateStreamProducerD3DTextureANGLE(EGLDisplay dpy,
                                    EGLStreamKHR stream,
                                    const EGLAttrib *attrib_list);
ANGLE_EXPORT EGLBoolean EGLAPIENTRY StreamPostD3DTextureANGLE(EGLDisplay dpy,
                                                              EGLStreamKHR stream,
                                                              void *texture,
                                                              const EGLAttrib *attrib_list);

// EGL_CHROMIUM_get_sync_values
ANGLE_EXPORT EGLBoolean EGLAPIENTRY GetSyncValuesCHROMIUM(EGLDisplay dpy,
                                                          EGLSurface surface,
                                                          EGLuint64KHR *ust,
                                                          EGLuint64KHR *msc,
                                                          EGLuint64KHR *sbc);

// EGL_KHR_swap_buffers_with_damage
ANGLE_EXPORT EGLBoolean EGLAPIENTRY SwapBuffersWithDamageKHR(EGLDisplay dpy,
                                                             EGLSurface surface,
                                                             EGLint *rects,
                                                             EGLint n_rects);

// EGL_ANDROID_presentation_time
ANGLE_EXPORT EGLBoolean EGLAPIENTRY PresentationTimeANDROID(EGLDisplay dpy,
                                                            EGLSurface surface,
                                                            EGLnsecsANDROID time);

// EGL_ANGLE_program_cache_control
ANGLE_EXPORT EGLint EGLAPIENTRY ProgramCacheGetAttribANGLE(EGLDisplay dpy, EGLenum attrib);
ANGLE_EXPORT void EGLAPIENTRY ProgramCacheQueryANGLE(EGLDisplay dpy,
                                                     EGLint index,
                                                     void *key,
                                                     EGLint *keysize,
                                                     void *binary,
                                                     EGLint *binarysize);
ANGLE_EXPORT void EGLAPIENTRY ProgramCachePopulateANGLE(EGLDisplay dpy,
                                                        const void *key,
                                                        EGLint keysize,
                                                        const void *binary,
                                                        EGLint binarysize);
ANGLE_EXPORT EGLint EGLAPIENTRY ProgramCacheResizeANGLE(EGLDisplay dpy, EGLint limit, EGLenum mode);

// EGL_KHR_debug
ANGLE_EXPORT EGLint EGLAPIENTRY DebugMessageControlKHR(EGLDEBUGPROCKHR callback,
                                                       const EGLAttrib *attrib_list);

ANGLE_EXPORT EGLBoolean EGLAPIENTRY QueryDebugKHR(EGLint attribute, EGLAttrib *value);

ANGLE_EXPORT EGLint EGLAPIENTRY LabelObjectKHR(EGLDisplay display,
                                               EGLenum objectType,
                                               EGLObjectKHR object,
                                               EGLLabelKHR label);

}  // namespace egl

#endif  // LIBGLESV2_ENTRYPOINTSEGLEXT_H_

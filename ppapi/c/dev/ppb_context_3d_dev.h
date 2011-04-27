/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_DEV_PPB_CONTEXT_3D_DEV_H_
#define PPAPI_C_DEV_PPB_CONTEXT_3D_DEV_H_

#include "ppapi/c/dev/pp_graphics_3d_dev.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

#define PPB_CONTEXT_3D_DEV_INTERFACE_0_1 "PPB_Context3D(Dev);0.1"
#define PPB_CONTEXT_3D_DEV_INTERFACE PPB_CONTEXT_3D_DEV_INTERFACE_0_1

struct PPB_Context3D_Dev {
  // Creates and initializes a rendering context and returns a handle to it.
  // The context can be used to render to any compatible PPB_Surface3D_Dev.
  //
  // If share_context is not NULL, then all shareable data, as defined
  // by the client API (note that for OpenGL and OpenGL ES, shareable data
  // excludes texture objects named 0) will be shared by share_context, all
  // other contexts share_context already shares with, and the newly created
  // context. An arbitrary number of PPB_Context3D_Dev can share data in
  // this fashion.
  //
  // attrib_list specifies a list of attributes for the context. The list
  // has the same structure as described for
  // PPB_Graphics3D_Dev::GetConfigAttribs. The only attribute that can be
  // specified in attrib_list is PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_VERSION,
  // and this attribute may only be specified when creating a OpenGL ES context.
  // attrib_list may be NULL or empty (first attribute is EGL_NONE), in which
  // case attributes assume their default values.
  //
  // If config is not a valid PP_Config3D_Dev, or does not support
  // the requested client API, then an PP_GRAPHICS3DERROR_BAD_CONFIG error is
  // generated (this includes requesting creation of an OpenGL ES 1.x context
  // when the PP_GRAPHICS3DATTRIB_RENDERABLE_TYPE attribute of config does not
  // contain PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES_BIT, or creation of an
  // OpenGL ES 2.x context when the attribute does not contain
  // PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES2_BIT).
  //
  // On failure Create returns NULL resource.
  PP_Resource (*Create)(PP_Instance instance,
                        PP_Config3D_Dev config,
                        PP_Resource share_context,
                        const int32_t* attrib_list);

  // Returns PP_TRUE if the given resource is a valid PPB_Context3D_Dev,
  // PP_FALSE if it is an invalid resource or is a resource of another type.
  PP_Bool (*IsContext3D)(PP_Resource resource);

  // Returns in value the value of attribute for context.
  // Attributes that can be queried for include:
  // - PP_GRAPHICS3DATTRIB_CONFIG_ID: returns the ID of the
  //   PP_Config3D_Dev with respect to which the context was created.
  // - PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_TYPE: returns the type of client API
  //   this context supports.
  // - PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_VERSION: returns the version of the
  //   client API this context supports, as specified at context creation time.
  // - PP_GRAPHICS3DATTRIB_RENDER_BUFFER: returns the buffer which client API
  //   rendering via this context will use. The value returned depends on
  //   properties of both the context, and the surface to which the context
  //   is bound:
  //   - If the context is bound to a surface, then either
  //     PP_GRAPHICS3DATTRIBVALUE_BACK_BUFFER or
  //     PP_GRAPHICS3DATTRIBVALUE_SINGLE_BUFFER may be returned. The value
  //     returned depends on the buffer requested by the setting of the
  //     PP_GRAPHICS3DATTRIB_RENDER_BUFFER property of the surface.
  //   - If the context is not bound to a surface, then
  //     PP_GRAPHICS3DATTRIBVALUE_NONE will be returned.
  //
  // On failure the following error codes may be returned:
  // - PP_GRAPHICS3DERROR_BAD_ATTRIBUTE if attribute is not a valid attribute
  // - PP_GRAPHICS3DERROR_BAD_CONTEXT if context is invalid.
  int32_t (*GetAttrib)(PP_Resource context,
                       int32_t attribute,
                       int32_t* value);

  // Binds context to the draw and read surfaces.
  // For an OpenGL or OpenGL ES context, draw is used for all operations except
  // for any pixel data read back or copied, which is taken from the frame
  // buffer values of read. Note that the same PPB_Surface3D_Dev may be
  // specified for both draw and read.
  //
  // On failure the following error codes may be returned:
  // - PP_GRAPHICS3DERROR_BAD_MATCH: if draw or read surfaces are not
  //   compatible with context.
  // - PP_GRAPHICS3DERROR_BAD_ACCESS: if either draw or read is bound to any
  //   other context.
  // - PP_GRAPHICS3DERROR_BAD_CONTEXT: if context is not a valid context.
  // - PP_GRAPHICS3DERROR_BAD_SURFACE: if either draw or read are not valid
  //   surfaces.
  // - PP_GRAPHICS3DERROR_BAD_MATCH: if draw and read cannot fit into
  //   graphics memory simultaneously.
  // - PP_ERROR_NOMEMORY: if the ancillary buffers for draw and read cannot
  //   be allocated.
  // - PP_GRAPHICS3DERROR_CONTEXT_LOST: if a power management event has
  //   occurred.
  //
  // If draw is destroyed after BindSurfaces is called, then subsequent
  // rendering commands will be processed and the context state will be updated,
  // but the surface contents become undefined. If read is destroyed after
  // BindSurfaces then pixel values read from the framebuffer (e.g., as result
  // of calling glReadPixels) are undefined.
  //
  // To unbind surfaces set draw and read to NULL.
  int32_t (*BindSurfaces)(PP_Resource context,
                          PP_Resource draw,
                          PP_Resource read);

  // Returns the surfaces bound to the context for drawing and reading in
  // draw and read respectively.
  //
  // On failure, the following error codes can be returned:
  // - PP_GRAPHICS3DERROR_BAD_CONTEXT: if context is not a valid context.
  // - PP_ERROR_BADARGUMENT: if either draw or read is NULL.
  int32_t (*GetBoundSurfaces)(PP_Resource context,
                              PP_Resource* draw,
                              PP_Resource* read);
};

#endif  // PPAPI_C_DEV_PPB_CONTEXT_3D_DEV_H_

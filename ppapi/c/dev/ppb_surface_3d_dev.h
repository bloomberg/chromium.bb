/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_DEV_PPB_SURFACE_3D_DEV_H_
#define PPAPI_C_DEV_PPB_SURFACE_3D_DEV_H_

#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

#define PPB_SURFACE_3D_DEV_INTERFACE "PPB_Surface3D(Dev);0.1"

struct PPB_Surface3D_Dev {
  // Creates a render surface and returns a handle to it.
  // Any PPB_Context3D_Dev created with a compatible PP_Config3D_Dev
  // can be used to render into this surface. The returned surface is
  // off-screen to start with. It must be attached to a plugin instance
  // using PPB_Instance::BindGraphics to draw on the web page.
  //
  // attrib_list specifies a list of attributes for the surface. The list has
  // the same structure as described for PPB_Graphics3D_Dev::GetConfigAttribs.
  // Attributes that can be specified in attrib_list include:
  // - PP_GRAPHICS3DATTRIB_WIDTH
  // - PP_GRAPHICS3DATTRIB_HEIGHT
  // - PP_GRAPHICS3DATTRIB_LARGEST_SURFACE: If true, creates the largest
  //   possible surface when the allocation of the surface would otherwise fail.
  //   The width and height of the allocated surface will never exceed the
  //   values of PP_GRAPHICS3DATTRIB_WIDTH and PP_GRAPHICS3DATTRIB_HEIGHT,
  //   respectively. If this option is used, PPB_Surface3D_Dev::GetAttrib
  //   can be used to retrieve surface dimensions.
  // - PP_GRAPHICS3DATTRIB_RENDER_BUFFER
  PP_Resource (*Create)(PP_Instance instance,
                        PP_Config3D_Dev config,
                        const int32_t* attrib_list);

  // Returns PP_TRUE if the given resource is a valid Surface3D, PP_FALSE if it
  // is an invalid resource or is a resource of another type.
  PP_Bool (*IsSurface3D)(PP_Resource resource);

  // Sets an attribute for PPB_Surface3D_Dev. The specified attribute of
  // surface is set to value. Attributes that can be specified are:
  // - PP_GRAPHICS3DATTRIB_MULTISAMPLE_RESOLVE: If value
  //   is PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_BOX, and the
  //   PP_GRAPHICS3DATTRIB_SURFACE_TYPE attribute used to create surface does
  //   not contain PP_GRAPHICS3DATTRIBVALUE_MULTISAMPLE_RESOLVE_BOX_BIT, a
  //   PP_GRAPHICS3DERROR_BAD_MATCH error is returned.
  // - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR: If value is
  //   PP_GRAPHICS3DATTRIBVALUE_BUFFER_PRESERVED, and the
  //   PP_GRAPHICS3DATTRIB_SURFACE_TYPE attribute used to create surface
  //   does not contain PP_GRAPHICS3DATTRIBVALUE_SWAP_BEHAVIOR_PRESERVED_BIT,
  //   a PP_GRAPHICS3DERROR_BAD_MATCH error is returned.
  int32_t (*SetAttrib)(PP_Resource surface,
                       int32_t attribute,
                       int32_t value);

  // Retrieves the value of attribute for surface. Attributes that can be
  // queried for are:
  // - PP_GRAPHICS3DATTRIB_CONFIG_ID: returns the ID of the
  //   PP_Config3D_Dev with respect to which the surface was created.
  // - PP_GRAPHICS3DATTRIB_LARGEST_SURFACE: returns the same attribute value
  //   specified when the surface was created with PPB_Surface3D_Dev::Create.
  // - PP_GRAPHICS3DATTRIB_WIDTH and PP_GRAPHICS3DATTRIB_HEIGHT: The returned
  //   size may be less than the requested size if
  //   PP_GRAPHICS3DATTRIB_LARGEST_SURFACE is true.
  // - PP_GRAPHICS3DATTRIB_RENDER_BUFFER
  // - PP_GRAPHICS3DATTRIB_MULTISAMPLE_RESOLVE
  // - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR
  //
  // If attribute is not a valid PPB_Surface3D_Dev surface attribute,
  // then an PP_GRAPHICS3DERROR_BAD_ATTRIBUTE error is returned. If surface
  // is not a valid PPB_Surface3D_Dev then an PP_GRAPHICS3DERROR_BAD_SURFACE
  // error is returned.
  int32_t (*GetAttrib)(PP_Resource surface,
                       int32_t attribute,
                       int32_t* value);

  // Makes the contents of the color buffer available for compositing.
  // This function has no effect on off-screen surfaces - ones not bound
  // to any plugin instance. The contents of ancillary buffers are always
  // undefined after calling SwapBuffers. The contents of the color buffer are
  // undefined if the value of the PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR attribute
  // of surface is not PP_GRAPHICS3DATTRIBVALUE_BUFFER_PRESERVED.
  //
  // If surface is bound as the draw surface of a context then SwapBuffers
  // performs an implicit flush operation on the context.
  //
  // This functions can run in two modes:
  // - In synchronous mode, you specify NULL for the callback and the callback
  //   data. This function will block the calling thread until the image has
  //   been painted to the screen. It is not legal to block the main thread of
  //   the plugin, you can use synchronous mode only from background threads.
  // - In asynchronous mode, you specify a callback function and the argument
  //   for that callback function. The callback function will be executed on
  //   the calling thread when the image has been painted to the screen. While
  //   you are waiting for a Flush callback, additional calls to Flush will
  //   fail.
  //
  // Because the callback is executed (or thread unblocked) only when the
  // plugin's current state is actually on the screen, this function provides a
  // way to rate limit animations. By waiting until the image is on the screen
  // before painting the next frame, you can ensure you're not generating
  // updates faster than the screen can be updated.
  //
  int32_t (*SwapBuffers)(PP_Resource surface,
                         struct PP_CompletionCallback callback);
};

#endif  // PPAPI_C_DEV_PPB_SURFACE_3D_DEV_H_

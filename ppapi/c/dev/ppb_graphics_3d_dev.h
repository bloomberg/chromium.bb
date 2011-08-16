/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_
#define PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_

#include "ppapi/c/dev/pp_graphics_3d_dev.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

// Example usage from plugin code:
//
// // Setup.
// PP_Resource context;
// int32_t attribs[] = {PP_GRAPHICS3DATTRIB_WIDTH, 800,
//                      PP_GRAPHICS3DATTRIB_HEIGHT, 800,
//                      PP_GRAPHICS3DATTRIB_NONE};
// context = g3d->Create(instance, attribs, &context);
// inst->BindGraphics(instance, context);
//
// // Present one frame.
// gles2->Clear(context, GL_COLOR_BUFFER);
// g3d->SwapBuffers(context);
//
// // Shutdown.
// core->ReleaseResource(context);

#define PPB_GRAPHICS_3D_DEV_INTERFACE_0_7 "PPB_Graphics3D(Dev);0.7"
#define PPB_GRAPHICS_3D_DEV_INTERFACE PPB_GRAPHICS_3D_DEV_INTERFACE_0_7

struct PPB_Graphics3D_Dev {
  // Returns a string describing some aspect of the Graphics3D implementation.
  // name may be one of:
  // - PP_GRAPHICS3DSTRING_EXTENSIONS: describes which extensions are supported
  //   by the implementation. The string is zero-terminated and contains a
  //   space-separated list of extension names; extension names themselves do
  //   not contain spaces. If there are no extensions, then the empty string is
  //   returned.
  // - PP_GRAPHICS3DSTRING_VENDOR: Implementation dependent.
  // - PP_GRAPHICS3DSTRING_VERSION: The format of the string is:
  //   <major version.minor version><space><vendor specific info>
  //   Both the major and minor portions of the version number are numeric.
  //   The vendor-specific information is optional; if present, its format and
  //   contents are implementation specific.
  // On failure, PP_VARTYPE_UNDEFINED is returned.
  //
  // TODO(alokp): Does this function need module argument?
  //
  struct PP_Var (*GetString)(int32_t name);

  // Creates and initializes a rendering context and returns a handle to it.
  // The returned context is off-screen to start with. It must be attached to
  // a plugin instance using PPB_Instance::BindGraphics to draw on the web page.
  //
  // If share_context is not NULL, then all shareable data, as defined
  // by the client API (note that for OpenGL and OpenGL ES, shareable data
  // excludes texture objects named 0) will be shared by share_context, all
  // other contexts share_context already shares with, and the newly created
  // context. An arbitrary number of PPB_Graphics3D_Dev can share data in
  // this fashion.
  //
  // attrib_list specifies a list of attributes for the context. It is a list
  // of attribute name-value pairs in which each attribute is immediately
  // followed by the corresponding desired value. The list is terminated with
  // PP_GRAPHICS3DATTRIB_NONE. The attrib_list may be NULL or empty
  // (first attribute is PP_GRAPHICS3DATTRIB_NONE). If an attribute is not
  // specified in attrib_list, then the default value is used (it is said to
  // be specified implicitly).
  //
  // Attributes for the context are chosen according to an attribute-specific
  // criteria. Attributes can be classified into two categories:
  // - AtLeast: The attribute value in the returned context meets or exceeds
  //            the value specified in attrib_list.
  // - Exact: The attribute value in the returned context is equal to
  //          the value specified in attrib_list.
  //
  // Attributes that can be specified in attrib_list include:
  // - PP_GRAPHICS3DATTRIB_ALPHA_SIZE: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_BLUE_SIZE: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_GREEN_SIZE: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_RED_SIZE: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_DEPTH_SIZE: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_STENCIL_SIZE: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_SAMPLES: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS: Category: AtLeast Default: 0.
  // - PP_GRAPHICS3DATTRIB_WIDTH: Category: Exact Default: 0.
  // - PP_GRAPHICS3DATTRIB_HEIGHT: Category: Exact Default: 0.
  // - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR:
  //   Category: Exact Default: Implementation defined.
  //
  // On failure NULL resource is returned.
  PP_Resource (*Create)(PP_Instance instance,
                        PP_Resource share_context,
                        const int32_t* attrib_list);

  // Returns PP_TRUE if the given resource is a valid PPB_Graphics3D_Dev,
  // PP_FALSE if it is an invalid resource or is a resource of another type.
  PP_Bool (*IsGraphics3D)(PP_Resource resource);

  // Retrieves the values for each attribute in attrib_list. The list
  // has the same structure as described for PPB_Graphics3D_Dev::Create.
  // It is both input and output structure for this function.
  //
  // All attributes specified in PPB_Graphics3D_Dev::Create can be queried for.
  // On failure the following error codes may be returned:
  // - PP_ERROR_BADRESOURCE if context is invalid.
  // - PP_GRAPHICS3DERROR_BAD_ATTRIBUTE if any attribute in the attrib_list
  //   is not a valid attribute
  //
  // Example usage: To get the values for rgb bits in the color buffer,
  // this function must be called as following:
  // int attrib_list[] = {PP_GRAPHICS3DATTRIB_RED_SIZE, 0,
  //                      PP_GRAPHICS3DATTRIB_GREEN_SIZE, 0,
  //                      PP_GRAPHICS3DATTRIB_BLUE_SIZE, 0,
  //                      PP_GRAPHICS3DATTRIB_NONE};
  // GetAttribs(context, attrib_list);
  // int red_bits = attrib_list[1];
  // int green_bits = attrib_list[3];
  // int blue_bits = attrib_list[5];
  int32_t (*GetAttribs)(PP_Resource context, int32_t* attrib_list);

  // Sets the values for each attribute in attrib_list. The list
  // has the same structure as described for PPB_Graphics3D_Dev::Create.
  //
  // Attributes that can be specified are:
  // - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR
  int32_t (*SetAttribs)(PP_Resource context, int32_t* attrib_list);

  // Resizes the backing surface for context.
  //
  // On failure the following error codes may be returned:
  // - PP_ERROR_BADRESOURCE if context is invalid.
  // - PP_ERROR_BADARGUMENT if the value specified for width or height
  //   is less than zero.
  //
  // If the surface could not be resized due to insufficient resources,
  // PP_ERROR_NOMEMORY error is returned on the next SwapBuffers callback.
  int32_t (*ResizeBuffers)(PP_Resource context,
                           int32_t width, int32_t height);

  // Makes the contents of the color buffer available for compositing.
  // This function has no effect on off-screen surfaces - ones not bound
  // to any plugin instance. The contents of ancillary buffers are always
  // undefined after calling SwapBuffers. The contents of the color buffer are
  // undefined if the value of the PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR attribute
  // of context is not PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED.
  //
  // SwapBuffers performs an implicit flush operation on context.
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
  int32_t (*SwapBuffers)(PP_Resource context,
                         struct PP_CompletionCallback callback);
};

#endif  /* PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_ */

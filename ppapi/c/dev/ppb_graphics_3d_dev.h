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
// int32_t config, num_config;
// g3d->GetConfigs(&config, 1, &num_config);
// int32_t attribs[] = {PP_GRAPHICS_3D_SURFACE_WIDTH, 800,
//                      PP_GRAPHICS_3D_SURFACE_HEIGHT, 800,
//                      PP_GRAPHICS_3D_ATTRIB_NONE};
// context = g3d->Create(instance, config, attribs, &context);
// inst->BindGraphics(instance, context);
//
// // Present one frame.
// gles2->Clear(context, GL_COLOR_BUFFER);
// g3d->SwapBuffers(context);
//
// // Shutdown.
// core->ReleaseResource(context);

#define PPB_GRAPHICS_3D_DEV_INTERFACE_0_6 "PPB_Graphics3D(Dev);0.6"
#define PPB_GRAPHICS_3D_DEV_INTERFACE PPB_GRAPHICS_3D_DEV_INTERFACE_0_6

struct PPB_Graphics3D_Dev {
  // TODO(alokp): Do these functions need module argument?

  // Retrieves the list of all available PP_Config3D_Devs.
  // configs is a pointer to a buffer containing config_size elements.
  // On success, PP_OK is returned. The number of configurations is returned
  // in num_config, and elements 0 through num_config - 1 of configs are filled
  // in with valid PP_Config3D_Devs. No more than config_size
  // PP_Config3D_Devs will be returned even if more are available.
  // However, if GetConfigs is called with configs = NULL, then no
  // configurations are returned, but the total number of configurations
  // available will be returned in num_config.
  //
  // On failure following error codes are returned:
  // PP_ERROR_BADARGUMENT if num_config is NULL.
  // PP_ERROR_FAILED for everything else.
  int32_t (*GetConfigs)(PP_Config3D_Dev* configs,
                        int32_t config_size,
                        int32_t* num_config);

  // Retrieves the values for each attribute in attrib_list.
  // attrib_list is a list of attribute name-value pairs terminated with
  // PP_GRAPHICS3DCONFIGATTRIB_NONE. It is both input and output structure
  // for this function.
  //
  // On success PP_OK is returned and attrib_list is populated with
  // values of the attributes specified in attrib_list.
  // On failure following error codes are returned:
  // PP_GRAPHICS3DERROR_BAD_CONFIG if config is not valid
  // PP_ERROR_BADARGUMENT if attrib_list is NULL or malformed
  // PP_GRAPHICS3DERROR_BAD_ATTRIBUTE if any of the attributes in the
  //   attrib_list is not recognized.
  //
  // Example usage: To get the values for rgb bits in the color buffer,
  // this function must be called as following:
  // int attrib_list[] = {PP_GRAPHICS3DCONFIGATTRIB_RED_SIZE, 0,
  //                      PP_GRAPHICS3DCONFIGATTRIB_GREEN_SIZE, 0,
  //                      PP_GRAPHICS3DCONFIGATTRIB_BLUE_SIZE, 0,
  //                      PP_GRAPHICS3DCONFIGATTRIB_NONE};
  // GetConfigAttribs(config, attrib_list);
  // int red_bits = attrib_list[1];
  // int green_bits = attrib_list[3];
  // int blue_bits = attrib_list[5];
  int32_t (*GetConfigAttribs)(PP_Config3D_Dev config,
                              int32_t* attrib_list);

  // Returns a string describing some aspect of the Graphics3D implementation.
  // name may be one of:
  // - PP_GRAPHICS3DSTRING_CLIENT_APIS: describes which client rendering APIs
  //   are supported. It is zero-terminated and contains a space-separated list
  //   of API names, which must include at least one of "OpenGL" or "OpenGL_ES".
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
  struct PP_Var (*GetString)(int32_t name);

  // Creates and initializes a rendering context and returns a handle to it.
  // The returned context is off-screen to start with. It must be attached to
  // a plugin instance using PPB_Instance::BindGraphics to draw on the web page.
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
  // PPB_Graphics3D_Dev::GetConfigAttribs. attrib_list may be NULL or empty
  // (first attribute is PP_GRAPHICS_3D_ATTRIB_NONE), in which case attributes
  // assume their default values.
  // Attributes that can be specified in attrib_list include:
  // - PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_VERSION: may only be specified when
  //   creating a OpenGL ES context.
  // - PP_GRAPHICS3DATTRIB_WIDTH: The default value is zero.
  // - PP_GRAPHICS3DATTRIB_HEIGHT: The default value is zero.
  // - PP_GRAPHICS3DATTRIB_LARGEST_SURFACE: If true, creates the largest
  //   possible surface when the allocation of the surface would otherwise fail.
  //   The width and height of the allocated surface will never exceed the
  //   values of PP_GRAPHICS3DATTRIB_WIDTH and PP_GRAPHICS3DATTRIB_HEIGHT,
  //   respectively. If this option is used, PPB_Graphics3D_Dev::GetAttrib
  //   can be used to retrieve surface dimensions.
  // - PP_GRAPHICS3DATTRIB_RENDER_BUFFER
  //
  // It will fail to create a context if config is not a valid PP_Config3D_Dev,
  // or does not support the requested client API (this includes requesting
  // creation of an OpenGL ES 1.x context when the
  // PP_GRAPHICS3DATTRIB_RENDERABLE_TYPE attribute of config does not
  // contain PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES_BIT, or creation of an
  // OpenGL ES 2.x context when the attribute does not contain
  // PP_GRAPHICS3DATTRIBVALUE_OPENGL_ES2_BIT).
  //
  // On failure Create returns NULL resource.
  PP_Resource (*Create)(PP_Instance instance,
                        PP_Config3D_Dev config,
                        PP_Resource share_context,
                        const int32_t* attrib_list);

  // Returns PP_TRUE if the given resource is a valid PPB_Graphics3D_Dev,
  // PP_FALSE if it is an invalid resource or is a resource of another type.
  PP_Bool (*IsGraphics3D)(PP_Resource resource);

  // Retrieves the values for each attribute in attrib_list. The list
  // has the same structure as described for
  // PPB_Graphics3D_Dev::GetConfigAttribs.
  //
  // Attributes that can be queried for include:
  // - PP_GRAPHICS3DATTRIB_CONFIG_ID: returns the ID of the
  //   PP_Config3D_Dev with respect to which the context was created.
  // - PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_TYPE: returns the type of client API
  //   this context supports.
  // - PP_GRAPHICS3DATTRIB_CONTEXT_CLIENT_VERSION: returns the version of the
  //   client API this context supports, as specified at context creation time.
  // - PP_GRAPHICS3DATTRIB_RENDER_BUFFER: returns the buffer which client API
  //   rendering via this context will use. Either
  //   PP_GRAPHICS3DATTRIBVALUE_BACK_BUFFER or
  //   PP_GRAPHICS3DATTRIBVALUE_SINGLE_BUFFER may be returned depending on the
  //   buffer requested by the setting of the PP_GRAPHICS3DATTRIB_RENDER_BUFFER
  //   property of the context.
  // - PP_GRAPHICS3DATTRIB_LARGEST_SURFACE: returns the same attribute value
  //   specified when the context was created with PPB_Graphics3D_Dev::Create.
  // - PP_GRAPHICS3DATTRIB_WIDTH and PP_GRAPHICS3DATTRIB_HEIGHT: The returned
  //   size may be less than the requested size if
  //   PP_GRAPHICS3DATTRIB_LARGEST_SURFACE is true.
  // - PP_GRAPHICS3DATTRIB_MULTISAMPLE_RESOLVE
  // - PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR
  //
  // On failure the following error codes may be returned:
  // - PP_ERROR_BADRESOURCE if context is invalid.
  // - PP_GRAPHICS3DERROR_BAD_ATTRIBUTE if any attribute in the attrib_list
  //   is not a valid attribute
  int32_t (*GetAttribs)(PP_Resource context,
                        int32_t* attrib_list);

  // Sets the values for each attribute in attrib_list. The list
  // has the same structure as described for
  // PPB_Graphics3D_Dev::GetConfigAttribs.
  //
  // Attributes that can be specified are:
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
  int32_t (*SetAttribs)(PP_Resource context,
                        int32_t* attrib_list);

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
  // of context is not PP_GRAPHICS3DATTRIBVALUE_BUFFER_PRESERVED.
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

/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_
#define PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_

#include "ppapi/c/dev/pp_graphics_3d_dev.h"

// Example usage from plugin code:
//
// // Setup.
// PP_Resource context, surface;
// int32_t config, num_config;
// g3d->GetConfigs(&config, 1, &num_config);
// int32_t attribs[] = {PP_GRAPHICS_3D_SURFACE_WIDTH, 800,
//                      PP_GRAPHICS_3D_SURFACE_HEIGHT, 800,
//                      PP_GRAPHICS_3D_ATTRIB_NONE};
// c3d->Create(instance, config, NULL, NULL, &context);
// s3d->Create(instance, config, attribs, &surface);
// c3d->BindSurfaces(context, surface, surface);
// inst->BindGraphics(instance, surface);
//
// // Present one frame.
// gles2->Clear(context, GL_COLOR_BUFFER);
// c3d->SwapBuffers(context);
//
// // Shutdown.
// core->ReleaseResource(context);
// core->ReleaseResource(surface);

#define PPB_GRAPHICS_3D_DEV_INTERFACE "PPB_Graphics3D(Dev);0.3"

struct PPB_Graphics3D_Dev {
  // TODO(alokp): Do these functions need module argument.

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
};

#endif  /* PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_ */


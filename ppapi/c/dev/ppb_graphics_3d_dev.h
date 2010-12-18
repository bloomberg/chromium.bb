// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_
#define PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

// Example usage from plugin code:
//
// PP_Resource context = device->Create(module, config, contextAttribList);
// CHECK(context);
//
// // Present one frame.
// CHECK(device->MakeCurrent(context));
// glClear(GL_COLOR_BUFFER);
// CHECK(device->MakeCurrent(NULL));
// CHECK(device->SwapBuffers(context));
//
// // Shutdown.
// core->ReleaseResource(context);

#define PPB_GRAPHICS_3D_DEV_INTERFACE "PPB_Graphics3D(Dev);0.2"

// These are the same error codes as used by EGL.
enum {
  PP_GRAPHICS_3D_ERROR_SUCCESS         = 0x3000,
  PP_GRAPHICS_3D_ERROR_NOT_INITIALIZED = 0x3001,
  PP_GRAOHICS_3D_ERROR_BAD_CONTEXT     = 0x3006,
  PP_GRAPHICS_3D_ERROR_BAD_PARAMETER   = 0x300C,
  PP_GRAPHICS_3D_ERROR_CONTEXT_LOST    = 0x300E
};

// QueryString targets, matching EGL ones.
enum {
  EGL_VENDOR      = 0x3053,
  EGL_VERSION     = 0x3054,
  EGL_EXTENSIONS  = 0x3055,
  EGL_CLIENT_APIS = 0x308D
};

struct PPB_Graphics3D_Dev {
  PP_Bool (*IsGraphics3D)(PP_Resource resource);

  // EGL-like configuration ----------------------------------------------------
  PP_Bool (*GetConfigs)(int32_t* configs,
                        int32_t config_size,
                        int32_t* num_config);

  PP_Bool (*ChooseConfig)(const int32_t* attrib_list,
                          int32_t* configs,
                          int32_t config_size,
                          int32_t* num_config);

  // TODO(apatrick): What to do if the browser window is moved to
  // another display? Do the configs potentially change?
  PP_Bool (*GetConfigAttrib)(int32_t config, int32_t attribute, int32_t* value);

  const char* (*QueryString)(int32_t name);
  // ---------------------------------------------------------------------------


  // Create a reference counted 3D context. Releasing a context while it is
  // current automatically sets the current context to NULL. This is only true
  // for the releasing thread. Releasing a context while it is current on
  // another thread leads to undefined behavior.
  PP_Resource (*CreateContext)(PP_Instance instance,
                               int32_t config,
                               int32_t share_context,
                               const int32_t* attrib_list);

  // Get the address of any GL functions, whether core or part of an extension.
  // Any thread.
  void* (*GetProcAddress)(const char* name);

  // Make a particular context current of the calling thread.  Returns PP_TRUE
  // on success, PP_FALSE on failure.
  PP_Bool (*MakeCurent)(PP_Resource context);

  // Returns the calling thread's current context or NULL if no context is
  // current.
  PP_Resource (*GetCurrentContext)();

  // Snapshots the rendered frame and makes it available for composition with
  // the rest of the page. The alpha channel is used for translucency effects.
  // One means fully opaque. Zero means fully transparent. Any thread.
  // TODO(apatrick): premultiplied alpha or linear alpha? Premultiplied alpha is
  // better for correct alpha blending effect. Most existing OpenGL code assumes
  // linear. I could convert from linear to premultiplied during the copy from
  // back-buffer to offscreen "front-buffer".
  PP_Bool (*SwapBuffers)(PP_Resource context);

  // Returns the current error for this thread. This is not associated with a
  // particular context. It is distinct from the GL error returned by
  // glGetError.
  uint32_t (*GetError)();
};

#endif  // PPAPI_C_DEV_PPB_GRAPHICS_3D_DEV_H_

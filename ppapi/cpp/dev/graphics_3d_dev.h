// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_
#define PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_

#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_opengles_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Graphics3D_Dev : public Resource {
 public:
  static bool GetConfigs(int32_t* configs, int32_t config_size,
                         int32_t* num_config);

  static bool ChooseConfig(const int32_t* attrib_list, int32_t* configs,
                           int32_t config_size, int32_t* num_config);

  static bool GetConfigAttrib(int32_t config, int32_t attribute,
                              int32_t* value);

  static const char* QueryString(int32_t name);

  static void* GetProcAddress(const char* name);

  static bool ResetCurrent();
  static Graphics3D_Dev GetCurrentContext();
  static uint32_t GetError();
  static const PPB_OpenGLES_Dev* GetImplementation();

  // Creates an is_null() Graphics3D object.
  Graphics3D_Dev() {}

  Graphics3D_Dev(const Instance& instance,
                 int32_t config,
                 int32_t share_context,
                 const int32_t* attrib_list);

  bool MakeCurrent() const;
  bool SwapBuffers() const;

 protected:
  explicit Graphics3D_Dev(PP_Resource resource_id) : Resource(resource_id) {}
  static Graphics3D_Dev FromResource(PP_Resource resource_id);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_


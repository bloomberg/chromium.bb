// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_
#define PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_

#include "ppapi/c/dev/ppb_graphics_3d_dev.h"

namespace pp {

class Var;

class Graphics3D_Dev {
 public:
  static int32_t GetConfigs(PP_Config3D_Dev* configs,
                            int32_t config_size,
                            int32_t* num_config);

  static int32_t GetConfigAttribs(PP_Config3D_Dev config,
                                  int32_t* attrib_list);

  static Var GetString(int32_t name);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_3D_DEV_H_


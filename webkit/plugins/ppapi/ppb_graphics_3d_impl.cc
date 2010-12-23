// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"

namespace webkit {
namespace ppapi {

namespace {

int32_t GetConfigs(PP_Config3D_Dev* configs,
                   int32_t config_size,
                   int32_t* num_config) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

int32_t GetConfigAttribs(PP_Config3D_Dev config, int32_t* attrib_list) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

PP_Var GetString(int32_t name) {
  // TODO(alokp): Implement me.
  return PP_MakeUndefined();
}

const PPB_Graphics3D_Dev ppb_graphics3d = {
  &GetConfigs,
  &GetConfigAttribs,
  &GetString
};

}  // namespace

const PPB_Graphics3D_Dev* PPB_Graphics3D_Impl::GetInterface() {
  return &ppb_graphics3d;
}

}  // namespace ppapi
}  // namespace webkit


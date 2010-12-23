// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/graphics_3d_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics3D_Dev>() {
  return PPB_GRAPHICS_3D_DEV_INTERFACE;
}

}  // namespace

// static
int32_t Graphics3D_Dev::GetConfigs(int32_t *configs,
                                   int32_t config_size,
                                   int32_t *num_config) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->GetConfigs(
      configs, config_size, num_config);
}

// static
int32_t Graphics3D_Dev::GetConfigAttribs(PP_Config3D_Dev config,
                                         int32_t* attrib_list) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->GetConfigAttribs(
      config, attrib_list);
}

// static
Var Graphics3D_Dev::GetString(int32_t name) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return Var();

  return Var(Var::PassRef(),
             get_interface<PPB_Graphics3D_Dev>()->GetString(name));
}

}  // namespace pp


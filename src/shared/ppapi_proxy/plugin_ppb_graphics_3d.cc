// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_graphics_3d.h"

#include <stdio.h>
#include <string.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

namespace {

int32_t GetConfigs(PP_Config3D_Dev* configs,
                   int32_t config_size,
                   int32_t* num_config) {
  UNREFERENCED_PARAMETER(configs);
  UNREFERENCED_PARAMETER(config_size);
  UNREFERENCED_PARAMETER(num_config);
  return PP_ERROR_FAILED;
}

int32_t GetConfigAttribs(PP_Config3D_Dev config,
                         int32_t* attrib_list) {
  UNREFERENCED_PARAMETER(config);
  UNREFERENCED_PARAMETER(attrib_list);
  return PP_ERROR_FAILED;
}

PP_Var GetString(int32_t name) {
  UNREFERENCED_PARAMETER(name);
  struct PP_Var result = { PP_VARTYPE_UNDEFINED, 0, {PP_FALSE} };
  return result;
}

}  // namespace

const PPB_Graphics3D_Dev* PluginGraphics3D::GetInterface() {
  static const PPB_Graphics3D_Dev graphics_3d_interface = {
    GetConfigs,
    GetConfigAttribs,
    GetString,
  };
  return &graphics_3d_interface;
}

}  // namespace ppapi_proxy

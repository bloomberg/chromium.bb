/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_graphics_3d.h"

#include <stdio.h>
#include <string.h>
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/dev/ppb_graphics_3d_dev.h"

namespace ppapi_proxy {

namespace {
bool IsGraphics3D(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return false;
}

bool GetConfigs(int32_t* configs,
                int32_t config_size,
                int32_t* num_config) {
  UNREFERENCED_PARAMETER(configs);
  UNREFERENCED_PARAMETER(config_size);
  UNREFERENCED_PARAMETER(num_config);
  return false;
}

bool ChooseConfig(const int32_t* attrib_list,
                  int32_t* configs,
                  int32_t config_size,
                  int32_t* num_config) {
  UNREFERENCED_PARAMETER(attrib_list);
  UNREFERENCED_PARAMETER(configs);
  UNREFERENCED_PARAMETER(config_size);
  UNREFERENCED_PARAMETER(num_config);
  return false;
}

bool GetConfigAttrib(int32_t config, int32_t attribute, int32_t* value) {
  UNREFERENCED_PARAMETER(config);
  UNREFERENCED_PARAMETER(attribute);
  UNREFERENCED_PARAMETER(value);
  return false;
}

const char* QueryString(int32_t name) {
  UNREFERENCED_PARAMETER(name);
  return NULL;
}

PP_Resource CreateContext(PP_Instance instance,
                          int32_t config,
                          int32_t share_context,
                          const int32_t* attrib_list) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(config);
  UNREFERENCED_PARAMETER(share_context);
  UNREFERENCED_PARAMETER(attrib_list);
  return kInvalidResourceId;
}

void* GetProcAddress(const char* name) {
  UNREFERENCED_PARAMETER(name);
  return NULL;
}

bool MakeCurrent(PP_Resource context) {
  UNREFERENCED_PARAMETER(context);
  return false;
}

PP_Resource GetCurrentContext() {
  return kInvalidResourceId;
}

bool SwapBuffers(PP_Resource context) {
  UNREFERENCED_PARAMETER(context);
  return false;
}

uint32_t GetError() {
  return 0;
}
}  // namespace

const PPB_Graphics3D_Dev* PluginGraphics3D::GetInterface() {
  static const PPB_Graphics3D_Dev intf = {
    IsGraphics3D,
    GetConfigs,
    ChooseConfig,
    GetConfigAttrib,
    QueryString,
    CreateContext,
    GetProcAddress,
    MakeCurrent,
    GetCurrentContext,
    SwapBuffers,
    GetError,
  };
  return &intf;
}

}  // namespace ppapi_proxy

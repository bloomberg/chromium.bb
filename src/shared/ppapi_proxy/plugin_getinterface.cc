/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_getinterface.h"
#include <stdlib.h>
#include <string.h>
#include "native_client/src/shared/ppapi_proxy/plugin_audio.h"
#include "native_client/src/shared/ppapi_proxy/plugin_audio_config.h"
#include "native_client/src/shared/ppapi_proxy/plugin_buffer.h"
#include "native_client/src/shared/ppapi_proxy/plugin_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_graphics_2d.h"
#include "native_client/src/shared/ppapi_proxy/plugin_graphics_3d.h"
#include "native_client/src/shared/ppapi_proxy/plugin_image_data.h"
#include "native_client/src/shared/ppapi_proxy/plugin_url_loader.h"
#include "native_client/src/shared/ppapi_proxy/plugin_url_request_info.h"
#include "native_client/src/shared/ppapi_proxy/plugin_url_response_info.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_core.h"

namespace ppapi_proxy {

namespace {
// TODO(sehr): Each interface struct pointer getter should ask the browser
// whether it supports its respective interface before using it.

typedef const void* (*GetInterfacePtr)();
struct InterfaceMapElement {
  const char* name;
  GetInterfacePtr func;
};

const InterfaceMapElement interface_map[] = {
  { PPB_AUDIO_DEV_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginAudio::GetInterface) },
  { PPB_AUDIO_CONFIG_DEV_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginAudioConfig::GetInterface) },
  { PPB_BUFFER_DEV_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginBuffer::GetInterface) },
  { PPB_CORE_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginCore::GetInterface) },
  { PPB_GRAPHICS_2D_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginGraphics2D::GetInterface) },
  { PPB_GRAPHICS_3D_DEV_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginGraphics3D::GetInterface) },
  { PPB_IMAGEDATA_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginImageData::GetInterface) },
  { PPB_URLLOADER_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginURLLoader::GetInterface) },
  { PPB_URLREQUESTINFO_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginURLRequestInfo::GetInterface) },
  { PPB_URLRESPONSEINFO_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginURLResponseInfo::GetInterface) },
  { PPB_VAR_DEPRECATED_INTERFACE,
    reinterpret_cast<GetInterfacePtr>(PluginVar::GetInterface) },
};
}

const void* GetInterfaceProxy(const char* interface_name) {
  // The key strings are macros that may not sort in an obvious order relative
  // to the name.  Hence, although we would like to use bsearch, we search
  // linearly.
  for (size_t i = 0; i < NACL_ARRAY_SIZE(interface_map); ++i) {
    if (strcmp(interface_name, interface_map[i].name) == 0) {
      return interface_map[i].func();
    }
  }
  return NULL;
}


}  // namespace ppapi_proxy

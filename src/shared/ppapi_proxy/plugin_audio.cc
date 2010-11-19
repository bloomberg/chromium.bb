/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_audio.h"

#include <stdio.h>
#include <string.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/dev/ppb_audio_config_dev.h"

namespace ppapi_proxy {

namespace {
PP_Resource Create(PP_Instance instance,
                   PP_Resource config,
                   PPB_Audio_Callback audio_callback,
                   void* user_data) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(config);
  UNREFERENCED_PARAMETER(audio_callback);
  UNREFERENCED_PARAMETER(user_data);
  return kInvalidResourceId;
}

PP_Bool IsAudio(PP_Resource resource) {
  return PluginResource::GetAs<PluginAudio>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

PP_Resource GetCurrentConfig(PP_Resource audio) {
  UNREFERENCED_PARAMETER(audio);
  return kInvalidResourceId;
}

PP_Bool StartPlayback(PP_Resource audio) {
  UNREFERENCED_PARAMETER(audio);
  return PP_FALSE;
}

PP_Bool StopPlayback(PP_Resource audio) {
  UNREFERENCED_PARAMETER(audio);
  return PP_FALSE;
}
}  // namespace

const PPB_Audio_Dev* PluginAudio::GetInterface() {
  static const PPB_Audio_Dev intf = {
    Create,
    IsAudio,
    GetCurrentConfig,
    StartPlayback,
    StopPlayback,
  };
  return &intf;
}

}  // namespace ppapi_proxy

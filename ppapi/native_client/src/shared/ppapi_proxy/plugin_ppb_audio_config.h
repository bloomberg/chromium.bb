// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_AUDIO_CONFIG_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_AUDIO_CONFIG_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/ppb_audio_config.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_AudioConfig interface.
class PluginAudioConfig : public PluginResource {
 public:
  static const PPB_AudioConfig* GetInterface();
  // Returns the 1.0 interface to support backwards-compatibility.
  static const PPB_AudioConfig_1_0* GetInterface1_0();

 protected:
  virtual ~PluginAudioConfig() {}

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginAudioConfig);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_AUDIO_CONFIG_H_

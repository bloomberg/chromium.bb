// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_audio_config_proxy.h"

#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/audio_config_impl.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateAudioConfigProxy(Dispatcher* dispatcher,
                                       const void* target_interface) {
  return new PPB_AudioConfig_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_AudioConfig_Proxy::PPB_AudioConfig_Proxy(Dispatcher* dispatcher,
                                             const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_AudioConfig_Proxy::~PPB_AudioConfig_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_AudioConfig_Proxy::GetInfo() {
  static const Info info = {
    thunk::GetPPB_AudioConfig_Thunk(),
    PPB_AUDIO_CONFIG_INTERFACE,
    INTERFACE_ID_PPB_AUDIO_CONFIG,
    false,
    &CreateAudioConfigProxy,
  };
  return &info;
}

bool PPB_AudioConfig_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // There are no IPC messages for this interface.
  NOTREACHED();
  return false;
}

}  // namespace proxy
}  // namespace ppapi

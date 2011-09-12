// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_AUDIO_CONFIG_PROXY_H_
#define PPAPI_PROXY_PPB_AUDIO_CONFIG_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_AudioConfig;

namespace ppapi {
namespace proxy {

class PPB_AudioConfig_Proxy : public InterfaceProxy {
 public:
  PPB_AudioConfig_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_AudioConfig_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_AudioConfig_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_AUDIO_CONFIG_PROXY_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_GLES_CHROMIUM_TEXTURE_MAPPING_PROXY_H_
#define PPAPI_PPB_GLES_CHROMIUM_TEXTURE_MAPPING_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"

struct PPB_GLESChromiumTextureMapping_Dev;

namespace pp {
namespace proxy {

class PPB_GLESChromiumTextureMapping_Proxy : public InterfaceProxy {
 public:
  PPB_GLESChromiumTextureMapping_Proxy(Dispatcher* dispatcher,
                                       const void* target_interface);
  virtual ~PPB_GLESChromiumTextureMapping_Proxy();

  const PPB_GLESChromiumTextureMapping_Dev*
  ppb_gles_chromium_tm_target() const {
    return reinterpret_cast<const PPB_GLESChromiumTextureMapping_Dev*>(
        target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual bool OnMessageReceived(const IPC::Message& msg);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_GLES_CHROMIUM_TEXTURE_MAPPING_PROXY_H_

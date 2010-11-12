// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_BUFFER_PROXY_H_
#define PPAPI_PPB_BUFFER_PROXY_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Buffer_Dev;

namespace pp {
namespace proxy {

class PPB_Buffer_Proxy : public InterfaceProxy {
 public:
  PPB_Buffer_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Buffer_Proxy();

  const PPB_Buffer_Dev* ppb_buffer_target() const {
    return static_cast<const PPB_Buffer_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Module module,
                   int32_t size,
                   PP_Resource* result_resource,
                   uint64_t* result_shm_handle);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_BUFFER_PROXY_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FILE_SYSTEM_PROXY_H_
#define PPAPI_PROXY_PPB_FILE_SYSTEM_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_FileSystem_Dev;

namespace pp {
namespace proxy {

class HostResource;

class PPB_FileSystem_Proxy : public InterfaceProxy {
 public:
  PPB_FileSystem_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_FileSystem_Proxy();

  static const Info* GetInfo();

  const PPB_FileSystem_Dev* ppb_file_system_target() const {
    return static_cast<const PPB_FileSystem_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance,
                   int type,
                   HostResource* result);
  void OnMsgOpen(const HostResource& filesystem,
                 int64_t expected_size);

  void OnMsgOpenComplete(const HostResource& filesystem,
                         int32_t result);

  void OpenCompleteInHost(int32_t result, const HostResource& host_resource);

  CompletionCallbackFactory<PPB_FileSystem_Proxy,
                            ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileSystem_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_FILE_SYSTEM_PROXY_H_

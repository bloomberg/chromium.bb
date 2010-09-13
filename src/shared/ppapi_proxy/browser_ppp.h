// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_H_

#include <stdarg.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppp.h"

namespace ppapi_proxy {

class BrowserPpp {
 public:
  explicit BrowserPpp(NaClSrpcChannel* channel)
      : channel_(channel), plugin_pid_(0) {}
  ~BrowserPpp() {}

  int32_t InitializeModule(PP_Module module_id,
                           PPB_GetInterface get_browser_interface,
                           PP_Instance instance);
  void ShutdownModule();
  const void* GetInterface(const char* interface_name);

  NaClSrpcChannel* channel() const { return channel_; }
  int plugin_pid() const { return plugin_pid_; }

 private:
  // The SRPC channel used to communicate with the plugin.
  NaClSrpcChannel* channel_;
  // The PID of the plugin.
  int plugin_pid_;
  // The thread used to handle CallOnMainThread, etc.
  struct NaClThread upcall_thread_;
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserPpp);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_H_

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
#include "ppapi/c/ppp_instance.h"

namespace plugin {
class PluginPpapi;
}

namespace ppapi_proxy {

class BrowserPpp {
 public:
  explicit BrowserPpp(NaClSrpcChannel* main_channel,
                      plugin::PluginPpapi* plugin)
      : main_channel_(main_channel), plugin_pid_(0), plugin_(plugin) {}
  ~BrowserPpp() {}

  int32_t InitializeModule(PP_Module module_id,
                           PPB_GetInterface get_browser_interface);
  void ShutdownModule();
  // Returns an interface pointer or NULL.
  const void* GetPluginInterface(const char* interface_name);
  // Returns an interface pointer or fails on a NULL CHECK.
  const void* GetPluginInterfaceSafe(const char* interface_name);

  // Guaranteed to be non-NULL if module initialization succeeded.
  // Use this instead of GetPluginInterface for PPP_INSTANCE_INTERFACE.
  const PPP_Instance* ppp_instance_interface() {
    return ppp_instance_interface_;
  }

  NaClSrpcChannel* main_channel() const { return main_channel_; }
  int plugin_pid() const { return plugin_pid_; }
  plugin::PluginPpapi* plugin() { return plugin_; }

 private:
  // The "main" SRPC channel used to communicate with the plugin.
  NaClSrpcChannel* main_channel_;
  // The PID of the plugin.
  int plugin_pid_;
  // Plugin that owns this proxy.
  plugin::PluginPpapi* plugin_;

  // Set on module initialization.
  const PPP_Instance* ppp_instance_interface_;

  // The thread used to handle calls on other than the main thread.
  struct NaClThread upcall_thread_;
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserPpp);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_H_

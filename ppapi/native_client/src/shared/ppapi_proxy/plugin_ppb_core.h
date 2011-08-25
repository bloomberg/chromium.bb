// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CORE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CORE_H_

#include "native_client/src/include/nacl_macros.h"

class PPB_Core;

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_Core interface.
// We will also need an rpc service to implement the trusted side, which is a
// very thin wrapper around the PPB_Core interface returned from the browser.
class PluginCore {
 public:
  // Return an interface pointer usable by PPAPI plugins.
  static const PPB_Core* GetInterface();
  // Mark the calling thread as the main thread for IsMainThread.
  static void MarkMainThread();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginCore);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CORE_H_

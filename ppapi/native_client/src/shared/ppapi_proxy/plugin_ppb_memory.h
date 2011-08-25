// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MEMORY_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MEMORY_H_

#include "native_client/src/include/nacl_macros.h"

struct PPB_Memory_Dev;

namespace ppapi_proxy {

// Implements the plugin side of the PPB_Memory_Dev interface.
class PluginMemory {
 public:
  // Returns an interface pointer suitable to the PPAPI client.
  static const PPB_Memory_Dev* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginMemory);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MEMORY_H_


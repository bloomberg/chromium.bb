// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MOUSE_LOCK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MOUSE_LOCK_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppb_mouse_lock_dev.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_MouseLock_Dev interface.
class PluginMouseLock {
 public:
  PluginMouseLock();
  static const PPB_MouseLock_Dev* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginMouseLock);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MOUSE_LOCK_H_


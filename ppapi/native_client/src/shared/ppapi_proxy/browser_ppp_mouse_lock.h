// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_MOUSE_LOCK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_MOUSE_LOCK_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppp_mouse_lock_dev.h"

namespace ppapi_proxy {

// Implements the trusted side of the PPP_MouseLock_Dev interface.
class BrowserMouseLock {
 public:
  static const PPP_MouseLock_Dev* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserMouseLock);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_MOUSE_LOCK_H_


// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_MESSAGING_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_MESSAGING_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppp_messaging.h"

namespace ppapi_proxy {

// Implements the trusted side of the PPP_Messaging interface.
class BrowserMessaging {
 public:
  static const PPP_Messaging* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserMessaging);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_MESSAGING_H_


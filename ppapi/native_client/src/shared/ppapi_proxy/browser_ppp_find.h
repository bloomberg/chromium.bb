// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_FIND_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_FIND_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

// Implements the trusted side of the PPP_Find_Dev interface.
class BrowserFind {
 public:
  static const PPP_Find_Dev* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserFind);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_FIND_H_


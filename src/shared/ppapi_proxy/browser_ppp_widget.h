// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_WIDGET_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_WIDGET_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

// Implements the trusted side of the PPP_Widget_Dev interface.
class BrowserWidget {
 public:
  static const PPP_Widget_Dev* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserWidget);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_PPP_WIDGET_H_


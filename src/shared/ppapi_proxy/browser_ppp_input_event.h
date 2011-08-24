// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_INPUT_EVENT_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_INPUT_EVENT_H_

#include <map>

#include "native_client/src/include/nacl_macros.h"
// Needed because ppp_instance.h does not include everything it needs.
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_input_event.h"

struct PPP_InputEvent;

namespace ppapi_proxy {

// Implements the trusted side of the PPP_InputEvent interface.
class BrowserInputEvent {
 public:
  static const PPP_InputEvent* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserInputEvent);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_INPUT_EVENT_H_

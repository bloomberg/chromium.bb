// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MOUSE_CURSOR_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MOUSE_CURSOR_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppb_mouse_cursor.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_MouseCursor interface.
class PluginMouseCursor {
 public:
  PluginMouseCursor();
  static const PPB_MouseCursor_1_0* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginMouseCursor);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_MOUSE_CURSOR_H_


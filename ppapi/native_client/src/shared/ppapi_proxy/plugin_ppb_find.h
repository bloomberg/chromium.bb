// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_FIND_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_FIND_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppb_find_dev.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_Find_Dev interface.
class PluginFind {
 public:
  PluginFind();
  static const PPB_Find_Dev* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginFind);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_FIND_H_


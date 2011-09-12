// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_URL_LOADER_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_URL_LOADER_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppb_url_loader.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_URLLoader interface.
class PluginURLLoader {
 public:
  static const PPB_URLLoader* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginURLLoader);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_URL_LOADER_H_

// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_URL_REQUEST_INFO_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_URL_REQUEST_INFO_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppb_url_request_info.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_URLRequestInfo interface.
class PluginURLRequestInfo {
 public:
  static const PPB_URLRequestInfo* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginURLRequestInfo);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_URL_REQUEST_INFO_H_

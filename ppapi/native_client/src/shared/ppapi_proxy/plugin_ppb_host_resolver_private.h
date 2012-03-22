// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_HOST_RESOLVER_PRIVATE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_HOST_RESOLVER_PRIVATE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_HostResolver_Private interface.
class PluginHostResolverPrivate {
 public:
  static const PPB_HostResolver_Private* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginHostResolverPrivate);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_HOST_RESOLVER_PRIVATE_H_

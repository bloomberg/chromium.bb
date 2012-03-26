// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_NET_ADDRESS_PRIVATE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_NET_ADDRESS_PRIVATE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/private/ppb_net_address_private.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_NetAddress_Private interface.
class PluginNetAddressPrivate {
 public:
  static const PPB_NetAddress_Private_0_1* GetInterface0_1();
  static const PPB_NetAddress_Private_1_0* GetInterface1_0();
  static const PPB_NetAddress_Private_1_1* GetInterface1_1();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginNetAddressPrivate);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_NET_ADDRESS_PRIVATE_H_

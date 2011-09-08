// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_mouse_lock.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_instance.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

namespace {

void MouseLockLost(PP_Instance instance) {
  DebugPrintf("PPP_MouseLock::MouseLockLost: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PppMouseLockRpcClient::PPP_MouseLock_MouseLockLost(
          GetMainSrpcChannel(instance),
          instance);

  DebugPrintf("PPP_MouseLock::MouseLockLost: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_MouseLock_Dev* BrowserMouseLock::GetInterface() {
  static const PPP_MouseLock_Dev mouse_lock_interface = {
    MouseLockLost
  };
  return &mouse_lock_interface;
}

}  // namespace ppapi_proxy


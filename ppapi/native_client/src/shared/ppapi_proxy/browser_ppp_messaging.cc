// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_messaging.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

namespace {

void HandleMessage(PP_Instance instance, struct PP_Var message) {
  DebugPrintf("PPP_Messaging::HandleMessage: instance=%"NACL_PRIu32"\n",
              instance);
  uint32_t message_length = kMaxVarSize;
  nacl::scoped_array<char> message_bytes(Serialize(&message, 1,
                                                   &message_length));
  NaClSrpcError srpc_result =
      PppMessagingRpcClient::PPP_Messaging_HandleMessage(
          GetMainSrpcChannel(instance),
          instance,
          message_length,
          message_bytes.get());

  DebugPrintf("PPP_Messaging::HandleMessage: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPP_Messaging* BrowserMessaging::GetInterface() {
  static const PPP_Messaging messaging_interface = {
    HandleMessage
  };
  return &messaging_interface;
}

}  // namespace ppapi_proxy


// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_messaging.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

void PostMessage(PP_Instance instance, struct PP_Var message) {
  DebugPrintf("PPB_Messaging::PostMessage: instance=%"NACL_PRIu32"\n",
              instance);
  if (!ppapi_proxy::PPBCoreInterface()->IsMainThread())
    return;  // Only supported on main thread.

  uint32_t message_length = kMaxVarSize;
  nacl::scoped_array<char> message_bytes(Serialize(&message, 1,
                                                   &message_length));
  NaClSrpcError srpc_result =
      PpbMessagingRpcClient::PPB_Messaging_PostMessage(
          GetMainSrpcChannel(),
          instance,
          message_length,
          message_bytes.get());
  DebugPrintf("PPB_Messaging::PostMessage: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

const PPB_Messaging* PluginMessaging::GetInterface() {
  static const PPB_Messaging  messaging_interface = {
    PostMessage
  };
  return &messaging_interface;
}

}  // namespace ppapi_proxy

// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp_selection.h"

// Include file order cannot be observed because ppp_instance declares a
// structure return type that causes an error on Windows.
// TODO(sehr, brettw): fix the return types and include order in PPAPI.
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

namespace ppapi_proxy {

namespace {

struct PP_Var GetSelectedText(PP_Instance instance, PP_Bool html) {
  DebugPrintf("PPP_Selection_Dev::GetSelectedText: "
              "instance=%"NACL_PRIu32"\n", instance);
  NaClSrpcChannel* channel = GetMainSrpcChannel(instance);
  nacl_abi_size_t text_size = kMaxVarSize;
  nacl::scoped_array<char> text_bytes(new char[kMaxVarSize]);
  NaClSrpcError srpc_result =
      PppSelectionRpcClient::PPP_Selection_GetSelectedText(
          channel,
          instance,
          static_cast<int32_t>(html),
          &text_size,
          text_bytes.get());

  DebugPrintf("PPP_Selection_Dev::GetSelectedText: %s\n",
              NaClSrpcErrorString(srpc_result));

  PP_Var selected_text = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    (void) DeserializeTo(
        channel, text_bytes.get(), text_size, 1, &selected_text);
  }
  return selected_text;
}

}  // namespace

const PPP_Selection_Dev* BrowserSelection::GetInterface() {
  static const PPP_Selection_Dev selection_interface = {
    GetSelectedText
  };
  return &selection_interface;
}

}  // namespace ppapi_proxy


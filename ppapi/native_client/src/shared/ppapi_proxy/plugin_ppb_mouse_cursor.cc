// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_mouse_cursor.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPPointBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Point));

PP_Bool SetCursor(PP_Instance instance,
                  enum PP_MouseCursor_Type type,
                  PP_Resource custom_image,
                  const struct PP_Point* hot_spot) {
  DebugPrintf("PPB_MouseCursor::SetCursor: "
              "instance=%"NACL_PRId32"\n", instance);

  int32_t hot_spot_size = (hot_spot != NULL) ? kPPPointBytes : 0;
  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbMouseCursorRpcClient::PPB_MouseCursor_SetCursor(
          GetMainSrpcChannel(),
          instance,
          type,
          custom_image,
          hot_spot_size,
          reinterpret_cast<char*>(const_cast<struct PP_Point*>(hot_spot)),
          &success);

  DebugPrintf("PPB_MouseCursor::SetCursor: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

}  // namespace

const PPB_MouseCursor_1_0* PluginMouseCursor::GetInterface() {
  static const PPB_MouseCursor mouse_cursor_interface = {
    SetCursor,
  };
  return &mouse_cursor_interface;
}

}  // namespace ppapi_proxy

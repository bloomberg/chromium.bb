// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_network_list_private.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_network_list_private.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Bool IsNetworkList(PP_Resource resource) {
  DebugPrintf("PPB_NetworkList_Private::IsNetworkList: "
              "resource=%"NACL_PRId32"\n", resource);

  int32_t is_network_list;
  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_IsNetworkList(
          GetMainSrpcChannel(), resource, &is_network_list);

  DebugPrintf("PPB_NetworkList_Private::IsNetworkList: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return PP_FALSE;

  return PP_FromBool(is_network_list);
}

uint32_t GetCount(PP_Resource resource) {
  DebugPrintf("PPB_NetworkList_Private::GetCount: "
              "resource=%"NACL_PRId32"\n", resource);

  int32_t count;
  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetCount(
          GetMainSrpcChannel(), resource, &count);

  DebugPrintf("PPB_NetworkList_Private::GetCount: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return 0;

  return count;
}

struct PP_Var GetName(PP_Resource resource, uint32_t index) {
  DebugPrintf("PPB_NetworkList_Private::GetName:"
              " resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  nacl_abi_size_t name_size = kMaxReturnVarSize;
  nacl::scoped_array<char> name_bytes(new char[name_size]);
  if (name_bytes.get() == NULL)
    return PP_MakeUndefined();

  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetName(
          GetMainSrpcChannel(), resource, index,
          &name_size, name_bytes.get());

  struct PP_Var name = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK)
    DeserializeTo(name_bytes.get(), name_size, 1, &name);

  DebugPrintf("PPB_NetworkList_Private::GetName: %s\n",
              NaClSrpcErrorString(srpc_result));

  return name;
}

PP_NetworkListType_Private GetType(PP_Resource resource, uint32_t index) {
  DebugPrintf("PPB_NetworkList_Private::GetType:"
              " resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  int32_t type;
  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetType(
          GetMainSrpcChannel(), resource, index, &type);

  DebugPrintf("PPB_NetworkList_Private::GetType: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return PP_NETWORKLIST_UNKNOWN;

  return static_cast<PP_NetworkListType_Private>(type);
}

PP_NetworkListState_Private GetState(PP_Resource resource, uint32_t index) {
  DebugPrintf("PPB_NetworkList_Private::GetState:"
              " resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  int32_t state;
  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetState(
          GetMainSrpcChannel(), resource, index, &state);

  DebugPrintf("PPB_NetworkList_Private::GetState: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return PP_NETWORKLIST_DOWN;

  return static_cast<PP_NetworkListState_Private>(state);
}

int32_t GetIpAddresses(PP_Resource resource,
                      uint32_t index,
                      PP_NetAddress_Private addresses[],
                      uint32_t count) {
  DebugPrintf("PPB_NetworkList_Private::GetIpAddresses:"
              " resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  nacl_abi_size_t addr_size = count * sizeof(PP_NetAddress_Private);
  int32_t addr_count;
  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetIpAddresses(
          GetMainSrpcChannel(), resource, index,
          &addr_size, reinterpret_cast<char*>(addresses),
          &addr_count);

  DebugPrintf("PPB_NetworkList_Private::GetIpAddresses: %s\n",
              NaClSrpcErrorString(srpc_result));

  DCHECK(static_cast<uint32_t>(addr_count) > count ||
         addr_count * sizeof(PP_NetAddress_Private) == addr_size);

  return addr_count;
}

struct PP_Var GetDisplayName(PP_Resource resource, uint32_t index) {
  DebugPrintf("PPB_NetworkList_Private::GetDisplayName:"
              " resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  nacl_abi_size_t name_size = kMaxReturnVarSize;
  nacl::scoped_array<char> name_bytes(new char[name_size]);
  if (name_bytes.get() == NULL)
    return PP_MakeUndefined();

  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetDisplayName(
          GetMainSrpcChannel(), resource, index,
          &name_size, name_bytes.get());

  struct PP_Var name = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK)
    DeserializeTo(name_bytes.get(), name_size, 1, &name);

  DebugPrintf("PPB_NetworkList_Private::GetDisplayName: %s\n",
              NaClSrpcErrorString(srpc_result));

  return name;
}

uint32_t GetMTU(PP_Resource resource, uint32_t index) {
  DebugPrintf("PPB_NetworkList_Private::GetMTU:"
              " resource=%"NACL_PRId32" index=%"NACL_PRId32"\n",
              resource, index);

  int32_t mtu;
  NaClSrpcError srpc_result = PpbNetworkListPrivateClient::
      PPB_NetworkList_Private_GetMTU(
          GetMainSrpcChannel(), resource, index, &mtu);

  DebugPrintf("PPB_NetworkList_Private::GetMTU: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return PP_NETWORKLIST_UNKNOWN;

  return mtu;
}

}  // namespace

const PPB_NetworkList_Private* PluginNetworkListPrivate::GetInterface() {
  static const PPB_NetworkList_Private networklist_private_interface = {
    IsNetworkList,
    GetCount,
    GetName,
    GetType,
    GetState,
    GetIpAddresses,
    GetDisplayName,
    GetMTU,
  };
  return &networklist_private_interface;
}

}  // namespace ppapi_proxy

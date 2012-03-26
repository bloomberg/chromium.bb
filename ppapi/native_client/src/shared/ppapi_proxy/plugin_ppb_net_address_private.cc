// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_net_address_private.h"

#include <cstddef>
#include <new>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Bool AreEqual(const PP_NetAddress_Private* addr1,
                 const PP_NetAddress_Private* addr2) {
  DebugPrintf("PPB_NetAddress_Private::AreEqual\n");

  char* const raw_addr1 =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr1));
  char* const raw_addr2 =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr2));

  int32_t equals;
  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_AreEqual(
          GetMainSrpcChannel(),
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)),
          raw_addr1,
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)),
          raw_addr2,
          &equals);

  DebugPrintf("PPB_NetAddress_Private::AreEqual: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && equals)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Bool AreHostsEqual(const PP_NetAddress_Private* addr1,
                      const PP_NetAddress_Private* addr2) {
  DebugPrintf("PPB_NetAddress_Private::AreHostsEqual\n");

  char* const raw_addr1 =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr1));
  char* const raw_addr2 =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr2));

  int32_t equals;
  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_AreHostsEqual(
          GetMainSrpcChannel(),
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)),
          raw_addr1,
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)),
          raw_addr2,
          &equals);

  DebugPrintf("PPB_NetAddress_Private::AreHostsEqual: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && equals)
    return PP_TRUE;
  return PP_FALSE;
}

PP_Var Describe(PP_Module module,
                const PP_NetAddress_Private* addr,
                PP_Bool include_port) {
  DebugPrintf("PP_NetAddress_Private::Describe: module=%"NACL_PRId32"\n",
              module);

  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));
  nacl_abi_size_t description_size = kMaxReturnVarSize;
  nacl::scoped_array<char> description_bytes(
      new (std::nothrow) char[description_size]);
  if (description_bytes.get() == NULL)
    return PP_MakeUndefined();

  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_Describe(
          GetMainSrpcChannel(),
          module,
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)), raw_addr,
          include_port,
          &description_size, description_bytes.get());

  PP_Var description = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    (void) DeserializeTo(description_bytes.get(), description_size, 1,
                         &description);
  }

  DebugPrintf("PPB_NetAddressPrivate::Describe: %s\n",
              NaClSrpcErrorString(srpc_result));

  return description;
}

PP_Bool ReplacePort(const PP_NetAddress_Private* src_addr,
                    uint16_t port,
                    PP_NetAddress_Private* dst_addr) {
  DebugPrintf("PPB_NetAddressPrivate::ReplacePort\n");

  char* const raw_src_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(src_addr));
  nacl_abi_size_t dst_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_dst_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(dst_addr));

  int32_t success;
  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_ReplacePort(
          GetMainSrpcChannel(),
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)),
          raw_src_addr,
          port,
          &dst_bytes, raw_dst_addr,
          &success);

  DebugPrintf("PPB_NetAddress_Private::ReplacePort: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

void GetAnyAddress(PP_Bool is_ipv6, PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_NetAddress_Private::GetAnyAddress\n");

  nacl_abi_size_t addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));

  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_GetAnyAddress(
          GetMainSrpcChannel(),
          is_ipv6,
          &addr_bytes, raw_addr);

  DebugPrintf("PPB_NetAddress_Private::GetAnyAddress: %s\n",
              NaClSrpcErrorString(srpc_result));
}

PP_NetAddressFamily_Private GetFamily(const PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_NetAddress_Private::GetFamily\n");

  nacl_abi_size_t addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));

  int32_t addr_family = PP_NETADDRESSFAMILY_UNSPECIFIED;
  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_GetFamily(
          GetMainSrpcChannel(),
          addr_bytes, raw_addr,
          &addr_family);

  DebugPrintf("PPB_NetAddress_Private::GetFamily: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return static_cast<PP_NetAddressFamily_Private>(addr_family);
  return PP_NETADDRESSFAMILY_UNSPECIFIED;
}

uint16_t GetPort(const PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_NetAddress_Private::GetPort\n");

  nacl_abi_size_t addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));
  int32_t port;

  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_GetPort(
          GetMainSrpcChannel(),
          addr_bytes, raw_addr,
          &port);

  DebugPrintf("PPB_NetAddress_Private::GetPort: %s\n",
              NaClSrpcErrorString(srpc_result));

  return static_cast<uint16_t>(static_cast<uint32_t>(port));
}

PP_Bool GetAddress(const PP_NetAddress_Private* addr,
                   void* address,
                   uint16_t address_size) {
  DebugPrintf("PP_NetAddress_Private::GetAddress");

  int32_t success = 0;
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));
  nacl_abi_size_t address_bytes_size = address_size;
  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_GetAddress(
          GetMainSrpcChannel(),
          static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)), raw_addr,
          &address_bytes_size, static_cast<char*>(address),
          &success);

  DebugPrintf("PPB_NetAddressPrivate::GetAddress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

uint32_t GetScopeID(const PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_NetAddress_Private::GetScopeID\n");

  nacl_abi_size_t addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));
  int32_t scope_id;

  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::PPB_NetAddress_Private_GetScopeID(
          GetMainSrpcChannel(),
          addr_bytes,
          raw_addr, &scope_id);

  DebugPrintf("PPB_NetAddress_Private::GetScopeID: %s\n",
              NaClSrpcErrorString(srpc_result));

  return static_cast<uint32_t>(scope_id);
}

void CreateFromIPv4Address(const uint8_t ip[4],
                           uint16_t port,
                           struct PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_NetAddress_Private::CreateFromIPv4Address\n");

  char* raw_ip = reinterpret_cast<char*>(const_cast<uint8_t*>(ip));

  nacl_abi_size_t addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));

  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::
      PPB_NetAddress_Private_CreateFromIPv4Address(
          GetMainSrpcChannel(),
          4, raw_ip, port,
          &addr_bytes, raw_addr);

  DebugPrintf("PPB_NetAddress_Private::CreateFromIPv4Address: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void CreateFromIPv6Address(const uint8_t ip[16],
                           uint32_t scope_id,
                           uint16_t port,
                           struct PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_NetAddress_Private::CreateFromIPv6Address\n");

  char* raw_ip = reinterpret_cast<char*>(const_cast<uint8_t*>(ip));

  nacl_abi_size_t addr_bytes =
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private));
  char* const raw_addr =
      reinterpret_cast<char*>(const_cast<PP_NetAddress_Private*>(addr));

  NaClSrpcError srpc_result =
      PpbNetAddressPrivateRpcClient::
      PPB_NetAddress_Private_CreateFromIPv6Address(
          GetMainSrpcChannel(),
          16, raw_ip, scope_id, port,
          &addr_bytes, raw_addr);

  DebugPrintf("PPB_NetAddress_Private::CreateFromIPv6Address: %s\n",
              NaClSrpcErrorString(srpc_result));
}

}  // namespace

// static
const PPB_NetAddress_Private_0_1* PluginNetAddressPrivate::GetInterface0_1() {
  static const PPB_NetAddress_Private_0_1 netaddress_private_interface = {
    AreEqual,
    AreHostsEqual,
    Describe,
    ReplacePort,
    GetAnyAddress
  };
  return &netaddress_private_interface;
}

// static
const PPB_NetAddress_Private_1_0* PluginNetAddressPrivate::GetInterface1_0() {
  static const PPB_NetAddress_Private_1_0 netaddress_private_interface = {
    AreEqual,
    AreHostsEqual,
    Describe,
    ReplacePort,
    GetAnyAddress,
    GetFamily,
    GetPort,
    GetAddress
  };
  return &netaddress_private_interface;
}

// static
const PPB_NetAddress_Private_1_1* PluginNetAddressPrivate::GetInterface1_1() {
  static const PPB_NetAddress_Private_1_1 netaddress_private_interface = {
    AreEqual,
    AreHostsEqual,
    Describe,
    ReplacePort,
    GetAnyAddress,
    GetFamily,
    GetPort,
    GetAddress,
    GetScopeID,
    CreateFromIPv4Address,
    CreateFromIPv6Address
  };
  return &netaddress_private_interface;
}

}  // namespace ppapi_proxy

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_NetAddress_Private functions.

#include <limits>

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/cpp/var.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBNetAddressPrivateInterface;
using ppapi_proxy::SerializeTo;
using ppapi_proxy::kMaxReturnVarSize;

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_AreEqual(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t addr1_bytes, char* addr1,
    nacl_abi_size_t addr2_bytes, char* addr2,
    // output
    int32_t* equals) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr1_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }
  if (addr2_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PP_Bool pp_equals =
      PPBNetAddressPrivateInterface()->AreEqual(
          reinterpret_cast<PP_NetAddress_Private*>(addr1),
          reinterpret_cast<PP_NetAddress_Private*>(addr2));

  DebugPrintf("PPB_NetAddress_Private::AreEqual: pp_equals=%d\n",
              pp_equals);

  *equals = (pp_equals == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_AreHostsEqual(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t addr1_bytes, char* addr1,
    nacl_abi_size_t addr2_bytes, char* addr2,
    // output
    int32_t* equals) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr1_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }
  if (addr2_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PP_Bool pp_equals =
      PPBNetAddressPrivateInterface()->AreHostsEqual(
          reinterpret_cast<PP_NetAddress_Private*>(addr1),
          reinterpret_cast<PP_NetAddress_Private*>(addr2));

  DebugPrintf("PPB_NetAddress_Private::AreHostsEqual: pp_equals=%d\n",
              pp_equals);

  *equals = (pp_equals == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_Describe(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    int32_t module,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t include_port,
    // output
    nacl_abi_size_t* description_bytes, char* description) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }
  if (include_port != PP_FALSE && include_port != PP_TRUE)
    return;
  if (*description_bytes != static_cast<nacl_abi_size_t>(kMaxReturnVarSize))
    return;

  PP_Var pp_address = PPBNetAddressPrivateInterface()->Describe(
      module,
      reinterpret_cast<PP_NetAddress_Private*>(addr),
      static_cast<PP_Bool>(include_port));
  pp::Var address(pp::PASS_REF, pp_address);

  if (!SerializeTo(&address.pp_var(), description, description_bytes))
    return;

  DebugPrintf("PPB_NetAddress_Private::Describe: description=%s\n",
              address.AsString().c_str());
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_ReplacePort(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t src_addr_bytes, char* src_addr,
    int32_t port,
    // output
    nacl_abi_size_t* dst_addr_bytes, char* dst_addr,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (src_addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }
  if (port < 0 || port > std::numeric_limits<uint16_t>::max())
    return;
  if (*dst_addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PP_Bool pp_success = PPBNetAddressPrivateInterface()->ReplacePort(
      reinterpret_cast<PP_NetAddress_Private*>(src_addr),
      static_cast<uint16_t>(port),
      reinterpret_cast<PP_NetAddress_Private*>(dst_addr));

  DebugPrintf("PPB_NetAddress_Private::ReplacePort: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_GetAnyAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    int32_t is_ipv6,
    // output
    nacl_abi_size_t* addr_bytes, char* addr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (is_ipv6 != PP_FALSE && is_ipv6 != PP_TRUE)
    return;
  if (*addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PPBNetAddressPrivateInterface()->GetAnyAddress(
      static_cast<PP_Bool>(is_ipv6),
      reinterpret_cast<PP_NetAddress_Private*>(addr));

  DebugPrintf("PPB_NetAddress_Private::GetAnyAddress\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_GetFamily(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t addr_bytes, char* addr,
    // output
    int32_t* addr_family) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  *addr_family = static_cast<int32_t>(
      PPBNetAddressPrivateInterface()->GetFamily(
          reinterpret_cast<PP_NetAddress_Private*>(addr)));

  DebugPrintf("PPB_NetAddress_Private::GetFamily\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_GetPort(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t addr_bytes, char* addr,
    // output
    int32_t* port) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  *port = PPBNetAddressPrivateInterface()->GetPort(
      reinterpret_cast<PP_NetAddress_Private*>(addr));

  DebugPrintf("PPB_NetAddress_Private::GetPort\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_GetAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t addr_bytes, char* addr,
    // output
    nacl_abi_size_t* address_bytes, char* address,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PP_Bool pp_success = PPBNetAddressPrivateInterface()->GetAddress(
      reinterpret_cast<PP_NetAddress_Private*>(addr),
      address, static_cast<uint16_t>(*address_bytes));

  DebugPrintf("PPB_NetAddress_Private::GetAddress: pp_success=%d\n",
              pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_GetScopeID(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t addr_bytes, char* addr,
    // output
    int32_t* scope_id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  *scope_id = PPBNetAddressPrivateInterface()->GetScopeID(
      reinterpret_cast<PP_NetAddress_Private*>(addr));

  DebugPrintf("PPB_NetAddress_Private::GetScopeID\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}

void
PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_CreateFromIPv4Address(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t ip_bytes, char* ip,
    int32_t port,
    // output
    nacl_abi_size_t* addr_bytes, char* addr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (ip_bytes != static_cast<nacl_abi_size_t>(4))
    return;
  if (port < 0 || port > std::numeric_limits<uint16_t>::max())
    return;
  if (*addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PPBNetAddressPrivateInterface()->CreateFromIPv4Address(
      reinterpret_cast<uint8_t*>(ip), static_cast<uint16_t>(port),
      reinterpret_cast<PP_NetAddress_Private*>(addr));

  DebugPrintf("PPB_NetAddress_Private::CreateFromIPv4Address\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}

void
PpbNetAddressPrivateRpcServer::PPB_NetAddress_Private_CreateFromIPv6Address(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    nacl_abi_size_t ip_bytes, char* ip,
    int32_t scope_id,
    int32_t port,
    // output
    nacl_abi_size_t* addr_bytes, char* addr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (ip_bytes != static_cast<nacl_abi_size_t>(16))
    return;
  if (port < 0 || port > std::numeric_limits<uint16_t>::max())
    return;
  if (*addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private))) {
    return;
  }

  PPBNetAddressPrivateInterface()->CreateFromIPv6Address(
      reinterpret_cast<uint8_t*>(ip), static_cast<uint32_t>(scope_id),
      static_cast<uint16_t>(port),
      reinterpret_cast<PP_NetAddress_Private*>(addr));

  DebugPrintf("PPB_NetAddress_Private::CreateFromIPv6Address\n");

  rpc->result = NACL_SRPC_RESULT_OK;
}

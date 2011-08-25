// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_UPCALL_H_
#define GEN_PPAPI_PROXY_UPCALL_H_

#ifndef __native_client__
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

class PppUpcallRpcServer {
 public:
  static void PPB_Core_CallOnMainThread(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t delay_in_milliseconds,
      int32_t callback_id,
      int32_t result);

 private:
  PppUpcallRpcServer();
  PppUpcallRpcServer(const PppUpcallRpcServer&);
  void operator=(const PppUpcallRpcServer);
};  // class PppUpcallRpcServer

class PpbUpcalls {
 public:
  static NaClSrpcHandlerDesc srpc_methods[];
};  // class PpbUpcalls


#endif  // GEN_PPAPI_PROXY_UPCALL_H_


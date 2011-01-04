// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "trusted/srpcgen/upcall.h"
#ifdef __native_client__
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

namespace {

static void PPB_Core_CallOnMainThreadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PppUpcallRpcServer::PPB_Core_CallOnMainThread(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_Graphics2D_FlushDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PppUpcallRpcServer::PPB_Graphics2D_Flush(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

}  // namespace

NaClSrpcHandlerDesc PpbUpcalls::srpc_methods[] = {
  { "PPB_Core_CallOnMainThread:ii:", PPB_Core_CallOnMainThreadDispatcher },
  { "PPB_Graphics2D_Flush:li:i", PPB_Graphics2D_FlushDispatcher },
  { NULL, NULL }
};


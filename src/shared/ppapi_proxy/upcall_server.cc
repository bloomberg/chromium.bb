// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "trusted/srpcgen/upcall.h"
#ifdef __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

namespace {

static void PPB_Core_CallOnMainThreadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
  PppUpcallRpcServer::PPB_Graphics2D_Flush(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

}  // namespace

NACL_SRPC_METHOD_ARRAY(PpbUpcalls::srpc_methods) = {
  { "PPB_Core_CallOnMainThread:ii:", PPB_Core_CallOnMainThreadDispatcher },
  { "PPB_Graphics2D_Flush:li:i", PPB_Graphics2D_FlushDispatcher },
  { NULL, NULL }
};  // NACL_SRPC_METHOD_ARRAY


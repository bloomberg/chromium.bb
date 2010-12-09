// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_UPCALL_H_
#define GEN_PPAPI_PROXY_UPCALL_H_
#ifdef __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

class PppUpcallRpcServer {
 public:
  static void PPB_Core_CallOnMainThread(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t closure_number,
      int32_t delay_in_milliseconds
  );
  static void PPB_Graphics2D_Flush(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t device_context,
      int32_t callback_index,
      int32_t* result
  );

 private:
  PppUpcallRpcServer();
  PppUpcallRpcServer(const PppUpcallRpcServer&);
  void operator=(const PppUpcallRpcServer);

};  // class PppUpcallRpcServer

class PpbUpcalls {
 public:
  static NACL_SRPC_METHOD_ARRAY(srpc_methods);
};  // class PpbUpcalls

#endif  // GEN_PPAPI_PROXY_UPCALL_H_


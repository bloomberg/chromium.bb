// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_UPCALL_H_
#define GEN_PPAPI_PROXY_UPCALL_H_
#ifdef __native_client__
#include <nacl/nacl_srpc.h>
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

class PppUpcallRpcClient {
 public:
  static NaClSrpcError PPB_Core_CallOnMainThread(
      NaClSrpcChannel* channel,
      int32_t closure_number,
      int32_t delay_in_milliseconds
  );
  static NaClSrpcError PPB_Graphics2D_Flush(
      NaClSrpcChannel* channel,
      int64_t device_context,
      int32_t callback_index,
      int32_t* result
  );

 private:
  PppUpcallRpcClient();
  PppUpcallRpcClient(const PppUpcallRpcClient&);
  void operator=(const PppUpcallRpcClient);

};  // class PppUpcallRpcClient



#endif  // GEN_PPAPI_PROXY_UPCALL_H_


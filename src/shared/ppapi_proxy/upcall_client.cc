// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "untrusted/srpcgen/upcall.h"
#ifdef __native_client__
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"

NaClSrpcError PppUpcallRpcClient::PPB_Core_CallOnMainThread(
    NaClSrpcChannel* channel,
    int32_t closure_number,
    int32_t delay_in_milliseconds)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_CallOnMainThread:ii:",
      closure_number,
      delay_in_milliseconds
  );
  return retval;
}

NaClSrpcError PppUpcallRpcClient::PPB_Graphics2D_Flush(
    NaClSrpcChannel* channel,
    int64_t device_context,
    int32_t callback_index,
    int32_t* result)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Flush:li:i",
      device_context,
      callback_index,
      result
  );
  return retval;
}



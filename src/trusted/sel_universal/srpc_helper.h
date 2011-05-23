/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_SRPC_HELPER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_SRPC_HELPER_H_

#define SRPC_PARAMS NaClSrpcRpc* rpc,     \
                    NaClSrpcArg** ins,                    \
                    NaClSrpcArg** outs, \
                    NaClSrpcClosure* done

#define UNIMPLEMENTED(name) \
void name(SRPC_PARAMS) { \
  UNREFERENCED_PARAMETER(rpc); \
  UNREFERENCED_PARAMETER(ins); \
  UNREFERENCED_PARAMETER(outs); \
  UNREFERENCED_PARAMETER(done); \
  NaClLog(LOG_FATAL, #name " not yet implemented\n"); \
}

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_SRPC_HELPER_H_ */

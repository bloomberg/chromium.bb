/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef _TESTS_POSIX_OVER_SRPC_LINUX_LIB_POSIX_OVER_SRPC_LINUX_H_
#define _TESTS_POSIX_OVER_SRPC_LINUX_LIB_POSIX_OVER_SRPC_LINUX_H_

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

// TODO(mikhailt): Decide how to avoid this absolute path. Mb launcher+linux_lib
// should be one entity - libpsrpc_launcher.
#include "native_client/tests/posix_over_srpc/launcher.h"

// This structure is used like server_instance_data to make it possible to know
// which child process called this particular srpc service method
// (like nonnacl_open, nonnacl_close etc).
struct ChildContext {
  int imc_handle;
  int child_id;
  nacl::SelLdrLauncher* sel_ldr_launcher;
  PosixOverSrpcLauncher* psrpc_launcher;
};

PosixOverSrpcLauncher* GetLauncher(NaClSrpcChannel* channel);
int GetChildId(NaClSrpcChannel* channel);
int NewHandle(NaClSrpcChannel* channel, HandledValue val);
void CollectDesc(NaClSrpcChannel* channel, void* desc, enum CollectTag tag);
void CloseUnusedDescs(NaClSrpcChannel* channel);
nacl::DescWrapperFactory* DescFactory(NaClSrpcChannel* channel);

// These macros are just to reduce code replication in posix calls processing
// methods.
#define GET_VALID_PTR(handle, pptr)                                            \
if (false == GetLauncher(channel)->ValueByHandle(                              \
        GetChildId(channel),                                                   \
        handle,                                                                \
        reinterpret_cast<HandledValue*>(pptr))) {                              \
  return NACL_SRPC_RESULT_APP_ERROR;                                           \
}

#define GET_VALID_FD(handle, pfd)                                              \
HandledValue __hidden_val__;                                                   \
if (false == GetLauncher(channel)->ValueByHandle(                              \
        GetChildId(channel),                                                   \
        handle,                                                                \
        &__hidden_val__)) {                                                    \
  return NACL_SRPC_RESULT_APP_ERROR;                                           \
}                                                                              \
*pfd = __hidden_val__.ival;


NaClSrpcError nonnacl_accept(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                             NaClSrpcArg** outs);
NaClSrpcError nonnacl_bind(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                           NaClSrpcArg** outs);
NaClSrpcError nonnacl_close(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                            NaClSrpcArg** outs);
NaClSrpcError nonnacl_closedir(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                               NaClSrpcArg** outs);
NaClSrpcError nonnacl_getcwd(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                             NaClSrpcArg** outs);
NaClSrpcError nonnacl_getpagesize(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                                  NaClSrpcArg** outs);
NaClSrpcError nonnacl_listen(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                             NaClSrpcArg** outs);
NaClSrpcError nonnacl_open(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                           NaClSrpcArg** outs);
NaClSrpcError nonnacl_opendir(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                              NaClSrpcArg** outs);
NaClSrpcError nonnacl_pathconf(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                               NaClSrpcArg** outs);
NaClSrpcError nonnacl_pipe(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                           NaClSrpcArg** outs);
NaClSrpcError nonnacl_readdir(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                              NaClSrpcArg** outs);
NaClSrpcError nonnacl_setsockopt(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                                 NaClSrpcArg** outs);
NaClSrpcError nonnacl_socket(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                             NaClSrpcArg** outs);
NaClSrpcError nonnacl_times(NaClSrpcChannel* channel, NaClSrpcArg** ins,
                            NaClSrpcArg** outs);

#endif  // _TESTS_POSIX_OVER_SRPC_LINUX_LIB_POSIX_OVER_SRPC_LINUX_H_

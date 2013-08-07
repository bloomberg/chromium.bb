/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/* Utility functions for seting up new NaCl app. */

#ifndef NATIVE_CLIENT_SRC_SHARED_LDR_NACL_LDR_H_
#define NATIVE_CLIENT_SRC_SHARED_LDR_NACL_LDR_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

EXTERN_C_BEGIN

int NaClLdrSetupCommandChannel(
    NaClSrpcImcDescType     socket_addr,
    struct NaClSrpcChannel  *command_channel);

int NaClLdrLoadModule(
    struct NaClSrpcChannel  *command_channel,
    NaClSrpcImcDescType     nexe);

int NaClLdrSetupReverseSetup(
    struct NaClSrpcChannel  *command_channel,
    NaClSrpcImcDescType     *reverse_addr);

int NaClLdrStartModule(
    struct NaClSrpcChannel  *command_channel);

int NaClLdrSetupUntrustedChannel(
    NaClSrpcImcDescType     socket_addr,
    struct NaClSrpcChannel  *untrusted_channel);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_SHARED_LDR_NACL_LDR_H_ */

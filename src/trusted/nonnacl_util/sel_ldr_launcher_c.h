/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// C bindings for class for launching sel_ldr.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_C_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_C_H_

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

extern struct NaClSelLdrLauncher* NaClSelLdrStart(
    const char* application_name,
    int imc_fd,
    int sel_ldr_argc,
    const char* sel_ldr_argv[],
    int application_argc,
    const char* application_argv[]);
extern void NaClSelLdrShutdown(struct NaClSelLdrLauncher* launcher);
extern int NaClSelLdrOpenSrpcChannels(struct NaClSelLdrLauncher* launcher,
                                      struct NaClSrpcChannel* command,
                                      struct NaClSrpcChannel* untrusted);

extern NaClHandle NaClSelLdrGetChild(struct NaClSelLdrLauncher* launcher);
extern NaClHandle NaClSelLdrGetChannel(struct NaClSelLdrLauncher* launcher);
extern struct NaClDesc* NaClSelLdrGetSockAddr(
    struct NaClSelLdrLauncher* launcher);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_C_H_

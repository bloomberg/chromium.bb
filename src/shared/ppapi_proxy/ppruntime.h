/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPRUNTIME_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPRUNTIME_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"

EXTERN_C_BEGIN

// Initialize srpc connection to the browser. Some APIs like manifest file
// opening do not need full ppapi initialization and so can be used after
// this function returns.
int IrtInit();

// The entry point for the main thread of the PPAPI plugin process.
int PpapiPluginMain();

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs);

void PpapiPluginRegisterDefaultThreadCreator();

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPRUNTIME_H_

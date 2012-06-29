/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_BOOTSTRAP_CHANNEL_ERROR_REPORTER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_BOOTSTRAP_CHANNEL_ERROR_REPORTER_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

void NaClBootstrapChannelErrorReporterInit(void);

void NaClBootstrapChannelErrorReporter(void *state,
                                       char *buf,
                                       size_t buf_bytes);

EXTERN_C_END


#endif

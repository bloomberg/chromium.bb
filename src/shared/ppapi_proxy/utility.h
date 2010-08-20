/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_UTILITY_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_UTILITY_H_

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"

namespace ppapi_proxy {

void DebugPrintf(const char* format, ...);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_UTILITY_H_

/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_UTILITY_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_UTILITY_H_

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/shared/platform/nacl_check.h"

namespace ppapi_proxy {

void DebugPrintf(const char* format, ...);

// Check to see if |data| points to a valid UTF8 string.  Checks at most |len|
// characters. See http://tools.ietf.org/html/rfc3629 for details.
bool StringIsUtf8(const char* data, uint32_t len);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_UTILITY_H_

/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_CRYPTO_DEV_H_
#define PPAPI_C_DEV_PPB_CRYPTO_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_CRYPTO_DEV_INTERFACE_0_1 "PPB_Crypto(Dev);0.1"
#define PPB_CRYPTO_DEV_INTERFACE PPB_CRYPTO_DEV_INTERFACE_0_1

struct PPB_Crypto_Dev_0_1 {
  /**
   * Fills the given buffer with random bytes. This is potentially slow so only
   * request the amount of data you need.
   */
  void (*GetRandomBytes)(char* buffer, uint32_t num_bytes);
};

typedef struct PPB_Crypto_Dev_0_1 PPB_Crypto_Dev;

#endif

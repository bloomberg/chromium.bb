// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PP_SIZE_H_
#define PPAPI_C_PP_SIZE_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup PP
 * @{
 */

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

struct PP_Size {
  int32_t width;
  int32_t height;
};

PP_INLINE struct PP_Size PP_MakeSize(int32_t w, int32_t h) {
  struct PP_Size ret;
  ret.width = w;
  ret.height = h;
  return ret;
}

/**
 * @}
 * End addtogroup PP
 */
#endif  // PPAPI_C_PP_SIZE_H_

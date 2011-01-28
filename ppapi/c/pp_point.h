/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_POINT_H_
#define PPAPI_C_PP_POINT_H_

/**
 * @file
 * Defines the API ...
 */

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 *
 * @addtogroup Structs
 * @{
 */
struct PP_Point {
  int32_t x;
  int32_t y;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Point, 8);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */
PP_INLINE struct PP_Point PP_MakePoint(int32_t x, int32_t y) {
  struct PP_Point ret;
  ret.x = x;
  ret.y = y;
  return ret;
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_POINT_H_ */


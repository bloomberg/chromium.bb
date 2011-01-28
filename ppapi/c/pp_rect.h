/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_RECT_H_
#define PPAPI_C_PP_RECT_H_

/**
 * @file
 * Defines the API ...
 */

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @addtogroup Structs
 * @{
 */
struct PP_Rect {
  struct PP_Point point;
  struct PP_Size size;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Rect, 16);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */
PP_INLINE struct PP_Rect PP_MakeRectFromXYWH(int32_t x, int32_t y,
                                             int32_t w, int32_t h) {
  struct PP_Rect ret;
  ret.point.x = x;
  ret.point.y = y;
  ret.size.width = w;
  ret.size.height = h;
  return ret;
}
/**
 * @}
 */
#endif  /* PPAPI_C_PP_RECT_H_ */


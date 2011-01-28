/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_BOOL_H_
#define PPAPI_C_PP_BOOL_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * Defines the API ...
 */

/**
 *
 * @addtogroup Enums
 * @{
 */

/**
 * A boolean value for use in PPAPI C headers.  The standard bool type is not
 * available to pre-C99 compilers, and is not guaranteed to be compatible
 * between C and C++, whereas the PPAPI C headers can be included from C or C++
 * code.
 */
typedef enum {
  PP_FALSE = 0,
  PP_TRUE = 1
} PP_Bool;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Bool, 4);
/**
 * @}
 */





#endif  /* PPAPI_C_PP_BOOL_H_ */


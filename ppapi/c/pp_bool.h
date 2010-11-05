// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PP_BOOL_H_
#define PPAPI_C_PP_BOOL_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup PP
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
/**
 * @}
 * End addtogroup PP
 */

#endif  // PPAPI_C_PP_BOOL_H_


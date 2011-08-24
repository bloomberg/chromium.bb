/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen_range/versions.idl modified Wed Aug 24 10:35:01 2011. */

#ifndef PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_
#define PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * File Comment. */


/**
 * @addtogroup Structs
 * @{
 */
/* Bogus Struct */
struct PP_Size {
  /**
   * Comment for function
   */
  int32_t (*Foo)(int32_t x);
  /**
   * Comment for function
   */
  int32_t (*Foo)(int32_t x, int32_t y);
  /**
   * Comment for function
   */
  int32_t (*Foo)(int32_t x, int32_t y, int32_t z);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_ */


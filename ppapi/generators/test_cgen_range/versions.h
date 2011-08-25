/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen_range/versions.idl modified Wed Aug 24 19:49:19 2011. */

#ifndef PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_
#define PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_

#include "ppapi/c/pp_macros.h"

#define BAR_INTERFACE_0_0 "Bar;0.0"
#define BAR_INTERFACE_1_0 "Bar;1.0"
#define BAR_INTERFACE_2_0 "Bar;2.0"
#define BAR_INTERFACE BAR_INTERFACE_2_0

/**
 * @file
 * File Comment. */


/**
 * @addtogroup Structs
 * @{
 */
/* Bogus Struct Foo */
struct Foo {
  /**
   * Comment for function x,y,z
   */
  int32_t (*Foo)(int32_t x, int32_t y, int32_t z);
};
struct Foo_0_0 {
  int32_t (*Foo)(int32_t x);
};
struct Foo_1_0 {
  int32_t (*Foo)(int32_t x, int32_t y);
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/* Inherit revisions thanks to Foo */
struct Bar {
  /**
   * Comment for function
   */
  int32_t (*UseFoo)( struct Foo* val);
};
struct Bar_0_0 {
  int32_t (*UseFoo)( struct Foo* val);
};
struct Bar_1_0 {
  int32_t (*UseFoo)( struct Foo* val);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_RANGE_VERSIONS_H_ */


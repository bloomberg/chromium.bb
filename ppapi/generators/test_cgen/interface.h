/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From test_cgen/interface.idl modified Mon Aug 22 15:15:43 2011. */

#ifndef PPAPI_C_TEST_CGEN_INTERFACE_H_
#define PPAPI_C_TEST_CGEN_INTERFACE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/test_cgen/stdint.h"


/**
 * @file
 * This file will test that the IDL snippet matches the comment.
 */


/**
 * @addtogroup Structs
 * @{
 */
/* struct ist { void* X; }; */
struct ist {
  void* X;
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/*
 * struct iface1 {
 * int8_t (*mem1)(int16_t x, int32_t y);
 * int32_t (*mem2)(const struct ist* a);
 * int32_t (*mem3)(struct ist* b);
 * };
 */
struct iface1 {
  int8_t (*mem1)(int16_t x, int32_t y);
  int32_t (*mem2)(const struct ist* a);
  int32_t (*mem3)(struct ist* b);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TEST_CGEN_INTERFACE_H_ */


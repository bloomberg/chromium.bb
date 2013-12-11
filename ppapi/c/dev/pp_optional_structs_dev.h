/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/pp_optional_structs_dev.idl modified Tue Dec 10 17:39:38 2013. */

#ifndef PPAPI_C_DEV_PP_OPTIONAL_STRUCTS_DEV_H_
#define PPAPI_C_DEV_PP_OPTIONAL_STRUCTS_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines optional structs for primitive types.
 */


/**
 * @addtogroup Structs
 * @{
 */
struct PP_Optional_Double_Dev {
  double value;
  PP_Bool is_set;
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PP_OPTIONAL_STRUCTS_DEV_H_ */


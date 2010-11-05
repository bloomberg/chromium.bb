// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMMON_H_
#define PPAPI_CPP_COMMON_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup CPP
 * @{
 */

#include "ppapi/c/pp_bool.h"

namespace pp {

/** Convert a C++ bool to the appropriate PP_Bool value. */
inline PP_Bool BoolToPPBool(bool value) {
  return value ? PP_TRUE : PP_FALSE;
}

/** Convert a PP_Bool to a C++ bool value. */
inline bool PPBoolToBool(PP_Bool value) {
  return !!value;
}

}  // namespace pp

/**
 * @}
 * End addtogroup CPP
 */

#endif  // PPAPI_CPP_COMMON_H_


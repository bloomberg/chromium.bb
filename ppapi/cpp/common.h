// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMMON_H_
#define PPAPI_CPP_COMMON_H_


/// @file
/// Defines the API to convert PP_Bool to and from their C++ equivalent.

#include "ppapi/c/pp_bool.h"

namespace pp {

/// This function is used to convert a C++ bool to the appropriate PP_Bool
/// value.
/// @param[in] value A C++ boolean value.
/// @return A PP_Bool equivalent of the C++ boolean value.
inline PP_Bool BoolToPPBool(bool value) {
  return value ? PP_TRUE : PP_FALSE;
}

/// This function is used to convert a PP_Bool to a C++ boolean value.
/// value.
/// @param[in] value A PP_Bool boolean value.
/// @return A C++ equivalent of the PP_Bool boolean value.
inline bool PPBoolToBool(PP_Bool value) {
  return !!value;
}

}  // namespace pp

#endif  // PPAPI_CPP_COMMON_H_


// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMMON_H_
#define PPAPI_CPP_COMMON_H_


/// @file
/// Defines the API to convert <code>PP_Bool</code> to and from their C++
/// equivalent.

#include "ppapi/c/pp_bool.h"

namespace pp {

/// BoolToPPBool() is used to convert a C++ bool to the appropriate
/// <code>PP_Bool</code> value.
///
/// @param[in] value A C++ boolean value.
///
/// @return A <code>PP_Bool</code> equivalent of the C++ boolean value.
inline PP_Bool BoolToPPBool(bool value) {
  return value ? PP_TRUE : PP_FALSE;
}

/// PPBoolToBool() is used to convert a <code>PP_Bool</code> to a C++
/// boolean value.
///
/// @param[in] value A <code>PP_Bool</code> boolean value.
///
/// @return A C++ equivalent of the <code>PP_Bool</code> boolean value.
inline bool PPBoolToBool(PP_Bool value) {
  return !!value;
}

}  // namespace pp

#endif  // PPAPI_CPP_COMMON_H_


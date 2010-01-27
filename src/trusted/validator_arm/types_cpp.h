/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Some useful C++ type extensions

#ifndef NAT_CLIENT_PRIVATE_TOOLS_NCV_ARM_TYPES_CPP_H__
#define NAT_CLIENT_PRIVATE_TOOLS_NCV_ARM_TYPES_CPP_H__

#include "native_client/src/shared/utils/types.h"

// Convert bool to Bool
inline Bool ToBool(bool b) {
  return b ? TRUE : FALSE;
}

// Define negation on Bool
inline Bool operator!(Bool b) {
  return b ? FALSE : TRUE;
}

// Define and on Bool
inline Bool operator&&(Bool b1, Bool b2) {
  return b1 ? ToBool(b2) : FALSE;
}

// Define or on Bool
inline Bool operator||(Bool b1, Bool b2) {
  return b1 ? TRUE : ToBool(b2);
}

#endif  // NAT_CLIENT_PRIVATE_TOOLS_NCV_ARM_TYPES_CPP_H__

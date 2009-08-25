/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

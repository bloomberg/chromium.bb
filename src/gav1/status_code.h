/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_GAV1_STATUS_CODE_H_
#define LIBGAV1_SRC_GAV1_STATUS_CODE_H_

#include "gav1/symbol_visibility.h"

// All the declarations in this file are part of the public ABI. This file may
// be included by both C and C++ files.

// The Libgav1StatusCode enum type: A libgav1 function may return
// Libgav1StatusCode to indicate success or the reason for failure.
typedef enum {
  // Success.
  kLibgav1StatusOk = 0,

  // An unknown error. Used as the default error status if error detail is not
  // available.
  kLibgav1StatusUnknownError = -1,

  // An invalid function argument.
  kLibgav1StatusInvalidArgument = -2,

  // Memory allocation failure.
  kLibgav1StatusOutOfMemory = -3,

  // Ran out of a resource (other than memory).
  kLibgav1StatusResourceExhausted = -4,

  // The object is not initialized.
  kLibgav1StatusNotInitialized = -5,

  // An operation that can only be performed once has already been performed.
  kLibgav1StatusAlready = -6,

  // Not implemented, or not supported.
  kLibgav1StatusUnimplemented = -7,

  // An internal error in libgav1. Usually this indicates a programming error.
  kLibgav1StatusInternalError = -8,

  // The bitstream is not encoded correctly or violates a bitstream conformance
  // requirement.
  kLibgav1StatusBitstreamError = -9,

  // An extra enumerator to prevent people from writing code that fails to
  // compile when a new status code is added.
  //
  // Do not reference this enumerator. In particular, if you write code that
  // switches on Libgav1StatusCode, add a default: case instead of a case that
  // mentions this enumerator.
  //
  // Do not depend on the value (currently -1000) listed here. It may change in
  // the future.
  kLibgav1StatusReservedForFutureExpansionUseDefaultInSwitchInstead_ = -1000
} Libgav1StatusCode;

#if defined(__cplusplus)
namespace libgav1 {

// Declare type aliases for C++.
using StatusCode = Libgav1StatusCode;

// Returns a human readable error string in en-US for the status code |status|.
// Always returns a valid (non-NULL) string.
LIBGAV1_PUBLIC const char* GetErrorString(StatusCode status);

}  // namespace libgav1
#endif  // defined(__cplusplus)

#endif  // LIBGAV1_SRC_GAV1_STATUS_CODE_H_

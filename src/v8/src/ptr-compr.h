// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PTR_COMPR_H_
#define V8_PTR_COMPR_H_

#if V8_TARGET_ARCH_64_BIT

#include "src/globals.h"

namespace v8 {
namespace internal {

constexpr size_t kPtrComprHeapReservationSize = size_t{4} * GB;
constexpr size_t kPtrComprIsolateRootBias = kPtrComprHeapReservationSize / 2;
constexpr size_t kPtrComprIsolateRootAlignment = size_t{4} * GB;

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_64_BIT

#endif  // V8_PTR_COMPR_H_

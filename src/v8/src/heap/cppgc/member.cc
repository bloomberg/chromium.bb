// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/member.h"

namespace cppgc {
namespace internal {

#if defined(CPPGC_POINTER_COMPRESSION)
thread_local uintptr_t CageBaseGlobal::g_base_ =
    CageBaseGlobal::kLowerHalfWordMask;
#endif  // defined(CPPGC_POINTER_COMPRESSION)

}  // namespace internal
}  // namespace cppgc

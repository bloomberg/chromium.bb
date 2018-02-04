// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_MACOSX)

#include "platform/audio/cpu/x86/VectorMathSSE.h"

#include <xmmintrin.h>

namespace blink {
namespace VectorMath {
namespace SSE {

using MType = __m128;

}  // namespace SSE
}  // namespace VectorMath
}  // namespace blink

#define MM_PS(name) _mm_##name##_ps
#define VECTOR_MATH_SIMD_NAMESPACE_NAME SSE

#include "platform/audio/cpu/x86/VectorMathImpl.h"

#undef MM_PS
#undef VECTOR_MATH_SIMD_NAMESPACE_NAME

#endif  // defined(ARCH_CPU_X86_FAMILY) && !defined(OS_MACOSX)

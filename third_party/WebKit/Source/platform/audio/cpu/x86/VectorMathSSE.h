// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VectorMathSSE_h
#define VectorMathSSE_h

#include <xmmintrin.h>

namespace blink {
namespace VectorMath {
namespace SSE {

using MType = __m128;

constexpr size_t kBitsPerRegister = 128u;
constexpr size_t kPackedFloatsPerRegister = kBitsPerRegister / 32u;
constexpr size_t kFramesToProcessMask = ~(kPackedFloatsPerRegister - 1u);

bool IsAligned(const float* p);

// dest[k] = source1[k] + source2[k]
void Vadd(const float* source1p,
          const float* source2p,
          float* dest_p,
          size_t frames_to_process);

// dest[k] = clip(source[k], low_threshold, high_threshold)
//         = max(low_threshold, min(high_threshold, source[k]))
void Vclip(const float* source_p,
           const float* low_threshold_p,
           const float* high_threshold_p,
           float* dest_p,
           size_t frames_to_process);

void Vmaxmgv(const float* source_p, float* max_p, size_t frames_to_process);

// dest[k] = source1[k] * source2[k]
void Vmul(const float* source1p,
          const float* source2p,
          float* dest_p,
          size_t frames_to_process);

// dest[k] += scale * source[k]
void Vsma(const float* source_p,
          const float* scale,
          float* dest_p,
          size_t frames_to_process);

// dest[k] = scale * source[k]
void Vsmul(const float* source_p,
           const float* scale,
           float* dest_p,
           size_t frames_to_process);

// sum += sum(source[k]^2) for all k
void Vsvesq(const float* source_p, float* sum_p, size_t frames_to_process);

// real_dest_p[k] = real1[k] * real2[k] - imag1[k] * imag2[k]
// imag_dest_p[k] = real1[k] * imag2[k] + imag1[k] * real2[k]
void Zvmul(const float* real1p,
           const float* imag1p,
           const float* real2p,
           const float* imag2p,
           float* real_dest_p,
           float* imag_dest_p,
           size_t frames_to_process);

}  // namespace SSE
}  // namespace VectorMath
}  // namespace blink

#endif  // VectorMathSSE_h

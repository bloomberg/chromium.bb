// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VectorMathScalar_h
#define VectorMathScalar_h

#include <algorithm>
#include <cmath>

#include "platform/wtf/MathExtras.h"

namespace blink {
namespace VectorMath {
namespace Scalar {

static ALWAYS_INLINE void Vadd(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  while (frames_to_process > 0u) {
    *dest_p = *source1p + *source2p;
    source1p += source_stride1;
    source2p += source_stride2;
    dest_p += dest_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Vclip(const float* source_p,
                                int source_stride,
                                const float* low_threshold_p,
                                const float* high_threshold_p,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  while (frames_to_process > 0u) {
    *dest_p = clampTo(*source_p, *low_threshold_p, *high_threshold_p);
    source_p += source_stride;
    dest_p += dest_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Vmaxmgv(const float* source_p,
                                  int source_stride,
                                  float* max_p,
                                  size_t frames_to_process) {
  while (frames_to_process > 0u) {
    *max_p = std::max(*max_p, std::abs(*source_p));
    source_p += source_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Vmul(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  while (frames_to_process > 0u) {
    *dest_p = *source1p * *source2p;
    source1p += source_stride1;
    source2p += source_stride2;
    dest_p += dest_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Vsma(const float* source_p,
                               int source_stride,
                               const float* scale,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  const float k = *scale;
  while (frames_to_process > 0u) {
    *dest_p += k * *source_p;
    source_p += source_stride;
    dest_p += dest_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Vsmul(const float* source_p,
                                int source_stride,
                                const float* scale,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  const float k = *scale;
  while (frames_to_process > 0u) {
    *dest_p = k * *source_p;
    source_p += source_stride;
    dest_p += dest_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Vsvesq(const float* source_p,
                                 int source_stride,
                                 float* sum_p,
                                 size_t frames_to_process) {
  while (frames_to_process > 0u) {
    const float sample = *source_p;
    *sum_p += sample * sample;
    source_p += source_stride;
    --frames_to_process;
  }
}

static ALWAYS_INLINE void Zvmul(const float* real1p,
                                const float* imag1p,
                                const float* real2p,
                                const float* imag2p,
                                float* real_dest_p,
                                float* imag_dest_p,
                                size_t frames_to_process) {
  for (size_t i = 0u; i < frames_to_process; ++i) {
    // Read and compute result before storing them, in case the
    // destination is the same as one of the sources.
    float real_result = real1p[i] * real2p[i] - imag1p[i] * imag2p[i];
    float imag_result = real1p[i] * imag2p[i] + imag1p[i] * real2p[i];

    real_dest_p[i] = real_result;
    imag_dest_p[i] = imag_result;
  }
}

}  // namespace Scalar
}  // namespace VectorMath
}  // namespace blink

#endif  // VectorMathScalar_h

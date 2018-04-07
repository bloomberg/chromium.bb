// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_ARM_VECTOR_MATH_NEON_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_ARM_VECTOR_MATH_NEON_H_

#include <arm_neon.h>

#include <algorithm>

#include "third_party/blink/renderer/platform/audio/vector_math_scalar.h"

namespace blink {
namespace VectorMath {
namespace NEON {

// TODO: Consider optimizing this.
using Scalar::Conv;

static ALWAYS_INLINE void Vadd(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride1 == 1 && source_stride2 == 1 && dest_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = dest_p + n - tail_frames;

    while (dest_p < end_p) {
      float32x4_t source1 = vld1q_f32(source1p);
      float32x4_t source2 = vld1q_f32(source2p);
      vst1q_f32(dest_p, vaddq_f32(source1, source2));

      source1p += 4;
      source2p += 4;
      dest_p += 4;
    }
    n = tail_frames;
  }

  Scalar::Vadd(source1p, source_stride1, source2p, source_stride2, dest_p,
               dest_stride, n);
}

static ALWAYS_INLINE void Vclip(const float* source_p,
                                int source_stride,
                                const float* low_threshold_p,
                                const float* high_threshold_p,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride == 1 && dest_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = dest_p + n - tail_frames;

    float32x4_t low = vdupq_n_f32(*low_threshold_p);
    float32x4_t high = vdupq_n_f32(*high_threshold_p);
    while (dest_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      vst1q_f32(dest_p, vmaxq_f32(vminq_f32(source, high), low));
      source_p += 4;
      dest_p += 4;
    }
    n = tail_frames;
  }

  Scalar::Vclip(source_p, source_stride, low_threshold_p, high_threshold_p,
                dest_p, dest_stride, n);
}

static ALWAYS_INLINE void Vmaxmgv(const float* source_p,
                                  int source_stride,
                                  float* max_p,
                                  size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = source_p + n - tail_frames;

    float32x4_t four_max = vdupq_n_f32(*max_p);
    while (source_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      four_max = vmaxq_f32(four_max, vabsq_f32(source));
      source_p += 4;
    }
    float32x2_t two_max =
        vmax_f32(vget_low_f32(four_max), vget_high_f32(four_max));

    float group_max[2];
    vst1_f32(group_max, two_max);
    *max_p = std::max(group_max[0], group_max[1]);

    n = tail_frames;
  }

  Scalar::Vmaxmgv(source_p, source_stride, max_p, n);
}

static ALWAYS_INLINE void Vmul(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride1 == 1 && source_stride2 == 1 && dest_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = dest_p + n - tail_frames;

    while (dest_p < end_p) {
      float32x4_t source1 = vld1q_f32(source1p);
      float32x4_t source2 = vld1q_f32(source2p);
      vst1q_f32(dest_p, vmulq_f32(source1, source2));

      source1p += 4;
      source2p += 4;
      dest_p += 4;
    }
    n = tail_frames;
  }

  Scalar::Vmul(source1p, source_stride1, source2p, source_stride2, dest_p,
               dest_stride, n);
}

static ALWAYS_INLINE void Vsma(const float* source_p,
                               int source_stride,
                               const float* scale,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride == 1 && dest_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = dest_p + n - tail_frames;

    float32x4_t k = vdupq_n_f32(*scale);
    while (dest_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      float32x4_t dest = vld1q_f32(dest_p);

      dest = vmlaq_f32(dest, source, k);
      vst1q_f32(dest_p, dest);

      source_p += 4;
      dest_p += 4;
    }
    n = tail_frames;
  }

  Scalar::Vsma(source_p, source_stride, scale, dest_p, dest_stride, n);
}

static ALWAYS_INLINE void Vsmul(const float* source_p,
                                int source_stride,
                                const float* scale,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride == 1 && dest_stride == 1) {
    float k = *scale;
    int tail_frames = n % 4;
    const float* end_p = dest_p + n - tail_frames;

    while (dest_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      vst1q_f32(dest_p, vmulq_n_f32(source, k));

      source_p += 4;
      dest_p += 4;
    }
    n = tail_frames;
  }

  Scalar::Vsmul(source_p, source_stride, scale, dest_p, dest_stride, n);
}

static ALWAYS_INLINE void Vsvesq(const float* source_p,
                                 int source_stride,
                                 float* sum_p,
                                 size_t frames_to_process) {
  int n = frames_to_process;

  if (source_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = source_p + n - tail_frames;

    float32x4_t four_sum = vdupq_n_f32(0);
    while (source_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      four_sum = vmlaq_f32(four_sum, source, source);
      source_p += 4;
    }
    float32x2_t two_sum =
        vadd_f32(vget_low_f32(four_sum), vget_high_f32(four_sum));

    float group_sum[2];
    vst1_f32(group_sum, two_sum);
    *sum_p += group_sum[0] + group_sum[1];

    n = tail_frames;
  }

  Scalar::Vsvesq(source_p, source_stride, sum_p, n);
}

static ALWAYS_INLINE void Zvmul(const float* real1p,
                                const float* imag1p,
                                const float* real2p,
                                const float* imag2p,
                                float* real_dest_p,
                                float* imag_dest_p,
                                size_t frames_to_process) {
  unsigned i = 0;

  unsigned end_size = frames_to_process - frames_to_process % 4;
  while (i < end_size) {
    float32x4_t real1 = vld1q_f32(real1p + i);
    float32x4_t real2 = vld1q_f32(real2p + i);
    float32x4_t imag1 = vld1q_f32(imag1p + i);
    float32x4_t imag2 = vld1q_f32(imag2p + i);

    float32x4_t real_result = vmlsq_f32(vmulq_f32(real1, real2), imag1, imag2);
    float32x4_t imag_result = vmlaq_f32(vmulq_f32(real1, imag2), imag1, real2);

    vst1q_f32(real_dest_p + i, real_result);
    vst1q_f32(imag_dest_p + i, imag_result);

    i += 4;
  }

  Scalar::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
                imag_dest_p + i, frames_to_process - i);
}

}  // namespace NEON
}  // namespace VectorMath
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_ARM_VECTOR_MATH_NEON_H_

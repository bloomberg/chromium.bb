// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VectorMathX86_h
#define VectorMathX86_h

#include "platform/audio/VectorMathScalar.h"
#include "platform/audio/cpu/x86/VectorMathSSE.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/MathExtras.h"

namespace blink {
namespace VectorMath {
namespace X86 {

static ALWAYS_INLINE void Vadd(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride1 == 1 && source_stride2 == 1 && dest_stride == 1) {
    // If the source1p address is not 16-byte aligned, the first several frames
    // (at most three) should be processed separately.
    for (; !SSE::IsAligned(source1p + i) && i < frames_to_process; ++i)
      dest_p[i] = source1p[i] + source2p[i];

    // Now the source1p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vadd(source1p + i, source2p + i, dest_p + i, sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vadd(source1p + i, source_stride1, source2p + i, source_stride2,
               dest_p + i, dest_stride, frames_to_process - i);
}

static ALWAYS_INLINE void Vclip(const float* source_p,
                                int source_stride,
                                const float* low_threshold_p,
                                const float* high_threshold_p,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride == 1 && dest_stride == 1) {
    // If the source_p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      dest_p[i] = clampTo(source_p[i], *low_threshold_p, *high_threshold_p);

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vclip(source_p + i, low_threshold_p, high_threshold_p, dest_p + i,
                 sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vclip(source_p + i, source_stride, low_threshold_p, high_threshold_p,
                dest_p + i, dest_stride, frames_to_process - i);
}

static ALWAYS_INLINE void Vmaxmgv(const float* source_p,
                                  int source_stride,
                                  float* max_p,
                                  size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride == 1) {
    // If the source_p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      *max_p = std::max(*max_p, fabsf(source_p[i]));

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vmaxmgv(source_p + i, max_p, sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vmaxmgv(source_p + i, source_stride, max_p, frames_to_process - i);
}

static ALWAYS_INLINE void Vmul(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride1 == 1 && source_stride2 == 1 && dest_stride == 1) {
    // If the source1p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source1p + i) && i < frames_to_process; ++i)
      dest_p[i] = source1p[i] * source2p[i];

    // Now the source1p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vmul(source1p + i, source2p + i, dest_p + i, sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vmul(source1p + i, source_stride1, source2p + i, source_stride2,
               dest_p + i, dest_stride, frames_to_process - i);
}

static ALWAYS_INLINE void Vsma(const float* source_p,
                               int source_stride,
                               const float* scale,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride == 1 && dest_stride == 1) {
    // If the source_p address is not 16-byte aligned, the first several frames
    // (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      dest_p[i] += *scale * source_p[i];

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vsma(source_p + i, scale, dest_p + i, sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vsma(source_p + i, source_stride, scale, dest_p + i, dest_stride,
               frames_to_process - i);
}

static ALWAYS_INLINE void Vsmul(const float* source_p,
                                int source_stride,
                                const float* scale,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride == 1 && dest_stride == 1) {
    // If the source_p address is not 16-byte aligned, the first several frames
    // (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      dest_p[i] = *scale * source_p[i];

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vsmul(source_p + i, scale, dest_p + i, sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vsmul(source_p + i, source_stride, scale, dest_p + i, dest_stride,
                frames_to_process - i);
}

static ALWAYS_INLINE void Vsvesq(const float* source_p,
                                 int source_stride,
                                 float* sum_p,
                                 size_t frames_to_process) {
  size_t i = 0u;

  if (source_stride == 1) {
    // If the source_p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      *sum_p += source_p[i] * source_p[i];

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vsvesq(source_p + i, sum_p, sse_frames_to_process);
      i += sse_frames_to_process;
    }
  }

  Scalar::Vsvesq(source_p + i, source_stride, sum_p, frames_to_process - i);
}

static ALWAYS_INLINE void Zvmul(const float* real1p,
                                const float* imag1p,
                                const float* real2p,
                                const float* imag2p,
                                float* real_dest_p,
                                float* imag_dest_p,
                                size_t frames_to_process) {
  size_t i = 0u;

  // If the real1p address is not 16-byte aligned, the first several
  // frames  (at most three) should be processed separately.
  for (; !SSE::IsAligned(real1p + i) && i < frames_to_process; ++i) {
    // Read and compute result before storing them, in case the
    // destination is the same as one of the sources.
    float real_result = real1p[i] * real2p[i] - imag1p[i] * imag2p[i];
    float imag_result = real1p[i] * imag2p[i] + imag1p[i] * real2p[i];

    real_dest_p[i] = real_result;
    imag_dest_p[i] = imag_result;
  }

  // Now the real1p+i address is 16-byte aligned. Start to apply SSE.
  size_t sse_frames_to_process =
      (frames_to_process - i) & SSE::kFramesToProcessMask;
  if (sse_frames_to_process > 0u) {
    SSE::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
               imag_dest_p + i, sse_frames_to_process);
    i += sse_frames_to_process;
  }

  Scalar::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
                imag_dest_p + i, frames_to_process - i);
}

}  // namespace X86
}  // namespace VectorMath
}  // namespace blink

#endif  // VectorMathX86_h

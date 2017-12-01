/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "platform/audio/VectorMath.h"

#include <stdint.h>
#include "build/build_config.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CPU.h"
#include "platform/wtf/MathExtras.h"

#if defined(OS_MACOSX)
#include <Accelerate/Accelerate.h>
#endif

#if defined(ARCH_CPU_X86_FAMILY)
#include <emmintrin.h>
#endif

#if WTF_CPU_ARM_NEON
#include <arm_neon.h>
#endif

#if HAVE_MIPS_MSA_INTRINSICS
#include "platform/cpu/mips/CommonMacrosMSA.h"
#endif

#include <math.h>
#include <algorithm>

#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_MACOSX)
// Define the blink::VectorMath::SSE namespace.
#include "platform/audio/cpu/x86/VectorMathImpl.cpp"
#endif

namespace blink {

namespace VectorMath {

#if defined(OS_MACOSX)
// On the Mac we use the highly optimized versions in Accelerate.framework
// In 32-bit mode (__ppc__ or __i386__) <Accelerate/Accelerate.h> includes
// <vecLib/vDSP_translate.h> which defines macros of the same name as
// our namespaced function names, so we must handle this case differently. Other
// architectures (64bit, ARM, etc.) do not include this header file.

void Vsmul(const float* source_p,
           int source_stride,
           const float* scale,
           float* dest_p,
           int dest_stride,
           size_t frames_to_process) {
#if defined(ARCH_CPU_X86)
  ::vsmul(source_p, source_stride, scale, dest_p, dest_stride,
          frames_to_process);
#else
  vDSP_vsmul(source_p, source_stride, scale, dest_p, dest_stride,
             frames_to_process);
#endif
}

void Vadd(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
#if defined(ARCH_CPU_X86)
  ::vadd(source1p, source_stride1, source2p, source_stride2, dest_p,
         dest_stride, frames_to_process);
#else
  vDSP_vadd(source1p, source_stride1, source2p, source_stride2, dest_p,
            dest_stride, frames_to_process);
#endif
}

void Vmul(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
#if defined(ARCH_CPU_X86)
  ::vmul(source1p, source_stride1, source2p, source_stride2, dest_p,
         dest_stride, frames_to_process);
#else
  vDSP_vmul(source1p, source_stride1, source2p, source_stride2, dest_p,
            dest_stride, frames_to_process);
#endif
}

void Zvmul(const float* real1p,
           const float* imag1p,
           const float* real2p,
           const float* imag2p,
           float* real_dest_p,
           float* imag_dest_p,
           size_t frames_to_process) {
  DSPSplitComplex sc1;
  DSPSplitComplex sc2;
  DSPSplitComplex dest;
  sc1.realp = const_cast<float*>(real1p);
  sc1.imagp = const_cast<float*>(imag1p);
  sc2.realp = const_cast<float*>(real2p);
  sc2.imagp = const_cast<float*>(imag2p);
  dest.realp = real_dest_p;
  dest.imagp = imag_dest_p;
#if defined(ARCH_CPU_X86)
  ::zvmul(&sc1, 1, &sc2, 1, &dest, 1, frames_to_process, 1);
#else
  vDSP_zvmul(&sc1, 1, &sc2, 1, &dest, 1, frames_to_process, 1);
#endif
}

void Vsma(const float* source_p,
          int source_stride,
          const float* scale,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  vDSP_vsma(source_p, source_stride, scale, dest_p, dest_stride, dest_p,
            dest_stride, frames_to_process);
}

void Vmaxmgv(const float* source_p,
             int source_stride,
             float* max_p,
             size_t frames_to_process) {
  vDSP_maxmgv(source_p, source_stride, max_p, frames_to_process);
}

void Vsvesq(const float* source_p,
            int source_stride,
            float* sum_p,
            size_t frames_to_process) {
  vDSP_svesq(const_cast<float*>(source_p), source_stride, sum_p,
             frames_to_process);
}

void Vclip(const float* source_p,
           int source_stride,
           const float* low_threshold_p,
           const float* high_threshold_p,
           float* dest_p,
           int dest_stride,
           size_t frames_to_process) {
  vDSP_vclip(const_cast<float*>(source_p), source_stride,
             const_cast<float*>(low_threshold_p),
             const_cast<float*>(high_threshold_p), dest_p, dest_stride,
             frames_to_process);
}
#else

void Vsma(const float* source_p,
          int source_stride,
          const float* scale,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  int n = frames_to_process;

#if defined(ARCH_CPU_X86_FAMILY)
  if ((source_stride == 1) && (dest_stride == 1)) {
    size_t i = 0u;

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

    source_p += i;
    dest_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
  if ((source_stride == 1) && (dest_stride == 1)) {
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
#elif HAVE_MIPS_MSA_INTRINSICS
  if ((source_stride == 1) && (dest_stride == 1)) {
    float* destPCopy = dest_p;
    v4f32 vScale;
    v4f32 vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6, vSrc7;
    v4f32 vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7;
    FloatInt scaleVal;

    scaleVal.floatVal = *scale;
    vScale = (v4f32)__msa_fill_w(scaleVal.intVal);

    for (; n >= 32; n -= 32) {
      LD_SP8(source_p, 4, vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6,
             vSrc7);
      LD_SP8(destPCopy, 4, vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6,
             vDst7);
      VSMA4(vSrc0, vSrc1, vSrc2, vSrc3, vDst0, vDst1, vDst2, vDst3, vScale);
      VSMA4(vSrc4, vSrc5, vSrc6, vSrc7, vDst4, vDst5, vDst6, vDst7, vScale);
      ST_SP8(vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7, dest_p, 4);
    }
  }
#endif
  while (n) {
    *dest_p += *source_p * *scale;
    source_p += source_stride;
    dest_p += dest_stride;
    n--;
  }
}

void Vsmul(const float* source_p,
           int source_stride,
           const float* scale,
           float* dest_p,
           int dest_stride,
           size_t frames_to_process) {
  int n = frames_to_process;

#if defined(ARCH_CPU_X86_FAMILY)
  if ((source_stride == 1) && (dest_stride == 1)) {
    size_t i = 0u;

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

    source_p += i;
    dest_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
  if ((source_stride == 1) && (dest_stride == 1)) {
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
#elif HAVE_MIPS_MSA_INTRINSICS
  if ((source_stride == 1) && (dest_stride == 1)) {
    v4f32 vScale;
    v4f32 vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6, vSrc7;
    v4f32 vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7;
    FloatInt scaleVal;

    scaleVal.floatVal = *scale;
    vScale = (v4f32)__msa_fill_w(scaleVal.intVal);

    for (; n >= 32; n -= 32) {
      LD_SP8(source_p, 4, vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6,
             vSrc7);
      VSMUL4(vSrc0, vSrc1, vSrc2, vSrc3, vDst0, vDst1, vDst2, vDst3, vScale);
      VSMUL4(vSrc4, vSrc5, vSrc6, vSrc7, vDst4, vDst5, vDst6, vDst7, vScale);
      ST_SP8(vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7, dest_p, 4);
    }
  }
#endif
  float k = *scale;
  while (n--) {
    *dest_p = k * *source_p;
    source_p += source_stride;
    dest_p += dest_stride;
  }
}

void Vadd(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  int n = frames_to_process;

#if defined(ARCH_CPU_X86_FAMILY)
  if ((source_stride1 == 1) && (source_stride2 == 1) && (dest_stride == 1)) {
    size_t i = 0u;

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

    source1p += i;
    source2p += i;
    dest_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
  if ((source_stride1 == 1) && (source_stride2 == 1) && (dest_stride == 1)) {
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
#elif HAVE_MIPS_MSA_INTRINSICS
  if ((source_stride1 == 1) && (source_stride2 == 1) && (dest_stride == 1)) {
    v4f32 vSrc1P0, vSrc1P1, vSrc1P2, vSrc1P3, vSrc1P4, vSrc1P5, vSrc1P6,
        vSrc1P7;
    v4f32 vSrc2P0, vSrc2P1, vSrc2P2, vSrc2P3, vSrc2P4, vSrc2P5, vSrc2P6,
        vSrc2P7;
    v4f32 vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7;

    for (; n >= 32; n -= 32) {
      LD_SP8(source1p, 4, vSrc1P0, vSrc1P1, vSrc1P2, vSrc1P3, vSrc1P4, vSrc1P5,
             vSrc1P6, vSrc1P7);
      LD_SP8(source2p, 4, vSrc2P0, vSrc2P1, vSrc2P2, vSrc2P3, vSrc2P4, vSrc2P5,
             vSrc2P6, vSrc2P7);
      ADD4(vSrc1P0, vSrc2P0, vSrc1P1, vSrc2P1, vSrc1P2, vSrc2P2, vSrc1P3,
           vSrc2P3, vDst0, vDst1, vDst2, vDst3);
      ADD4(vSrc1P4, vSrc2P4, vSrc1P5, vSrc2P5, vSrc1P6, vSrc2P6, vSrc1P7,
           vSrc2P7, vDst4, vDst5, vDst6, vDst7);
      ST_SP8(vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7, dest_p, 4);
    }
  }
#endif
  while (n--) {
    *dest_p = *source1p + *source2p;
    source1p += source_stride1;
    source2p += source_stride2;
    dest_p += dest_stride;
  }
}

void Vmul(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  int n = frames_to_process;

#if defined(ARCH_CPU_X86_FAMILY)
  if ((source_stride1 == 1) && (source_stride2 == 1) && (dest_stride == 1)) {
    size_t i = 0u;

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

    source1p += i;
    source2p += i;
    dest_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
  if ((source_stride1 == 1) && (source_stride2 == 1) && (dest_stride == 1)) {
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
#elif HAVE_MIPS_MSA_INTRINSICS
  if ((source_stride1 == 1) && (source_stride2 == 1) && (dest_stride == 1)) {
    v4f32 vSrc1P0, vSrc1P1, vSrc1P2, vSrc1P3, vSrc1P4, vSrc1P5, vSrc1P6,
        vSrc1P7;
    v4f32 vSrc2P0, vSrc2P1, vSrc2P2, vSrc2P3, vSrc2P4, vSrc2P5, vSrc2P6,
        vSrc2P7;
    v4f32 vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7;

    for (; n >= 32; n -= 32) {
      LD_SP8(source1p, 4, vSrc1P0, vSrc1P1, vSrc1P2, vSrc1P3, vSrc1P4, vSrc1P5,
             vSrc1P6, vSrc1P7);
      LD_SP8(source2p, 4, vSrc2P0, vSrc2P1, vSrc2P2, vSrc2P3, vSrc2P4, vSrc2P5,
             vSrc2P6, vSrc2P7);
      MUL4(vSrc1P0, vSrc2P0, vSrc1P1, vSrc2P1, vSrc1P2, vSrc2P2, vSrc1P3,
           vSrc2P3, vDst0, vDst1, vDst2, vDst3);
      MUL4(vSrc1P4, vSrc2P4, vSrc1P5, vSrc2P5, vSrc1P6, vSrc2P6, vSrc1P7,
           vSrc2P7, vDst4, vDst5, vDst6, vDst7);
      ST_SP8(vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7, dest_p, 4);
    }
  }
#endif
  while (n) {
    *dest_p = *source1p * *source2p;
    source1p += source_stride1;
    source2p += source_stride2;
    dest_p += dest_stride;
    n--;
  }
}

void Zvmul(const float* real1p,
           const float* imag1p,
           const float* real2p,
           const float* imag2p,
           float* real_dest_p,
           float* imag_dest_p,
           size_t frames_to_process) {
  unsigned i = 0;
#if defined(ARCH_CPU_X86_FAMILY)
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
#elif WTF_CPU_ARM_NEON
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
#endif
  for (; i < frames_to_process; ++i) {
    // Read and compute result before storing them, in case the
    // destination is the same as one of the sources.
    float real_result = real1p[i] * real2p[i] - imag1p[i] * imag2p[i];
    float imag_result = real1p[i] * imag2p[i] + imag1p[i] * real2p[i];

    real_dest_p[i] = real_result;
    imag_dest_p[i] = imag_result;
  }
}

void Vsvesq(const float* source_p,
            int source_stride,
            float* sum_p,
            size_t frames_to_process) {
  int n = frames_to_process;
  float sum = 0;

#if defined(ARCH_CPU_X86_FAMILY)
  if (source_stride == 1) {
    size_t i = 0u;

    // If the source_p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      sum += source_p[i] * source_p[i];

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vsvesq(source_p + i, &sum, sse_frames_to_process);
      i += sse_frames_to_process;
    }

    source_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
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
    sum += group_sum[0] + group_sum[1];

    n = tail_frames;
  }
#endif

  while (n--) {
    float sample = *source_p;
    sum += sample * sample;
    source_p += source_stride;
  }

  DCHECK(sum_p);
  *sum_p = sum;
}

void Vmaxmgv(const float* source_p,
             int source_stride,
             float* max_p,
             size_t frames_to_process) {
  int n = frames_to_process;
  float max = 0;

#if defined(ARCH_CPU_X86_FAMILY)
  if (source_stride == 1) {
    size_t i = 0u;

    // If the source_p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      max = std::max(max, fabsf(source_p[i]));

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vmaxmgv(source_p + i, &max, sse_frames_to_process);
      i += sse_frames_to_process;
    }

    source_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
  if (source_stride == 1) {
    int tail_frames = n % 4;
    const float* end_p = source_p + n - tail_frames;

    float32x4_t four_max = vdupq_n_f32(0);
    while (source_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      four_max = vmaxq_f32(four_max, vabsq_f32(source));
      source_p += 4;
    }
    float32x2_t two_max =
        vmax_f32(vget_low_f32(four_max), vget_high_f32(four_max));

    float group_max[2];
    vst1_f32(group_max, two_max);
    max = std::max(group_max[0], group_max[1]);

    n = tail_frames;
  }
#elif HAVE_MIPS_MSA_INTRINSICS
  if (source_stride == 1) {
    v4f32 vMax = {
        0,
    };
    v4f32 vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6, vSrc7;
    const v16i8 vSignBitMask = (v16i8)__msa_fill_w(0x7FFFFFFF);

    for (; n >= 32; n -= 32) {
      LD_SP8(source_p, 4, vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6,
             vSrc7);
      AND_W4_SP(vSrc0, vSrc1, vSrc2, vSrc3, vSignBitMask);
      VMAX_W4_SP(vSrc0, vSrc1, vSrc2, vSrc3, vMax);
      AND_W4_SP(vSrc4, vSrc5, vSrc6, vSrc7, vSignBitMask);
      VMAX_W4_SP(vSrc4, vSrc5, vSrc6, vSrc7, vMax);
    }

    max = std::max(max, vMax[0]);
    max = std::max(max, vMax[1]);
    max = std::max(max, vMax[2]);
    max = std::max(max, vMax[3]);
  }
#endif

  while (n--) {
    max = std::max(max, fabsf(*source_p));
    source_p += source_stride;
  }

  DCHECK(max_p);
  *max_p = max;
}

void Vclip(const float* source_p,
           int source_stride,
           const float* low_threshold_p,
           const float* high_threshold_p,
           float* dest_p,
           int dest_stride,
           size_t frames_to_process) {
  int n = frames_to_process;
  float low_threshold = *low_threshold_p;
  float high_threshold = *high_threshold_p;

#if DCHECK_IS_ON()
  // Do the same DCHECKs that |clampTo| would do so that optimization paths do
  // not have to do them.
  for (size_t i = 0u; i < frames_to_process; ++i)
    DCHECK(!std::isnan(source_p[i]));
  // This also ensures that thresholds are not NaNs.
  DCHECK_LE(low_threshold, high_threshold);
#endif

#if defined(ARCH_CPU_X86_FAMILY)
  if (source_stride == 1 && dest_stride == 1) {
    size_t i = 0u;

    // If the source_p address is not 16-byte aligned, the first several
    // frames  (at most three) should be processed separately.
    for (; !SSE::IsAligned(source_p + i) && i < frames_to_process; ++i)
      dest_p[i] = clampTo(source_p[i], low_threshold, high_threshold);

    // Now the source_p+i address is 16-byte aligned. Start to apply SSE.
    size_t sse_frames_to_process =
        (frames_to_process - i) & SSE::kFramesToProcessMask;
    if (sse_frames_to_process > 0u) {
      SSE::Vclip(source_p + i, &low_threshold, &high_threshold, dest_p + i,
                 sse_frames_to_process);
      i += sse_frames_to_process;
    }

    source_p += i;
    dest_p += i;
    n -= i;
  }
#elif WTF_CPU_ARM_NEON
  if ((source_stride == 1) && (dest_stride == 1)) {
    int tail_frames = n % 4;
    const float* end_p = dest_p + n - tail_frames;

    float32x4_t low = vdupq_n_f32(low_threshold);
    float32x4_t high = vdupq_n_f32(high_threshold);
    while (dest_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      vst1q_f32(dest_p, vmaxq_f32(vminq_f32(source, high), low));
      source_p += 4;
      dest_p += 4;
    }
    n = tail_frames;
  }
#elif HAVE_MIPS_MSA_INTRINSICS
  if ((source_stride == 1) && (dest_stride == 1)) {
    v4f32 vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6, vSrc7;
    v4f32 vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7;
    v4f32 vLowThr, vHighThr;
    FloatInt lowThr, highThr;

    lowThr.floatVal = low_threshold;
    highThr.floatVal = high_threshold;
    vLowThr = (v4f32)__msa_fill_w(lowThr.intVal);
    vHighThr = (v4f32)__msa_fill_w(highThr.intVal);

    for (; n >= 32; n -= 32) {
      LD_SP8(source_p, 4, vSrc0, vSrc1, vSrc2, vSrc3, vSrc4, vSrc5, vSrc6,
             vSrc7);
      VCLIP4(vSrc0, vSrc1, vSrc2, vSrc3, vLowThr, vHighThr, vDst0, vDst1, vDst2,
             vDst3);
      VCLIP4(vSrc4, vSrc5, vSrc6, vSrc7, vLowThr, vHighThr, vDst4, vDst5, vDst6,
             vDst7);
      ST_SP8(vDst0, vDst1, vDst2, vDst3, vDst4, vDst5, vDst6, vDst7, dest_p, 4);
    }
  }
#endif
  while (n--) {
    *dest_p = clampTo(*source_p, low_threshold, high_threshold);
    source_p += source_stride;
    dest_p += dest_stride;
  }
}

#endif  // defined(OS_MACOSX)

}  // namespace VectorMath

}  // namespace blink

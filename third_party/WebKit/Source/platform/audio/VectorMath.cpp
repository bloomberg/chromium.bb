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

#include <cmath>

#include "build/build_config.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CPU.h"

#if defined(OS_MACOSX)
#include "platform/audio/mac/VectorMathMac.h"
#elif WTF_CPU_ARM_NEON
#include "platform/audio/cpu/arm/VectorMathNEON.h"
#elif defined(ARCH_CPU_X86_FAMILY)
#include "platform/audio/cpu/x86/VectorMathX86.h"
#else
#include "platform/audio/VectorMathScalar.h"
#endif

#if HAVE_MIPS_MSA_INTRINSICS
#include "platform/cpu/mips/CommonMacrosMSA.h"
#endif

#include <algorithm>

namespace blink {

namespace VectorMath {

namespace {
#if defined(OS_MACOSX)
namespace Impl = Mac;
#elif WTF_CPU_ARM_NEON
namespace Impl = NEON;
#elif defined(ARCH_CPU_X86_FAMILY)
namespace Impl = X86;
#else
namespace Impl = Scalar;
#endif
}  // namespace

void Vsma(const float* source_p,
          int source_stride,
          const float* scale,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
#if HAVE_MIPS_MSA_INTRINSICS
  int n = frames_to_process;

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

  frames_to_process = n;
#endif

  Impl::Vsma(source_p, source_stride, scale, dest_p, dest_stride,
             frames_to_process);
}

void Vsmul(const float* source_p,
           int source_stride,
           const float* scale,
           float* dest_p,
           int dest_stride,
           size_t frames_to_process) {
#if HAVE_MIPS_MSA_INTRINSICS
  int n = frames_to_process;

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

  frames_to_process = n;
#endif

  Impl::Vsmul(source_p, source_stride, scale, dest_p, dest_stride,
              frames_to_process);
}

void Vadd(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
#if HAVE_MIPS_MSA_INTRINSICS
  int n = frames_to_process;

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

  frames_to_process = n;
#endif

  Impl::Vadd(source1p, source_stride1, source2p, source_stride2, dest_p,
             dest_stride, frames_to_process);
}

void Vmul(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
#if HAVE_MIPS_MSA_INTRINSICS
  int n = frames_to_process;

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

  frames_to_process = n;
#endif

  Impl::Vmul(source1p, source_stride1, source2p, source_stride2, dest_p,
             dest_stride, frames_to_process);
}

void Zvmul(const float* real1p,
           const float* imag1p,
           const float* real2p,
           const float* imag2p,
           float* real_dest_p,
           float* imag_dest_p,
           size_t frames_to_process) {
  Impl::Zvmul(real1p, imag1p, real2p, imag2p, real_dest_p, imag_dest_p,
              frames_to_process);
}

void Vsvesq(const float* source_p,
            int source_stride,
            float* sum_p,
            size_t frames_to_process) {
  float sum = 0;

  Impl::Vsvesq(source_p, source_stride, &sum, frames_to_process);

  DCHECK(sum_p);
  *sum_p = sum;
}

void Vmaxmgv(const float* source_p,
             int source_stride,
             float* max_p,
             size_t frames_to_process) {
  float max = 0;

#if HAVE_MIPS_MSA_INTRINSICS
  int n = frames_to_process;

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

  frames_to_process = n;
#endif

  Impl::Vmaxmgv(source_p, source_stride, &max, frames_to_process);

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

#if HAVE_MIPS_MSA_INTRINSICS
  int n = frames_to_process;

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

  frames_to_process = n;
#endif

  Impl::Vclip(source_p, source_stride, &low_threshold, &high_threshold, dest_p,
              dest_stride, frames_to_process);
}

}  // namespace VectorMath

}  // namespace blink

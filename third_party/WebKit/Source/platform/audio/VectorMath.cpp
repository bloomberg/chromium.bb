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
#elif HAVE_MIPS_MSA_INTRINSICS
#include "platform/audio/cpu/mips/VectorMathMSA.h"
#elif defined(ARCH_CPU_X86_FAMILY)
#include "platform/audio/cpu/x86/VectorMathX86.h"
#else
#include "platform/audio/VectorMathScalar.h"
#endif

namespace blink {

namespace VectorMath {

namespace {
#if defined(OS_MACOSX)
namespace Impl = Mac;
#elif WTF_CPU_ARM_NEON
namespace Impl = NEON;
#elif HAVE_MIPS_MSA_INTRINSICS
namespace Impl = MSA;
#elif defined(ARCH_CPU_X86_FAMILY)
namespace Impl = X86;
#else
namespace Impl = Scalar;
#endif
}  // namespace

void Vadd(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  Impl::Vadd(source1p, source_stride1, source2p, source_stride2, dest_p,
             dest_stride, frames_to_process);
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

  Impl::Vclip(source_p, source_stride, &low_threshold, &high_threshold, dest_p,
              dest_stride, frames_to_process);
}

void Vmaxmgv(const float* source_p,
             int source_stride,
             float* max_p,
             size_t frames_to_process) {
  float max = 0;

  Impl::Vmaxmgv(source_p, source_stride, &max, frames_to_process);

  DCHECK(max_p);
  *max_p = max;
}

void Vmul(const float* source1p,
          int source_stride1,
          const float* source2p,
          int source_stride2,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  Impl::Vmul(source1p, source_stride1, source2p, source_stride2, dest_p,
             dest_stride, frames_to_process);
}

void Vsma(const float* source_p,
          int source_stride,
          const float* scale,
          float* dest_p,
          int dest_stride,
          size_t frames_to_process) {
  const float k = *scale;

  Impl::Vsma(source_p, source_stride, &k, dest_p, dest_stride,
             frames_to_process);
}

void Vsmul(const float* source_p,
           int source_stride,
           const float* scale,
           float* dest_p,
           int dest_stride,
           size_t frames_to_process) {
  const float k = *scale;

  Impl::Vsmul(source_p, source_stride, &k, dest_p, dest_stride,
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

}  // namespace VectorMath

}  // namespace blink

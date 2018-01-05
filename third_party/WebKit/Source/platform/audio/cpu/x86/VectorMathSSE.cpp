// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_MACOSX)

#include "platform/audio/cpu/x86/VectorMathSSE.h"

#include "platform/audio/VectorMathScalar.h"
#include "platform/wtf/Assertions.h"

#include <algorithm>
#include <cmath>

#include <xmmintrin.h>

namespace blink {
namespace VectorMath {
namespace SSE {

#define MM_PS(name) _mm_##name##_ps

bool IsAligned(const float* p) {
  constexpr size_t kBytesPerRegister = kBitsPerRegister / 8u;
  constexpr size_t kAlignmentOffsetMask = kBytesPerRegister - 1u;
  return (reinterpret_cast<size_t>(p) & kAlignmentOffsetMask) == 0u;
}

void Vadd(const float* source1p,
          const float* source2p,
          float* dest_p,
          size_t frames_to_process) {
  const float* const source1_end_p = source1p + frames_to_process;

  DCHECK(IsAligned(source1p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

#define ADD_ALL(loadSource2, storeDest)              \
  while (source1p < source1_end_p) {                 \
    MType m_source1 = MM_PS(load)(source1p);         \
    MType m_source2 = MM_PS(loadSource2)(source2p);  \
    MType m_dest = MM_PS(add)(m_source1, m_source2); \
    MM_PS(storeDest)(dest_p, m_dest);                \
    source1p += kPackedFloatsPerRegister;            \
    source2p += kPackedFloatsPerRegister;            \
    dest_p += kPackedFloatsPerRegister;              \
  }

  if (IsAligned(source2p)) {
    if (IsAligned(dest_p)) {
      ADD_ALL(load, store);
    } else {
      ADD_ALL(load, storeu);
    }
  } else {
    if (IsAligned(dest_p)) {
      ADD_ALL(loadu, store);
    } else {
      ADD_ALL(loadu, storeu);
    }
  }

#undef ADD_ALL
}

void Vclip(const float* source_p,
           const float* low_threshold_p,
           const float* high_threshold_p,
           float* dest_p,
           size_t frames_to_process) {
  const float* const source_end_p = source_p + frames_to_process;

  DCHECK(IsAligned(source_p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

  MType m_low_threshold = MM_PS(set1)(*low_threshold_p);
  MType m_high_threshold = MM_PS(set1)(*high_threshold_p);

#define CLIP_ALL(storeDest)                                                  \
  while (source_p < source_end_p) {                                          \
    MType m_source = MM_PS(load)(source_p);                                  \
    MType m_dest =                                                           \
        MM_PS(max)(m_low_threshold, MM_PS(min)(m_high_threshold, m_source)); \
    MM_PS(storeDest)(dest_p, m_dest);                                        \
    source_p += kPackedFloatsPerRegister;                                    \
    dest_p += kPackedFloatsPerRegister;                                      \
  }

  if (IsAligned(dest_p)) {
    CLIP_ALL(store);
  } else {
    CLIP_ALL(storeu);
  }

#undef CLIP_ALL
}

void Vmaxmgv(const float* source_p, float* max_p, size_t frames_to_process) {
  constexpr uint32_t kMask = 0x7FFFFFFFu;

  const float* const source_end_p = source_p + frames_to_process;

  DCHECK(IsAligned(source_p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

  MType m_mask = MM_PS(set1)(*reinterpret_cast<const float*>(&kMask));
  MType m_max = MM_PS(setzero)();

  while (source_p < source_end_p) {
    MType m_source = MM_PS(load)(source_p);
    // Calculate the absolute value by ANDing the source with the mask,
    // which will set the sign bit to 0.
    m_source = MM_PS(and)(m_source, m_mask);
    m_max = MM_PS(max)(m_source, m_max);
    source_p += kPackedFloatsPerRegister;
  }

  // Combine the packed floats.
  const float* maxes = reinterpret_cast<const float*>(&m_max);
  for (unsigned i = 0u; i < kPackedFloatsPerRegister; ++i)
    *max_p = std::max(*max_p, maxes[i]);
}

void Vmul(const float* source1p,
          const float* source2p,
          float* dest_p,
          size_t frames_to_process) {
  const float* const source1_end_p = source1p + frames_to_process;

  DCHECK(IsAligned(source1p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

#define MULTIPLY_ALL(loadSource2, storeDest)         \
  while (source1p < source1_end_p) {                 \
    MType m_source1 = MM_PS(load)(source1p);         \
    MType m_source2 = MM_PS(loadSource2)(source2p);  \
    MType m_dest = MM_PS(mul)(m_source1, m_source2); \
    MM_PS(storeDest)(dest_p, m_dest);                \
    source1p += kPackedFloatsPerRegister;            \
    source2p += kPackedFloatsPerRegister;            \
    dest_p += kPackedFloatsPerRegister;              \
  }

  if (IsAligned(source2p)) {
    if (IsAligned(dest_p)) {
      MULTIPLY_ALL(load, store);
    } else {
      MULTIPLY_ALL(load, storeu);
    }
  } else {
    if (IsAligned(dest_p)) {
      MULTIPLY_ALL(loadu, store);
    } else {
      MULTIPLY_ALL(loadu, storeu);
    }
  }

#undef MULTIPLY_ALL
}

void Vsma(const float* source_p,
          const float* scale,
          float* dest_p,
          size_t frames_to_process) {
  const float* const source_end_p = source_p + frames_to_process;

  DCHECK(IsAligned(source_p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

  const MType m_scale = MM_PS(set1)(*scale);

#define SCALAR_MULTIPLY_AND_ADD_ALL(loadDest, storeDest)        \
  while (source_p < source_end_p) {                             \
    MType m_source = MM_PS(load)(source_p);                     \
    MType m_dest = MM_PS(loadDest)(dest_p);                     \
    m_dest = MM_PS(add)(m_dest, MM_PS(mul)(m_scale, m_source)); \
    MM_PS(storeDest)(dest_p, m_dest);                           \
    source_p += kPackedFloatsPerRegister;                       \
    dest_p += kPackedFloatsPerRegister;                         \
  }

  if (IsAligned(dest_p)) {
    SCALAR_MULTIPLY_AND_ADD_ALL(load, store);
  } else {
    SCALAR_MULTIPLY_AND_ADD_ALL(loadu, storeu);
  }

#undef SCALAR_MULTIPLY_AND_ADD_ALL
}

void Vsmul(const float* source_p,
           const float* scale,
           float* dest_p,
           size_t frames_to_process) {
  const float* const source_end_p = source_p + frames_to_process;

  DCHECK(IsAligned(source_p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

  const MType m_scale = MM_PS(set1)(*scale);

#define SCALAR_MULTIPLY_ALL(storeDest)            \
  while (source_p < source_end_p) {               \
    MType m_source = MM_PS(load)(source_p);       \
    MType m_dest = MM_PS(mul)(m_scale, m_source); \
    MM_PS(storeDest)(dest_p, m_dest);             \
    source_p += kPackedFloatsPerRegister;         \
    dest_p += kPackedFloatsPerRegister;           \
  }

  if (IsAligned(dest_p)) {
    SCALAR_MULTIPLY_ALL(store);
  } else {
    SCALAR_MULTIPLY_ALL(storeu);
  }

#undef SCALAR_MULTIPLY_ALL
}

void Vsvesq(const float* source_p, float* sum_p, size_t frames_to_process) {
  const float* const source_end_p = source_p + frames_to_process;

  DCHECK(IsAligned(source_p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

  MType m_sum = MM_PS(setzero)();

  while (source_p < source_end_p) {
    MType m_source = MM_PS(load)(source_p);
    m_sum = MM_PS(add)(m_sum, MM_PS(mul)(m_source, m_source));
    source_p += kPackedFloatsPerRegister;
  }

  // Combine the packed floats.
  const float* sums = reinterpret_cast<const float*>(&m_sum);
  for (unsigned i = 0u; i < kPackedFloatsPerRegister; ++i)
    *sum_p += sums[i];
}

void Zvmul(const float* real1p,
           const float* imag1p,
           const float* real2p,
           const float* imag2p,
           float* real_dest_p,
           float* imag_dest_p,
           size_t frames_to_process) {
  DCHECK(IsAligned(real1p));
  DCHECK_EQ(0u, frames_to_process % kPackedFloatsPerRegister);

#define MULTIPLY_ALL(loadOtherThanReal1, storeDest)                           \
  for (size_t i = 0u; i < frames_to_process; i += kPackedFloatsPerRegister) { \
    MType real1 = MM_PS(load)(real1p + i);                                    \
    MType real2 = MM_PS(loadOtherThanReal1)(real2p + i);                      \
    MType imag1 = MM_PS(loadOtherThanReal1)(imag1p + i);                      \
    MType imag2 = MM_PS(loadOtherThanReal1)(imag2p + i);                      \
    MType real =                                                              \
        MM_PS(sub)(MM_PS(mul)(real1, real2), MM_PS(mul)(imag1, imag2));       \
    MType imag =                                                              \
        MM_PS(add)(MM_PS(mul)(real1, imag2), MM_PS(mul)(imag1, real2));       \
    MM_PS(storeDest)(real_dest_p + i, real);                                  \
    MM_PS(storeDest)(imag_dest_p + i, imag);                                  \
  }

  if (IsAligned(imag1p) && IsAligned(real2p) && IsAligned(imag2p) &&
      IsAligned(real_dest_p) && IsAligned(imag_dest_p)) {
    MULTIPLY_ALL(load, store);
  } else {
    MULTIPLY_ALL(loadu, storeu);
  }

#undef MULTIPLY_ALL
}

#undef MM_PS

}  // namespace SSE
}  // namespace VectorMath
}  // namespace blink

#endif  // defined(ARCH_CPU_X86_FAMILY) && !defined(OS_MACOSX)

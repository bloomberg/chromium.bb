// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/dsp/convolve.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1
#include <smmintrin.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

void ConvolveCompoundCopy_SSE4(
    const void* const reference, const ptrdiff_t reference_stride,
    const int /*horizontal_filter_index*/, const int /*vertical_filter_index*/,
    const int /*subpixel_x*/, const int /*subpixel_y*/, const int width,
    const int height, void* prediction, const ptrdiff_t pred_stride) {
  const auto* src = static_cast<const uint8_t*>(reference);
  const ptrdiff_t src_stride = reference_stride;
  auto* dest = static_cast<uint16_t*>(prediction);
  constexpr int kRoundBitsVertical =
      kInterRoundBitsVertical - kInterRoundBitsCompoundVertical;
  if (width >= 16) {
    int y = height;
    do {
      int x = 0;
      do {
        const __m128i v_src = LoadUnaligned16(&src[x]);
        const __m128i v_src_ext_lo = _mm_cvtepu8_epi16(v_src);
        const __m128i v_src_ext_hi =
            _mm_cvtepu8_epi16(_mm_srli_si128(v_src, 8));
        const __m128i v_dest_lo =
            _mm_slli_epi16(v_src_ext_lo, kRoundBitsVertical);
        const __m128i v_dest_hi =
            _mm_slli_epi16(v_src_ext_hi, kRoundBitsVertical);
        // TODO(slavarnway): Investigate using aligned stores.
        StoreUnaligned16(&dest[x], v_dest_lo);
        StoreUnaligned16(&dest[x + 8], v_dest_hi);
        x += 16;
      } while (x < width);
      src += src_stride;
      dest += pred_stride;
    } while (--y != 0);
  } else if (width == 8) {
    int y = height;
    do {
      const __m128i v_src = LoadLo8(&src[0]);
      const __m128i v_src_ext = _mm_cvtepu8_epi16(v_src);
      const __m128i v_dest = _mm_slli_epi16(v_src_ext, kRoundBitsVertical);
      StoreUnaligned16(&dest[0], v_dest);
      src += src_stride;
      dest += pred_stride;
    } while (--y != 0);
  } else { /* width == 4 */
    int y = height;
    do {
      const __m128i v_src0 = Load4(&src[0]);
      const __m128i v_src1 = Load4(&src[src_stride]);
      const __m128i v_src = _mm_unpacklo_epi32(v_src0, v_src1);
      const __m128i v_src_ext = _mm_cvtepu8_epi16(v_src);
      const __m128i v_dest = _mm_slli_epi16(v_src_ext, kRoundBitsVertical);
      StoreLo8(&dest[0], v_dest);
      StoreHi8(&dest[pred_stride], v_dest);
      src += src_stride * 2;
      dest += pred_stride * 2;
      y -= 2;
    } while (y != 0);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);

  dsp->convolve[0][1][0][0] = ConvolveCompoundCopy_SSE4;
}

}  // namespace
}  // namespace low_bitdepth

void ConvolveInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1

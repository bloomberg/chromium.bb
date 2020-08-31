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

#include "src/dsp/intrapred.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON

#include <arm_neon.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

uint8x16_t Set2ValuesQ(const uint8_t* a) {
  uint16_t combined_values = a[0] | a[1] << 8;
  return vreinterpretq_u8_u16(vdupq_n_u16(combined_values));
}

uint32_t SumVector(uint32x2_t a) {
#if defined(__aarch64__)
  return vaddv_u32(a);
#else
  const uint64x1_t b = vpaddl_u32(a);
  return vget_lane_u32(vreinterpret_u32_u64(b), 0);
#endif  // defined(__aarch64__)
}

uint32_t SumVector(uint32x4_t a) {
#if defined(__aarch64__)
  return vaddvq_u32(a);
#else
  const uint64x2_t b = vpaddlq_u32(a);
  const uint64x1_t c = vadd_u64(vget_low_u64(b), vget_high_u64(b));
  return vget_lane_u32(vreinterpret_u32_u64(c), 0);
#endif  // defined(__aarch64__)
}

// Divide by the number of elements.
uint32_t Average(const uint32_t sum, const int width, const int height) {
  return RightShiftWithRounding(sum, FloorLog2(width) + FloorLog2(height));
}

// Subtract |val| from every element in |a|.
void BlockSubtract(const uint32_t val,
                   int16_t a[kCflLumaBufferStride][kCflLumaBufferStride],
                   const int width, const int height) {
  assert(val <= INT16_MAX);
  const int16x8_t val_v = vdupq_n_s16(static_cast<int16_t>(val));

  for (int y = 0; y < height; ++y) {
    if (width == 4) {
      const int16x4_t b = vld1_s16(a[y]);
      vst1_s16(a[y], vsub_s16(b, vget_low_s16(val_v)));
    } else if (width == 8) {
      const int16x8_t b = vld1q_s16(a[y]);
      vst1q_s16(a[y], vsubq_s16(b, val_v));
    } else if (width == 16) {
      const int16x8_t b = vld1q_s16(a[y]);
      const int16x8_t c = vld1q_s16(a[y] + 8);
      vst1q_s16(a[y], vsubq_s16(b, val_v));
      vst1q_s16(a[y] + 8, vsubq_s16(c, val_v));
    } else /* block_width == 32 */ {
      const int16x8_t b = vld1q_s16(a[y]);
      const int16x8_t c = vld1q_s16(a[y] + 8);
      const int16x8_t d = vld1q_s16(a[y] + 16);
      const int16x8_t e = vld1q_s16(a[y] + 24);
      vst1q_s16(a[y], vsubq_s16(b, val_v));
      vst1q_s16(a[y] + 8, vsubq_s16(c, val_v));
      vst1q_s16(a[y] + 16, vsubq_s16(d, val_v));
      vst1q_s16(a[y] + 24, vsubq_s16(e, val_v));
    }
  }
}

template <int block_width, int block_height>
void CflSubsampler420_NEON(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, const ptrdiff_t stride) {
  const auto* src = static_cast<const uint8_t*>(source);
  uint32_t sum;
  if (block_width == 4) {
    assert(max_luma_width >= 8);
    uint32x2_t running_sum = vdup_n_u32(0);

    for (int y = 0; y < block_height; ++y) {
      const uint8x8_t row0 = vld1_u8(src);
      const uint8x8_t row1 = vld1_u8(src + stride);

      uint16x4_t sum_row = vpadal_u8(vpaddl_u8(row0), row1);
      sum_row = vshl_n_u16(sum_row, 1);
      running_sum = vpadal_u16(running_sum, sum_row);
      vst1_s16(luma[y], vreinterpret_s16_u16(sum_row));

      if (y << 1 < max_luma_height - 2) {
        // Once this threshold is reached the loop could be simplified.
        src += stride << 1;
      }
    }

    sum = SumVector(running_sum);
  } else if (block_width == 8) {
    const uint8x16_t x_index = {0, 0, 2,  2,  4,  4,  6,  6,
                                8, 8, 10, 10, 12, 12, 14, 14};
    const uint8x16_t x_max_index = vdupq_n_u8(max_luma_width - 2);
    const uint8x16_t x_mask = vcltq_u8(x_index, x_max_index);

    uint32x4_t running_sum = vdupq_n_u32(0);

    for (int y = 0; y < block_height; ++y) {
      const uint8x16_t x_max0 = Set2ValuesQ(src + max_luma_width - 2);
      const uint8x16_t x_max1 = Set2ValuesQ(src + max_luma_width - 2 + stride);

      uint8x16_t row0 = vld1q_u8(src);
      row0 = vbslq_u8(x_mask, row0, x_max0);
      uint8x16_t row1 = vld1q_u8(src + stride);
      row1 = vbslq_u8(x_mask, row1, x_max1);

      uint16x8_t sum_row = vpadalq_u8(vpaddlq_u8(row0), row1);
      sum_row = vshlq_n_u16(sum_row, 1);
      running_sum = vpadalq_u16(running_sum, sum_row);
      vst1q_s16(luma[y], vreinterpretq_s16_u16(sum_row));

      if (y << 1 < max_luma_height - 2) {
        src += stride << 1;
      }
    }

    sum = SumVector(running_sum);
  } else /* block_width >= 16 */ {
    const uint8x16_t x_max_index = vdupq_n_u8(max_luma_width - 2);
    uint32x4_t running_sum = vdupq_n_u32(0);

    for (int y = 0; y < block_height; ++y) {
      uint8x16_t x_index = {0,  2,  4,  6,  8,  10, 12, 14,
                            16, 18, 20, 22, 24, 26, 28, 30};
      const uint8x16_t x_max00 = vdupq_n_u8(src[max_luma_width - 2]);
      const uint8x16_t x_max01 = vdupq_n_u8(src[max_luma_width - 2 + 1]);
      const uint8x16_t x_max10 = vdupq_n_u8(src[stride + max_luma_width - 2]);
      const uint8x16_t x_max11 =
          vdupq_n_u8(src[stride + max_luma_width - 2 + 1]);
      for (int x = 0; x < block_width; x += 16) {
        const ptrdiff_t src_x_offset = x << 1;
        const uint8x16_t x_mask = vcltq_u8(x_index, x_max_index);
        const uint8x16x2_t row0 = vld2q_u8(src + src_x_offset);
        const uint8x16x2_t row1 = vld2q_u8(src + src_x_offset + stride);
        const uint8x16_t row_masked_00 = vbslq_u8(x_mask, row0.val[0], x_max00);
        const uint8x16_t row_masked_01 = vbslq_u8(x_mask, row0.val[1], x_max01);
        const uint8x16_t row_masked_10 = vbslq_u8(x_mask, row1.val[0], x_max10);
        const uint8x16_t row_masked_11 = vbslq_u8(x_mask, row1.val[1], x_max11);

        uint16x8_t sum_row_lo =
            vaddl_u8(vget_low_u8(row_masked_00), vget_low_u8(row_masked_01));
        sum_row_lo = vaddw_u8(sum_row_lo, vget_low_u8(row_masked_10));
        sum_row_lo = vaddw_u8(sum_row_lo, vget_low_u8(row_masked_11));
        sum_row_lo = vshlq_n_u16(sum_row_lo, 1);
        running_sum = vpadalq_u16(running_sum, sum_row_lo);
        vst1q_s16(luma[y] + x, vreinterpretq_s16_u16(sum_row_lo));

        uint16x8_t sum_row_hi =
            vaddl_u8(vget_high_u8(row_masked_00), vget_high_u8(row_masked_01));
        sum_row_hi = vaddw_u8(sum_row_hi, vget_high_u8(row_masked_10));
        sum_row_hi = vaddw_u8(sum_row_hi, vget_high_u8(row_masked_11));
        sum_row_hi = vshlq_n_u16(sum_row_hi, 1);
        running_sum = vpadalq_u16(running_sum, sum_row_hi);
        vst1q_s16(luma[y] + x + 8, vreinterpretq_s16_u16(sum_row_hi));

        x_index = vaddq_u8(x_index, vdupq_n_u8(32));
      }
      if (y << 1 < max_luma_height - 2) {
        src += stride << 1;
      }
    }
    sum = SumVector(running_sum);
  }

  const uint32_t average = Average(sum, block_width, block_height);
  BlockSubtract(average, luma, block_width, block_height);
}

template <int block_width, int block_height>
void CflSubsampler444_NEON(
    int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int max_luma_width, const int max_luma_height,
    const void* const source, const ptrdiff_t stride) {
  const auto* src = static_cast<const uint8_t*>(source);
  uint32_t sum;
  if (block_width == 4) {
    assert(max_luma_width >= 4);
    uint32x4_t running_sum = vdupq_n_u32(0);
    uint8x8_t row = vdup_n_u8(0);

    for (int y = 0; y < block_height; y += 2) {
      row = Load4<0>(src, row);
      row = Load4<1>(src + stride, row);
      if (y < (max_luma_height - 1)) {
        src += stride << 1;
      }

      const uint16x8_t row_shifted = vshll_n_u8(row, 3);
      running_sum = vpadalq_u16(running_sum, row_shifted);
      vst1_s16(luma[y], vreinterpret_s16_u16(vget_low_u16(row_shifted)));
      vst1_s16(luma[y + 1], vreinterpret_s16_u16(vget_high_u16(row_shifted)));
    }

    sum = SumVector(running_sum);
  } else if (block_width == 8) {
    const uint8x8_t x_index = {0, 1, 2, 3, 4, 5, 6, 7};
    const uint8x8_t x_max_index = vdup_n_u8(max_luma_width - 1);
    const uint8x8_t x_mask = vclt_u8(x_index, x_max_index);

    uint32x4_t running_sum = vdupq_n_u32(0);

    for (int y = 0; y < block_height; ++y) {
      const uint8x8_t x_max = vdup_n_u8(src[max_luma_width - 1]);
      const uint8x8_t row = vbsl_u8(x_mask, vld1_u8(src), x_max);

      const uint16x8_t row_shifted = vshll_n_u8(row, 3);
      running_sum = vpadalq_u16(running_sum, row_shifted);
      vst1q_s16(luma[y], vreinterpretq_s16_u16(row_shifted));

      if (y < max_luma_height - 1) {
        src += stride;
      }
    }

    sum = SumVector(running_sum);
  } else /* block_width >= 16 */ {
    const uint8x16_t x_max_index = vdupq_n_u8(max_luma_width - 1);
    uint32x4_t running_sum = vdupq_n_u32(0);

    for (int y = 0; y < block_height; ++y) {
      uint8x16_t x_index = {0, 1, 2,  3,  4,  5,  6,  7,
                            8, 9, 10, 11, 12, 13, 14, 15};
      const uint8x16_t x_max = vdupq_n_u8(src[max_luma_width - 1]);
      for (int x = 0; x < block_width; x += 16) {
        const uint8x16_t x_mask = vcltq_u8(x_index, x_max_index);
        const uint8x16_t row = vbslq_u8(x_mask, vld1q_u8(src + x), x_max);

        const uint16x8_t row_shifted_low = vshll_n_u8(vget_low_u8(row), 3);
        const uint16x8_t row_shifted_high = vshll_n_u8(vget_high_u8(row), 3);
        running_sum = vpadalq_u16(running_sum, row_shifted_low);
        running_sum = vpadalq_u16(running_sum, row_shifted_high);
        vst1q_s16(luma[y] + x, vreinterpretq_s16_u16(row_shifted_low));
        vst1q_s16(luma[y] + x + 8, vreinterpretq_s16_u16(row_shifted_high));

        x_index = vaddq_u8(x_index, vdupq_n_u8(16));
      }
      if (y < max_luma_height - 1) {
        src += stride;
      }
    }
    sum = SumVector(running_sum);
  }

  const uint32_t average = Average(sum, block_width, block_height);
  BlockSubtract(average, luma, block_width, block_height);
}

// Saturate |dc + ((alpha * luma) >> 6))| to uint8_t.
inline uint8x8_t Combine8(const int16x8_t luma, const int alpha,
                          const int16x8_t dc) {
  const int16x8_t la = vmulq_n_s16(luma, alpha);
  // Subtract the sign bit to round towards zero.
  const int16x8_t sub_sign = vsraq_n_s16(la, la, 15);
  // Shift and accumulate.
  const int16x8_t result = vrsraq_n_s16(dc, sub_sign, 6);
  return vqmovun_s16(result);
}

// The range of luma/alpha is not really important because it gets saturated to
// uint8_t. Saturated int16_t >> 6 outranges uint8_t.
template <int block_height>
inline void CflIntraPredictor4xN_NEON(
    void* const dest, const ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int alpha) {
  auto* dst = static_cast<uint8_t*>(dest);
  const int16x8_t dc = vdupq_n_s16(dst[0]);
  for (int y = 0; y < block_height; y += 2) {
    const int16x4_t luma_row0 = vld1_s16(luma[y]);
    const int16x4_t luma_row1 = vld1_s16(luma[y + 1]);
    const uint8x8_t sum =
        Combine8(vcombine_s16(luma_row0, luma_row1), alpha, dc);
    StoreLo4(dst, sum);
    dst += stride;
    StoreHi4(dst, sum);
    dst += stride;
  }
}

template <int block_height>
inline void CflIntraPredictor8xN_NEON(
    void* const dest, const ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int alpha) {
  auto* dst = static_cast<uint8_t*>(dest);
  const int16x8_t dc = vdupq_n_s16(dst[0]);
  for (int y = 0; y < block_height; ++y) {
    const int16x8_t luma_row = vld1q_s16(luma[y]);
    const uint8x8_t sum = Combine8(luma_row, alpha, dc);
    vst1_u8(dst, sum);
    dst += stride;
  }
}

template <int block_height>
inline void CflIntraPredictor16xN_NEON(
    void* const dest, const ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int alpha) {
  auto* dst = static_cast<uint8_t*>(dest);
  const int16x8_t dc = vdupq_n_s16(dst[0]);
  for (int y = 0; y < block_height; ++y) {
    const int16x8_t luma_row_0 = vld1q_s16(luma[y]);
    const int16x8_t luma_row_1 = vld1q_s16(luma[y] + 8);
    const uint8x8_t sum_0 = Combine8(luma_row_0, alpha, dc);
    const uint8x8_t sum_1 = Combine8(luma_row_1, alpha, dc);
    vst1_u8(dst, sum_0);
    vst1_u8(dst + 8, sum_1);
    dst += stride;
  }
}

template <int block_height>
inline void CflIntraPredictor32xN_NEON(
    void* const dest, const ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
    const int alpha) {
  auto* dst = static_cast<uint8_t*>(dest);
  const int16x8_t dc = vdupq_n_s16(dst[0]);
  for (int y = 0; y < block_height; ++y) {
    const int16x8_t luma_row_0 = vld1q_s16(luma[y]);
    const int16x8_t luma_row_1 = vld1q_s16(luma[y] + 8);
    const int16x8_t luma_row_2 = vld1q_s16(luma[y] + 16);
    const int16x8_t luma_row_3 = vld1q_s16(luma[y] + 24);
    const uint8x8_t sum_0 = Combine8(luma_row_0, alpha, dc);
    const uint8x8_t sum_1 = Combine8(luma_row_1, alpha, dc);
    const uint8x8_t sum_2 = Combine8(luma_row_2, alpha, dc);
    const uint8x8_t sum_3 = Combine8(luma_row_3, alpha, dc);
    vst1_u8(dst, sum_0);
    vst1_u8(dst + 8, sum_1);
    vst1_u8(dst + 16, sum_2);
    vst1_u8(dst + 24, sum_3);
    dst += stride;
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);

  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType420] =
      CflSubsampler420_NEON<4, 4>;
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType420] =
      CflSubsampler420_NEON<4, 8>;
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType420] =
      CflSubsampler420_NEON<4, 16>;

  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType420] =
      CflSubsampler420_NEON<8, 4>;
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType420] =
      CflSubsampler420_NEON<8, 8>;
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType420] =
      CflSubsampler420_NEON<8, 16>;
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType420] =
      CflSubsampler420_NEON<8, 32>;

  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType420] =
      CflSubsampler420_NEON<16, 4>;
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType420] =
      CflSubsampler420_NEON<16, 8>;
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType420] =
      CflSubsampler420_NEON<16, 16>;
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType420] =
      CflSubsampler420_NEON<16, 32>;

  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType420] =
      CflSubsampler420_NEON<32, 8>;
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType420] =
      CflSubsampler420_NEON<32, 16>;
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType420] =
      CflSubsampler420_NEON<32, 32>;

  dsp->cfl_subsamplers[kTransformSize4x4][kSubsamplingType444] =
      CflSubsampler444_NEON<4, 4>;
  dsp->cfl_subsamplers[kTransformSize4x8][kSubsamplingType444] =
      CflSubsampler444_NEON<4, 8>;
  dsp->cfl_subsamplers[kTransformSize4x16][kSubsamplingType444] =
      CflSubsampler444_NEON<4, 16>;

  dsp->cfl_subsamplers[kTransformSize8x4][kSubsamplingType444] =
      CflSubsampler444_NEON<8, 4>;
  dsp->cfl_subsamplers[kTransformSize8x8][kSubsamplingType444] =
      CflSubsampler444_NEON<8, 8>;
  dsp->cfl_subsamplers[kTransformSize8x16][kSubsamplingType444] =
      CflSubsampler444_NEON<8, 16>;
  dsp->cfl_subsamplers[kTransformSize8x32][kSubsamplingType444] =
      CflSubsampler444_NEON<8, 32>;

  dsp->cfl_subsamplers[kTransformSize16x4][kSubsamplingType444] =
      CflSubsampler444_NEON<16, 4>;
  dsp->cfl_subsamplers[kTransformSize16x8][kSubsamplingType444] =
      CflSubsampler444_NEON<16, 8>;
  dsp->cfl_subsamplers[kTransformSize16x16][kSubsamplingType444] =
      CflSubsampler444_NEON<16, 16>;
  dsp->cfl_subsamplers[kTransformSize16x32][kSubsamplingType444] =
      CflSubsampler444_NEON<16, 32>;

  dsp->cfl_subsamplers[kTransformSize32x8][kSubsamplingType444] =
      CflSubsampler444_NEON<32, 8>;
  dsp->cfl_subsamplers[kTransformSize32x16][kSubsamplingType444] =
      CflSubsampler444_NEON<32, 16>;
  dsp->cfl_subsamplers[kTransformSize32x32][kSubsamplingType444] =
      CflSubsampler444_NEON<32, 32>;

  dsp->cfl_intra_predictors[kTransformSize4x4] = CflIntraPredictor4xN_NEON<4>;
  dsp->cfl_intra_predictors[kTransformSize4x8] = CflIntraPredictor4xN_NEON<8>;
  dsp->cfl_intra_predictors[kTransformSize4x16] = CflIntraPredictor4xN_NEON<16>;

  dsp->cfl_intra_predictors[kTransformSize8x4] = CflIntraPredictor8xN_NEON<4>;
  dsp->cfl_intra_predictors[kTransformSize8x8] = CflIntraPredictor8xN_NEON<8>;
  dsp->cfl_intra_predictors[kTransformSize8x16] = CflIntraPredictor8xN_NEON<16>;
  dsp->cfl_intra_predictors[kTransformSize8x32] = CflIntraPredictor8xN_NEON<32>;

  dsp->cfl_intra_predictors[kTransformSize16x4] = CflIntraPredictor16xN_NEON<4>;
  dsp->cfl_intra_predictors[kTransformSize16x8] = CflIntraPredictor16xN_NEON<8>;
  dsp->cfl_intra_predictors[kTransformSize16x16] =
      CflIntraPredictor16xN_NEON<16>;
  dsp->cfl_intra_predictors[kTransformSize16x32] =
      CflIntraPredictor16xN_NEON<32>;

  dsp->cfl_intra_predictors[kTransformSize32x8] = CflIntraPredictor32xN_NEON<8>;
  dsp->cfl_intra_predictors[kTransformSize32x16] =
      CflIntraPredictor32xN_NEON<16>;
  dsp->cfl_intra_predictors[kTransformSize32x32] =
      CflIntraPredictor32xN_NEON<32>;
  // Max Cfl predictor size is 32x32.
}

}  // namespace
}  // namespace low_bitdepth

void IntraPredCflInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void IntraPredCflInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON

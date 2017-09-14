/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/

#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/blockd.h"
#include "av1/common/pred_common.h"
#include "aom_mem/aom_mem.h"

namespace {

using libaom_test::ACMRandom;

const int count_test_block = 100000;

typedef void (*IntraPred)(uint16_t *dst, ptrdiff_t stride,
                          const uint16_t *above, const uint16_t *left, int bps);

struct IntraPredFunc {
  IntraPredFunc(IntraPred pred = NULL, IntraPred ref = NULL,
                int block_width_value = 0, int block_height_value = 0,
                int bit_depth_value = 0)
      : pred_fn(pred), ref_fn(ref), block_width(block_width_value),
        block_height(block_height_value), bit_depth(bit_depth_value) {}

  IntraPred pred_fn;
  IntraPred ref_fn;
  int block_width;
  int block_height;
  int bit_depth;
};

class AV1IntraPredTest : public ::testing::TestWithParam<IntraPredFunc> {
 public:
  void RunTest(uint16_t *left_col, uint16_t *above_data, uint16_t *dst,
               uint16_t *ref_dst) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int block_width = params_.block_width;
    const int block_height = params_.block_height;
    above_row_ = above_data + 16;
    left_col_ = left_col;
    dst_ = dst;
    ref_dst_ = ref_dst;
    int error_count = 0;
    for (int i = 0; i < count_test_block; ++i) {
      // Fill edges with random data, try first with saturated values.
      for (int x = -1; x <= block_width * 2; x++) {
        if (i == 0) {
          above_row_[x] = mask_;
        } else {
          above_row_[x] = rnd.Rand16() & mask_;
        }
      }
      for (int y = 0; y < block_height; y++) {
        if (i == 0) {
          left_col_[y] = mask_;
        } else {
          left_col_[y] = rnd.Rand16() & mask_;
        }
      }
      Predict();
      CheckPrediction(i, &error_count);
    }
    ASSERT_EQ(0, error_count);
  }

 protected:
  virtual void SetUp() {
    params_ = GetParam();
    stride_ = params_.block_width * 3;
    mask_ = (1 << params_.bit_depth) - 1;
  }

  void Predict() {
    const int bit_depth = params_.bit_depth;
    params_.ref_fn(ref_dst_, stride_, above_row_, left_col_, bit_depth);
    ASM_REGISTER_STATE_CHECK(
        params_.pred_fn(dst_, stride_, above_row_, left_col_, bit_depth));
  }

  void CheckPrediction(int test_case_number, int *error_count) const {
    // For each pixel ensure that the calculated value is the same as reference.
    const int block_width = params_.block_width;
    const int block_height = params_.block_height;
    for (int y = 0; y < block_height; y++) {
      for (int x = 0; x < block_width; x++) {
        *error_count += ref_dst_[x + y * stride_] != dst_[x + y * stride_];
        if (*error_count == 1) {
          ASSERT_EQ(ref_dst_[x + y * stride_], dst_[x + y * stride_])
              << " Failed on Test Case Number " << test_case_number;
        }
      }
    }
  }

  uint16_t *above_row_;
  uint16_t *left_col_;
  uint16_t *dst_;
  uint16_t *ref_dst_;
  ptrdiff_t stride_;
  int mask_;

  IntraPredFunc params_;
};

TEST_P(AV1IntraPredTest, IntraPredTests) {
  // max block size is 32
  DECLARE_ALIGNED(16, uint16_t, left_col[2 * 32]);
  DECLARE_ALIGNED(16, uint16_t, above_data[2 * 32 + 32]);
  DECLARE_ALIGNED(16, uint16_t, dst[3 * 32 * 32]);
  DECLARE_ALIGNED(16, uint16_t, ref_dst[3 * 32 * 32]);
  RunTest(left_col, above_data, dst, ref_dst);
}

#define highbd_entry(type, width, height, opt, bd)                            \
  IntraPredFunc(&aom_highbd_##type##_predictor_##width##x##height##_##opt,    \
                &aom_highbd_##type##_predictor_##width##x##height##_c, width, \
                height, bd)

#if HAVE_SSE2
#if CONFIG_HIGHBITDEPTH
const IntraPredFunc IntraPredTestVector8[] = {
  highbd_entry(dc, 4, 4, sse2, 8),   highbd_entry(dc, 8, 8, sse2, 8),
  highbd_entry(dc, 16, 16, sse2, 8), highbd_entry(dc, 32, 32, sse2, 8),
  highbd_entry(v, 4, 4, sse2, 8),    highbd_entry(v, 8, 8, sse2, 8),
  highbd_entry(v, 16, 16, sse2, 8),  highbd_entry(v, 32, 32, sse2, 8),
};

INSTANTIATE_TEST_CASE_P(SSE2_TO_C_8, AV1IntraPredTest,
                        ::testing::ValuesIn(IntraPredTestVector8));

const IntraPredFunc IntraPredTestVector10[] = {
  highbd_entry(dc, 4, 4, sse2, 10),   highbd_entry(dc, 8, 8, sse2, 10),
  highbd_entry(dc, 16, 16, sse2, 10), highbd_entry(dc, 32, 32, sse2, 10),
  highbd_entry(v, 4, 4, sse2, 10),    highbd_entry(v, 8, 8, sse2, 10),
  highbd_entry(v, 16, 16, sse2, 10),  highbd_entry(v, 32, 32, sse2, 10),
};

INSTANTIATE_TEST_CASE_P(SSE2_TO_C_10, AV1IntraPredTest,
                        ::testing::ValuesIn(IntraPredTestVector10));

const IntraPredFunc IntraPredTestVector12[] = {
  highbd_entry(dc, 4, 4, sse2, 12),   highbd_entry(dc, 8, 8, sse2, 12),
  highbd_entry(dc, 16, 16, sse2, 12), highbd_entry(dc, 32, 32, sse2, 12),
  highbd_entry(v, 4, 4, sse2, 12),    highbd_entry(v, 8, 8, sse2, 12),
  highbd_entry(v, 16, 16, sse2, 12),  highbd_entry(v, 32, 32, sse2, 12),
};

INSTANTIATE_TEST_CASE_P(SSE2_TO_C_12, AV1IntraPredTest,
                        ::testing::ValuesIn(IntraPredTestVector12));

#endif  // CONFIG_HIGHBITDEPTH
#endif  // HAVE_SSE2
}  // namespace

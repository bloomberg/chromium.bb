/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "av1/common/blockd.h"
#include "aom_mem/aom_mem.h"

typedef void (*SubtractFunc)(int rows, int cols, int16_t *diff_ptr,
                             ptrdiff_t diff_stride, const uint8_t *src_ptr,
                             ptrdiff_t src_stride, const uint8_t *pred_ptr,
                             ptrdiff_t pred_stride);

namespace vp9 {

class AV1SubtractBlockTest : public ::testing::TestWithParam<SubtractFunc> {
 public:
  virtual void TearDown() { libaom_test::ClearSystemState(); }
};

using libaom_test::ACMRandom;

TEST_P(AV1SubtractBlockTest, SimpleSubtract) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());

  // FIXME(rbultje) split in its own file
  for (BLOCK_SIZE bsize = BLOCK_4X4; bsize < BLOCK_SIZES;
       bsize = static_cast<BLOCK_SIZE>(static_cast<int>(bsize) + 1)) {
    const int block_width = 4 * num_4x4_blocks_wide_lookup[bsize];
    const int block_height = 4 * num_4x4_blocks_high_lookup[bsize];
    int16_t *diff = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(*diff) * block_width * block_height * 2));
    uint8_t *pred = reinterpret_cast<uint8_t *>(
        aom_memalign(16, block_width * block_height * 2));
    uint8_t *src = reinterpret_cast<uint8_t *>(
        aom_memalign(16, block_width * block_height * 2));

    for (int n = 0; n < 100; n++) {
      for (int r = 0; r < block_height; ++r) {
        for (int c = 0; c < block_width * 2; ++c) {
          src[r * block_width * 2 + c] = rnd.Rand8();
          pred[r * block_width * 2 + c] = rnd.Rand8();
        }
      }

      GetParam()(block_height, block_width, diff, block_width, src, block_width,
                 pred, block_width);

      for (int r = 0; r < block_height; ++r) {
        for (int c = 0; c < block_width; ++c) {
          EXPECT_EQ(diff[r * block_width + c],
                    (src[r * block_width + c] - pred[r * block_width + c]))
              << "r = " << r << ", c = " << c << ", bs = " << bsize;
        }
      }

      GetParam()(block_height, block_width, diff, block_width * 2, src,
                 block_width * 2, pred, block_width * 2);

      for (int r = 0; r < block_height; ++r) {
        for (int c = 0; c < block_width; ++c) {
          EXPECT_EQ(
              diff[r * block_width * 2 + c],
              (src[r * block_width * 2 + c] - pred[r * block_width * 2 + c]))
              << "r = " << r << ", c = " << c << ", bs = " << bsize;
        }
      }
    }
    aom_free(diff);
    aom_free(pred);
    aom_free(src);
  }
}

INSTANTIATE_TEST_CASE_P(C, AV1SubtractBlockTest,
                        ::testing::Values(aom_subtract_block_c));

#if HAVE_SSE2 && CONFIG_USE_X86INC
INSTANTIATE_TEST_CASE_P(SSE2, AV1SubtractBlockTest,
                        ::testing::Values(aom_subtract_block_sse2));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, AV1SubtractBlockTest,
                        ::testing::Values(aom_subtract_block_neon));
#endif
#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(MSA, AV1SubtractBlockTest,
                        ::testing::Values(aom_subtract_block_msa));
#endif

}  // namespace vp9

/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
using libaom_test::ACMRandom;

typedef void (*GetLevelCountsFunc)(const uint8_t *const levels, const int width,
                                   const int height,
                                   uint8_t *const level_counts);

class TxbTest : public ::testing::TestWithParam<GetLevelCountsFunc> {
 public:
  TxbTest() : get_br_level_counts_func_(GetParam()) {}

  virtual ~TxbTest() {}

  virtual void SetUp() {
    level_counts_ref_ = reinterpret_cast<uint8_t *>(
        aom_memalign(16, sizeof(*level_counts_ref_) * MAX_TX_SQUARE));
    ASSERT_TRUE(level_counts_ref_ != NULL);
    level_counts_ = reinterpret_cast<uint8_t *>(
        aom_memalign(16, sizeof(*level_counts_) * MAX_TX_SQUARE));
    ASSERT_TRUE(level_counts_ != NULL);
  }

  virtual void TearDown() {
    aom_free(level_counts_ref_);
    aom_free(level_counts_);
    libaom_test::ClearSystemState();
  }

  void GetLevelCountsRun() {
    const int kNumTests = 10000;
    int result = 0;

    for (int tx_size = TX_4X4; tx_size < TX_SIZES_ALL; ++tx_size) {
      const int width = tx_size_wide[tx_size];
      const int height = tx_size_high[tx_size];
      levels_ = set_levels(levels_buf_, width);
      memset(levels_buf_, 0, sizeof(*levels_buf_) * MAX_TX_SQUARE);

      for (int i = 0; i < kNumTests && !result; ++i) {
        InitLevels(width, height, i);

        av1_get_br_level_counts_c(levels_, width, height, level_counts_ref_);
        get_br_level_counts_func_(levels_, width, height, level_counts_);

        PrintDiff(width, height);

        result = memcmp(level_counts_, level_counts_ref_,
                        sizeof(*level_counts_ref_) * MAX_TX_SQUARE);

        EXPECT_EQ(result, 0) << " width " << width << " height " << height;
      }
    }
  }

  void SpeedTestGetLevelCountsRun() {
    const int kNumTests = 10000000;
    aom_usec_timer timer;

    for (int tx_size = TX_4X4; tx_size < TX_SIZES_ALL; ++tx_size) {
      const int width = tx_size_wide[tx_size];
      const int height = tx_size_high[tx_size];
      levels_ = set_levels(levels_buf_, width);
      memset(levels_buf_, 0, sizeof(*levels_buf_) * MAX_TX_SQUARE);
      InitLevels(width, height, 0);

      aom_usec_timer_start(&timer);
      for (int i = 0; i < kNumTests; ++i) {
        get_br_level_counts_func_(levels_, width, height, level_counts_);
      }
      aom_usec_timer_mark(&timer);

      const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
      printf("get_br_level_counts_%2dx%2d: %7.1f ms\n", width, height,
             elapsed_time / 1000.0);
    }
  }

 private:
  void InitLevels(const int width, const int height, const int idx) {
    const int stride = width + TX_PAD_HOR;
    // levels_[] is initialized to either the whole possible range, or small
    // values around base level.
    const int max_value = (idx & 1) ? INT8_MAX : (NUM_BASE_LEVELS + 2);

    for (int i = 0; i < height; ++i) {
      for (int j = 0; j < width; ++j) {
        levels_[i * stride + j] =
            static_cast<uint8_t>(clamp(rnd_.Rand8(), 0, max_value));
      }
    }
    for (int i = 0; i < MAX_TX_SQUARE; ++i) {
      level_counts_ref_[i] = level_counts_[i] = rnd_.Rand8();
    }
  }

  void PrintDiff(const int width, const int height) const {
    if (memcmp(level_counts_, level_counts_ref_,
               sizeof(*level_counts_ref_) * MAX_TX_SQUARE)) {
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          if (level_counts_ref_[y * width + x] !=
              level_counts_[y * width + x]) {
            printf("count[%d][%d] diff:%6d (ref),%6d (opt)\n", y, x,
                   level_counts_ref_[y * width + x],
                   level_counts_[y * width + x]);
            break;
          }
        }
      }
    }
  }

  GetLevelCountsFunc get_br_level_counts_func_;
  ACMRandom rnd_;
  tran_low_t *coeff_;
  uint8_t levels_buf_[TX_PAD_2D];
  uint8_t *levels_;
  uint8_t *level_counts_ref_;
  uint8_t *level_counts_;
};

TEST_P(TxbTest, BitExact) { GetLevelCountsRun(); }

TEST_P(TxbTest, DISABLED_Speed) { SpeedTestGetLevelCountsRun(); }

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, TxbTest,
                        ::testing::Values(av1_get_br_level_counts_sse2));
#endif
}  // namespace

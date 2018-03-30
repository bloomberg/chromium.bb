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

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_ports/mem.h"
#include "av1/common/scan.h"
#include "av1/common/txb_common.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
using libaom_test::ACMRandom;

class BuildCompDiffwtdMaskTest : public ::testing::TestWithParam<int> {
 public:
  virtual ~BuildCompDiffwtdMaskTest() {}

  virtual void TearDown() { libaom_test::ClearSystemState(); }
  void RunTest(const int sb_type, const int is_speed,
               const DIFFWTD_MASK_TYPE type);

 private:
  ACMRandom rnd_;
};

void BuildCompDiffwtdMaskTest::RunTest(const int sb_type, const int is_speed,
                                       const DIFFWTD_MASK_TYPE type) {
  const int width = block_size_wide[sb_type];
  const int height = block_size_high[sb_type];
  DECLARE_ALIGNED(16, uint8_t, mask_ref[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, mask_test[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, src0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, src1[MAX_SB_SQUARE]);
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int i = 0; i < width * height; i++) {
    src0[i] = rnd.Rand8();
    src1[i] = rnd.Rand8();
  }
  const int run_times = is_speed ? (10000000 / (width + height)) : 1;
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_times; ++i) {
    av1_build_compound_diffwtd_mask_c(mask_ref, type, src0, width, src1, width,
                                      height, width);
  }
  const double t1 = get_time_mark(&timer);
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_times; ++i) {
    av1_build_compound_diffwtd_mask_sse4_1(mask_test, type, src0, width, src1,
                                           width, height, width);
  }
  const double t2 = get_time_mark(&timer);
  if (is_speed) {
    printf("mask %d %3dx%-3d:%7.2f/%7.2fns", type, width, height, t1, t2);
    printf("(%3.2f)\n", t1 / t2);
  }
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      ASSERT_EQ(mask_ref[c + r * width], mask_test[c + r * width])
          << "[" << r << "," << c << "] " << run_times << " @ " << width << "x"
          << height << " inv " << type;
    }
  }
}

TEST_P(BuildCompDiffwtdMaskTest, match) {
  RunTest(GetParam(), 0, DIFFWTD_38);
  RunTest(GetParam(), 0, DIFFWTD_38_INV);
}
TEST_P(BuildCompDiffwtdMaskTest, DISABLED_Speed) {
  RunTest(GetParam(), 1, DIFFWTD_38);
  RunTest(GetParam(), 1, DIFFWTD_38_INV);
}

INSTANTIATE_TEST_CASE_P(SSE4_1, BuildCompDiffwtdMaskTest,
                        ::testing::Range(0, static_cast<int>(BLOCK_SIZES_ALL),
                                         1));
}  // namespace

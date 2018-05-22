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

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "aom_mem/aom_mem.h"

using libaom_test::ACMRandom;

namespace {
class AverageTestBase : public ::testing::Test {
 public:
  AverageTestBase(int width, int height) : width_(width), height_(height) {}

  static void SetUpTestCase() {
    source_data_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBlockSize));
  }

  static void TearDownTestCase() {
    aom_free(source_data_);
    source_data_ = NULL;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  // Handle blocks up to 4 blocks 64x64 with stride up to 128
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 64 * 128;

  virtual void SetUp() {
    source_stride_ = (width_ + 31) & ~31;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  void FillConstant(uint8_t fill_constant) {
    for (int i = 0; i < width_ * height_; ++i) {
      source_data_[i] = fill_constant;
    }
  }

  void FillRandom() {
    for (int i = 0; i < width_ * height_; ++i) {
      source_data_[i] = rnd_.Rand8();
    }
  }

  int width_, height_;
  static uint8_t *source_data_;
  int source_stride_;

  ACMRandom rnd_;
};

typedef int (*SatdFunc)(const int16_t *coeffs, int length);
typedef ::testing::tuple<int, SatdFunc> SatdTestParam;

class SatdTest : public ::testing::Test,
                 public ::testing::WithParamInterface<SatdTestParam> {
 protected:
  virtual void SetUp() {
    satd_size_ = GET_PARAM(0);
    satd_func_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(*src_) * satd_size_));
    ASSERT_TRUE(src_ != NULL);
  }

  virtual void TearDown() {
    libaom_test::ClearSystemState();
    aom_free(src_);
  }

  void FillConstant(const int16_t val) {
    for (int i = 0; i < satd_size_; ++i) src_[i] = val;
  }

  void FillRandom() {
    for (int i = 0; i < satd_size_; ++i) src_[i] = rnd_.Rand16();
  }

  void Check(int expected) {
    int total;
    ASM_REGISTER_STATE_CHECK(total = satd_func_(src_, satd_size_));
    EXPECT_EQ(expected, total);
  }

  int satd_size_;

 private:
  int16_t *src_;
  SatdFunc satd_func_;
  ACMRandom rnd_;
};

uint8_t *AverageTestBase::source_data_ = NULL;

TEST_P(SatdTest, MinValue) {
  const int kMin = -32640;
  const int expected = -kMin * satd_size_;
  FillConstant(kMin);
  Check(expected);
}

TEST_P(SatdTest, MaxValue) {
  const int kMax = 32640;
  const int expected = kMax * satd_size_;
  FillConstant(kMax);
  Check(expected);
}

TEST_P(SatdTest, Random) {
  int expected;
  switch (satd_size_) {
    case 16: expected = 205298; break;
    case 64: expected = 1113950; break;
    case 256: expected = 4268415; break;
    case 1024: expected = 16954082; break;
    default:
      FAIL() << "Invalid satd size (" << satd_size_
             << ") valid: 16/64/256/1024";
  }
  FillRandom();
  Check(expected);
}

using ::testing::make_tuple;

INSTANTIATE_TEST_CASE_P(C, SatdTest,
                        ::testing::Values(make_tuple(16, &aom_satd_c),
                                          make_tuple(64, &aom_satd_c),
                                          make_tuple(256, &aom_satd_c),
                                          make_tuple(1024, &aom_satd_c)));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, SatdTest,
                        ::testing::Values(make_tuple(16, &aom_satd_sse2),
                                          make_tuple(64, &aom_satd_sse2),
                                          make_tuple(256, &aom_satd_sse2),
                                          make_tuple(1024, &aom_satd_sse2)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, SatdTest,
                        ::testing::Values(make_tuple(16, &aom_satd_neon),
                                          make_tuple(64, &aom_satd_neon),
                                          make_tuple(256, &aom_satd_neon),
                                          make_tuple(1024, &aom_satd_neon)));
#endif

}  // namespace

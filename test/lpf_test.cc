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

#include <cmath>
#include <cstdlib>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/entropy.h"
#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
// Horizontally and Vertically need 32x32: 8  Coeffs preceeding filtered section
//                                         16 Coefs within filtered section
//                                         8  Coeffs following filtered section
const int kNumCoeffs = 1024;

const int number_of_iterations = 10000;

const int kSpeedTestNum = 500000;

typedef uint16_t Pixel;
#define PIXEL_WIDTH 16

typedef void (*loop_op_t)(Pixel *s, int p, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh, int bd);
typedef void (*dual_loop_op_t)(Pixel *s, int p, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1, int bd);

typedef std::tr1::tuple<loop_op_t, loop_op_t, int> loop8_param_t;
typedef std::tr1::tuple<dual_loop_op_t, dual_loop_op_t, int> dualloop8_param_t;

void InitInput(Pixel *s, Pixel *ref_s, ACMRandom *rnd, const uint8_t limit,
               const int mask, const int32_t p, const int i) {
  uint16_t tmp_s[kNumCoeffs];

  for (int j = 0; j < kNumCoeffs;) {
    const uint8_t val = rnd->Rand8();
    if (val & 0x80) {  // 50% chance to choose a new value.
      tmp_s[j] = rnd->Rand16();
      j++;
    } else {  // 50% chance to repeat previous value in row X times.
      int k = 0;
      while (k++ < ((val & 0x1f) + 1) && j < kNumCoeffs) {
        if (j < 1) {
          tmp_s[j] = rnd->Rand16();
        } else if (val & 0x20) {  // Increment by a value within the limit.
          tmp_s[j] = tmp_s[j - 1] + (limit - 1);
        } else {  // Decrement by a value within the limit.
          tmp_s[j] = tmp_s[j - 1] - (limit - 1);
        }
        j++;
      }
    }
  }

  for (int j = 0; j < kNumCoeffs;) {
    const uint8_t val = rnd->Rand8();
    if (val & 0x80) {
      j++;
    } else {  // 50% chance to repeat previous value in column X times.
      int k = 0;
      while (k++ < ((val & 0x1f) + 1) && j < kNumCoeffs) {
        if (j < 1) {
          tmp_s[j] = rnd->Rand16();
        } else if (val & 0x20) {  // Increment by a value within the limit.
          tmp_s[(j % 32) * 32 + j / 32] =
              tmp_s[((j - 1) % 32) * 32 + (j - 1) / 32] + (limit - 1);
        } else {  // Decrement by a value within the limit.
          tmp_s[(j % 32) * 32 + j / 32] =
              tmp_s[((j - 1) % 32) * 32 + (j - 1) / 32] - (limit - 1);
        }
        j++;
      }
    }
  }

  for (int j = 0; j < kNumCoeffs; j++) {
    if (i % 2) {
      s[j] = tmp_s[j] & mask;
    } else {
      s[j] = tmp_s[p * (j % p) + j / p] & mask;
    }
    ref_s[j] = s[j];
  }
}

uint8_t GetOuterThresh(ACMRandom *rnd) {
  return static_cast<uint8_t>(rnd->PseudoUniform(3 * MAX_LOOP_FILTER + 5));
}

uint8_t GetInnerThresh(ACMRandom *rnd) {
  return static_cast<uint8_t>(rnd->PseudoUniform(MAX_LOOP_FILTER + 1));
}

uint8_t GetHevThresh(ACMRandom *rnd) {
  return static_cast<uint8_t>(rnd->PseudoUniform(MAX_LOOP_FILTER + 1) >> 4);
}

class Loop8Test6Param : public ::testing::TestWithParam<loop8_param_t> {
 public:
  virtual ~Loop8Test6Param() {}
  virtual void SetUp() {
    loopfilter_op_ = GET_PARAM(0);
    ref_loopfilter_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bit_depth_;
  int mask_;
  loop_op_t loopfilter_op_;
  loop_op_t ref_loopfilter_op_;
};

class Loop8Test9Param : public ::testing::TestWithParam<dualloop8_param_t> {
 public:
  virtual ~Loop8Test9Param() {}
  virtual void SetUp() {
    loopfilter_op_ = GET_PARAM(0);
    ref_loopfilter_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bit_depth_;
  int mask_;
  dual_loop_op_t loopfilter_op_;
  dual_loop_op_t ref_loopfilter_op_;
};

TEST_P(Loop8Test6Param, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
  const int32_t p = kNumCoeffs / 32;
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, s[kNumCoeffs]);
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, ref_s[kNumCoeffs]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = GetOuterThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetInnerThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetHevThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    InitInput(s, ref_s, &rnd, *limit, mask_, p, i);
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit, limit, thresh, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh, bit_depth_));

    for (int j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test6Param, C output doesn't match SSE2 "
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test6Param, ValueCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, s[kNumCoeffs]);
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, ref_s[kNumCoeffs]);
  int err_count_total = 0;
  int first_failure = -1;

  // NOTE: The code in av1_loopfilter.c:update_sharpness computes mblim as a
  // function of sharpness_lvl and the loopfilter lvl as:
  // block_inside_limit = lvl >> ((sharpness_lvl > 0) + (sharpness_lvl > 4));
  // ...
  // memset(lfi->lfthr[lvl].mblim, (2 * (lvl + 2) + block_inside_limit),
  //        SIMD_WIDTH);
  // This means that the largest value for mblim will occur when sharpness_lvl
  // is equal to 0, and lvl is equal to its greatest value (MAX_LOOP_FILTER).
  // In this case block_inside_limit will be equal to MAX_LOOP_FILTER and
  // therefore mblim will be equal to (2 * (lvl + 2) + block_inside_limit) =
  // 2 * (MAX_LOOP_FILTER + 2) + MAX_LOOP_FILTER = 3 * MAX_LOOP_FILTER + 4

  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = GetOuterThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetInnerThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetHevThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;
    for (int j = 0; j < kNumCoeffs; ++j) {
      s[j] = rnd.Rand16() & mask_;
      ref_s[j] = s[j];
    }
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit, limit, thresh, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh, bit_depth_));

    for (int j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test6Param, C output doesn't match SSE2 "
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test6Param, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = kSpeedTestNum;
  const int32_t bd = bit_depth_;
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);

  uint8_t tmp = GetOuterThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  blimit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetInnerThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  limit[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                 tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetHevThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  thresh[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };

  int32_t p = kNumCoeffs / 32;
  for (int j = 0; j < kNumCoeffs; ++j) {
    s[j] = rnd.Rand16() & mask_;
  }

  for (int i = 0; i < count_test_block; ++i) {
    loopfilter_op_(s + 8 + p * 8, p, blimit, limit, thresh, bd);
  }
}

TEST_P(Loop8Test9Param, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, s[kNumCoeffs]);
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, ref_s[kNumCoeffs]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = GetOuterThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetInnerThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetHevThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetOuterThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetInnerThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetHevThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;
    const uint8_t limit = *limit0 < *limit1 ? *limit0 : *limit1;
    InitInput(s, ref_s, &rnd, limit, mask_, p, i);
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,
                       limit1, thresh1, bit_depth_);
    ASM_REGISTER_STATE_CHECK(loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0,
                                            thresh0, blimit1, limit1, thresh1,
                                            bit_depth_));

    for (int j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test9Param, C output doesn't match SSE2 "
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test9Param, ValueCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = number_of_iterations;
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, s[kNumCoeffs]);
  DECLARE_ALIGNED(PIXEL_WIDTH, Pixel, ref_s[kNumCoeffs]);
  int err_count_total = 0;
  int first_failure = -1;
  for (int i = 0; i < count_test_block; ++i) {
    int err_count = 0;
    uint8_t tmp = GetOuterThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetInnerThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetHevThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetOuterThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetInnerThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                    tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    tmp = GetHevThresh(&rnd);
    DECLARE_ALIGNED(16, const uint8_t,
                    thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                     tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
    int32_t p = kNumCoeffs / 32;  // TODO(pdlf) can we have non-square here?
    for (int j = 0; j < kNumCoeffs; ++j) {
      s[j] = rnd.Rand16() & mask_;
      ref_s[j] = s[j];
    }
    ref_loopfilter_op_(ref_s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1,
                       limit1, thresh1, bit_depth_);
    ASM_REGISTER_STATE_CHECK(loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0,
                                            thresh0, blimit1, limit1, thresh1,
                                            bit_depth_));

    for (int j = 0; j < kNumCoeffs; ++j) {
      err_count += ref_s[j] != s[j];
    }
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Loop8Test9Param, C output doesn't match SSE2"
         "loopfilter output. "
      << "First failed at test case " << first_failure;
}

TEST_P(Loop8Test9Param, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = kSpeedTestNum;
  DECLARE_ALIGNED(16, uint16_t, s[kNumCoeffs]);

  uint8_t tmp = GetOuterThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  blimit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetInnerThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  limit0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetHevThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  thresh0[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetOuterThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  blimit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetInnerThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  limit1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                  tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  tmp = GetHevThresh(&rnd);
  DECLARE_ALIGNED(16, const uint8_t,
                  thresh1[16]) = { tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp,
                                   tmp, tmp, tmp, tmp, tmp, tmp, tmp, tmp };
  int32_t p = kNumCoeffs / 32;  // TODO(pdlf) can we have non-square here?
  for (int j = 0; j < kNumCoeffs; ++j) {
    s[j] = rnd.Rand16() & mask_;
  }

  for (int i = 0; i < count_test_block; ++i) {
    const int32_t bd = bit_depth_;
    loopfilter_op_(s + 8 + p * 8, p, blimit0, limit0, thresh0, blimit1, limit1,
                   thresh1, bd);
  }
}

using std::tr1::make_tuple;

#if HAVE_SSE2

const loop8_param_t kHbdLoop8Test6[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             8),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             8),
#if !CONFIG_DEBLOCK_13TAP
  // Despite the name the following funcition is doing 15-tap filtering
  // which is changed to 13-tap and not yet implemented in SIMD
  make_tuple(&aom_highbd_lpf_horizontal_16_sse2,
             &aom_highbd_lpf_horizontal_16_c, 8),
#endif
#if !CONFIG_DEBLOCK_13TAP  // No SIMD implementation for deblock_13tap yet
  make_tuple(&aom_highbd_lpf_horizontal_16_dual_sse2,
             &aom_highbd_lpf_horizontal_16_dual_c, 8),
#endif
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 8),
#if !CONFIG_DEBLOCK_13TAP  // No SIMD implementation for deblock_13tap yet
  make_tuple(&aom_highbd_lpf_vertical_16_sse2, &aom_highbd_lpf_vertical_16_c,
             8),
#endif
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             10),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             10),
#if !CONFIG_DEBLOCK_13TAP
  // Despite the name the following funcition is doing 15-tap filtering
  // which is changed to 13-tap and not yet implemented in SIMD
  make_tuple(&aom_highbd_lpf_horizontal_16_sse2,
             &aom_highbd_lpf_horizontal_16_c, 10),
#endif
#if !CONFIG_DEBLOCK_13TAP  // No SIMD implementation for deblock_13tap yet
  make_tuple(&aom_highbd_lpf_horizontal_16_dual_sse2,
             &aom_highbd_lpf_horizontal_16_dual_c, 10),
#endif
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 10),
#if !CONFIG_DEBLOCK_13TAP  // No SIMD implementation for deblock_13tap yet
  make_tuple(&aom_highbd_lpf_vertical_16_sse2, &aom_highbd_lpf_vertical_16_c,
             10),
#endif
  make_tuple(&aom_highbd_lpf_horizontal_4_sse2, &aom_highbd_lpf_horizontal_4_c,
             12),
  make_tuple(&aom_highbd_lpf_vertical_4_sse2, &aom_highbd_lpf_vertical_4_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_sse2, &aom_highbd_lpf_horizontal_8_c,
             12),
#if !CONFIG_DEBLOCK_13TAP
  // Despite the name the following funcition is doing 15-tap filtering
  // which is changed to 13-tap and not yet implemented in SIMD
  make_tuple(&aom_highbd_lpf_horizontal_16_sse2,
             &aom_highbd_lpf_horizontal_16_c, 12),
#endif
#if !CONFIG_DEBLOCK_13TAP  // No SIMD implementation for deblock_13tap yet
  make_tuple(&aom_highbd_lpf_horizontal_16_dual_sse2,
             &aom_highbd_lpf_horizontal_16_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_16_sse2, &aom_highbd_lpf_vertical_16_c,
             12),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_sse2,
             &aom_highbd_lpf_vertical_16_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_sse2,
             &aom_highbd_lpf_vertical_16_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_sse2,
             &aom_highbd_lpf_vertical_16_dual_c, 12),
#endif
  make_tuple(&aom_highbd_lpf_vertical_8_sse2, &aom_highbd_lpf_vertical_8_c, 12)
};

INSTANTIATE_TEST_CASE_P(SSE2, Loop8Test6Param,
                        ::testing::ValuesIn(kHbdLoop8Test6));
#endif  // HAVE_SSE2

#if HAVE_AVX2
#if !CONFIG_DEBLOCK_13TAP  // No SIMD implementation for deblock_13tap yet
const loop8_param_t kHbdLoop8Test6Avx2[] = {
  make_tuple(&aom_highbd_lpf_horizontal_16_dual_avx2,
             &aom_highbd_lpf_horizontal_16_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_16_dual_avx2,
             &aom_highbd_lpf_horizontal_16_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_16_dual_avx2,
             &aom_highbd_lpf_horizontal_16_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_avx2,
             &aom_highbd_lpf_vertical_16_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_avx2,
             &aom_highbd_lpf_vertical_16_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_16_dual_avx2,
             &aom_highbd_lpf_vertical_16_dual_c, 12)
};

INSTANTIATE_TEST_CASE_P(AVX2, Loop8Test6Param,
                        ::testing::ValuesIn(kHbdLoop8Test6Avx2));

#endif
#endif

#if HAVE_SSE2
const dualloop8_param_t kHbdLoop8Test9[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_sse2,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_sse2,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_sse2,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_sse2,
             &aom_highbd_lpf_vertical_8_dual_c, 12)
};

INSTANTIATE_TEST_CASE_P(SSE2, Loop8Test9Param,
                        ::testing::ValuesIn(kHbdLoop8Test9));
#endif  // HAVE_SSE2

#if HAVE_AVX2
const dualloop8_param_t kHbdLoop8Test9Avx2[] = {
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_4_dual_avx2,
             &aom_highbd_lpf_horizontal_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_horizontal_8_dual_avx2,
             &aom_highbd_lpf_horizontal_8_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_4_dual_avx2,
             &aom_highbd_lpf_vertical_4_dual_c, 12),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 8),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 10),
  make_tuple(&aom_highbd_lpf_vertical_8_dual_avx2,
             &aom_highbd_lpf_vertical_8_dual_c, 12),
};

INSTANTIATE_TEST_CASE_P(AVX2, Loop8Test9Param,
                        ::testing::ValuesIn(kHbdLoop8Test9Avx2));
#endif

}  // namespace

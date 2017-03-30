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

#include <cstdlib>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "av1/common/od_dering.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {

typedef int (*dering_dir_t)(uint16_t *y, int ystride, const uint16_t *in,
                            int threshold, int dir);

typedef std::tr1::tuple<dering_dir_t, dering_dir_t, int> dering_dir_param_t;

class DeringDirTest : public ::testing::TestWithParam<dering_dir_param_t> {
 public:
  virtual ~DeringDirTest() {}
  virtual void SetUp() {
    dering = GET_PARAM(0);
    ref_dering = GET_PARAM(1);
    bsize = GET_PARAM(2);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bsize;
  dering_dir_t dering;
  dering_dir_t ref_dering;
};

typedef DeringDirTest DeringSpeedTest;

void test_dering(int bsize, int iterations,
                 int (*dering)(uint16_t *y, int ystride, const uint16_t *in,
                               int threshold, int dir),
                 int (*ref_dering)(uint16_t *y, int ystride, const uint16_t *in,
                                   int threshold, int dir)) {
  const int size = 8;
  const int ysize = size + 2 * OD_FILT_VBORDER;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[ysize * OD_FILT_BSTRIDE]);
  DECLARE_ALIGNED(16, static uint16_t, d[size * size]);
  DECLARE_ALIGNED(16, static uint16_t, ref_d[size * size]);
  memset(ref_d, 0, sizeof(ref_d));
  memset(d, 0, sizeof(d));

  int error = 0, threshold = 0, dir;
  int depth, bits, level, count, errdepth = 0, errthreshold = 0;
  unsigned int pos = 0;
  int ref_res = 0, res = 0;

  for (depth = 8; depth <= 12; depth += 2) {
    for (count = 0; count < iterations; count++) {
      for (level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (dir = 0; dir < 8; dir++) {
            for (threshold = 0; threshold < 64 << (depth - 8) && !error;
                 threshold += !error << (depth - 8)) {
              ref_res = ref_dering(
                  ref_d, size,
                  s + OD_FILT_HBORDER + OD_FILT_VBORDER * OD_FILT_BSTRIDE,
                  threshold, dir);
              // If dering and ref_dering are the same, we're just testing speed
              if (dering != ref_dering)
                ASM_REGISTER_STATE_CHECK(
                    res = dering(d, size, s + OD_FILT_HBORDER +
                                              OD_FILT_VBORDER * OD_FILT_BSTRIDE,
                                 threshold, dir));
              if (ref_dering != dering) {
                for (pos = 0; pos < sizeof(d) / sizeof(*d) && !error; pos++) {
                  error = ref_d[pos] != d[pos];
                  errdepth = depth;
                  errthreshold = threshold;
                }
                error |= res != ref_res;
              }
            }
          }
        }
      }
    }
  }

  pos--;
  EXPECT_EQ(0, error) << "Error: DeringDirTest, SIMD and C mismatch."
                      << std::endl
                      << "First error at " << pos % size << "," << pos / size
                      << " (" << (int16_t)ref_d[pos] << " : " << (int16_t)d[pos]
                      << ") " << std::endl
                      << "return: " << res << " : " << ref_res << std::endl
                      << "threshold: " << errthreshold << std::endl
                      << "depth: " << errdepth << std::endl
                      << "size: " << bsize << std::endl
                      << std::endl;
}

void test_dering_speed(int bsize, int iterations,
                       int (*dering)(uint16_t *y, int ystride,
                                     const uint16_t *in, int threshold,
                                     int dir),
                       int (*ref_dering)(uint16_t *y, int ystride,
                                         const uint16_t *in, int threshold,
                                         int dir)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_dering(bsize, iterations, ref_dering, ref_dering);
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_dering(bsize, iterations, dering, dering);
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

#if 0
  std::cout << "[          ] C time = " << ref_elapsed_time / 1000
            << " ms, SIMD time = " << elapsed_time / 1000 << " ms" << std::endl;
#endif

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: DeringSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

typedef int (*find_dir_t)(const od_dering_in *img, int stride, int32_t *var,
                          int coeff_shift);

typedef std::tr1::tuple<find_dir_t, find_dir_t> find_dir_param_t;

class DeringFindDirTest : public ::testing::TestWithParam<find_dir_param_t> {
 public:
  virtual ~DeringFindDirTest() {}
  virtual void SetUp() {
    finddir = GET_PARAM(0);
    ref_finddir = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  find_dir_t finddir;
  find_dir_t ref_finddir;
};

typedef DeringFindDirTest DeringFindDirSpeedTest;

void test_finddir(int (*finddir)(const od_dering_in *img, int stride,
                                 int32_t *var, int coeff_shift),
                  int (*ref_finddir)(const od_dering_in *img, int stride,
                                     int32_t *var, int coeff_shift)) {
  const int size = 8;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size * size]);

  int error = 0;
  int depth, bits, level, count, errdepth = 0;
  int ref_res = 0, res = 0;
  int32_t ref_var = 0, var = 0;

  for (depth = 8; depth <= 12 && !error; depth += 2) {
    for (count = 0; count < 512 && !error; count++) {
      for (level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (int c = 0; c < 1 + 9 * (finddir == ref_finddir); c++)
            ref_res = ref_finddir(s, size, &ref_var, depth - 8);
          if (finddir != ref_finddir)
            ASM_REGISTER_STATE_CHECK(res = finddir(s, size, &var, depth - 8));
          if (ref_finddir != finddir) {
            if (res != ref_res || var != ref_var) error = 1;
            errdepth = depth;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, error) << "Error: DeringFindDirTest, SIMD and C mismatch."
                      << std::endl
                      << "return: " << res << " : " << ref_res << std::endl
                      << "var: " << var << " : " << ref_var << std::endl
                      << "depth: " << errdepth << std::endl
                      << std::endl;
}

void test_finddir_speed(int (*finddir)(const od_dering_in *img, int stride,
                                       int32_t *var, int coeff_shift),
                        int (*ref_finddir)(const od_dering_in *img, int stride,
                                           int32_t *var, int coeff_shift)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_finddir(ref_finddir, ref_finddir);
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_finddir(finddir, finddir);
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

#if 0
  std::cout << "[          ] C time = " << ref_elapsed_time / 1000
            << " ms, SIMD time = " << elapsed_time / 1000 << " ms" << std::endl;
#endif

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: DeringFindDirSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

TEST_P(DeringDirTest, TestSIMDNoMismatch) {
  test_dering(bsize, 1, dering, ref_dering);
}

TEST_P(DeringSpeedTest, DISABLED_TestSpeed) {
  test_dering_speed(bsize, 4, dering, ref_dering);
}

TEST_P(DeringFindDirTest, TestSIMDNoMismatch) {
  test_finddir(finddir, ref_finddir);
}

TEST_P(DeringFindDirSpeedTest, DISABLED_TestSpeed) {
  test_finddir_speed(finddir, ref_finddir);
}

using std::tr1::make_tuple;

// VS compiling for 32 bit targets does not support vector types in
// structs as arguments, which makes the v256 type of the intrinsics
// hard to support, so optimizations for this target are disabled.
#if defined(_WIN64) || !defined(_MSC_VER) || defined(__clang__)
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, DeringDirTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_sse2,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_sse2,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(SSE2, DeringFindDirTest,
                        ::testing::Values(make_tuple(&od_dir_find8_sse2,
                                                     &od_dir_find8_c)));
#endif
#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, DeringDirTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_ssse3,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_ssse3,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(SSSE3, DeringFindDirTest,
                        ::testing::Values(make_tuple(&od_dir_find8_ssse3,
                                                     &od_dir_find8_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, DeringDirTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_sse4_1,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_sse4_1,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(SSE4_1, DeringFindDirTest,
                        ::testing::Values(make_tuple(&od_dir_find8_sse4_1,
                                                     &od_dir_find8_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, DeringDirTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_neon,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_neon,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(NEON, DeringFindDirTest,
                        ::testing::Values(make_tuple(&od_dir_find8_neon,
                                                     &od_dir_find8_c)));
#endif

// Test speed for all supported architectures
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, DeringSpeedTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_sse2,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_sse2,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(SSE2, DeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&od_dir_find8_sse2,
                                                     &od_dir_find8_c)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, DeringSpeedTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_ssse3,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_ssse3,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(SSSE3, DeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&od_dir_find8_ssse3,
                                                     &od_dir_find8_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, DeringSpeedTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_sse4_1,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_sse4_1,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(SSE4_1, DeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&od_dir_find8_sse4_1,
                                                     &od_dir_find8_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, DeringSpeedTest,
    ::testing::Values(make_tuple(&od_filter_dering_direction_4x4_neon,
                                 &od_filter_dering_direction_4x4_c, 4),
                      make_tuple(&od_filter_dering_direction_8x8_neon,
                                 &od_filter_dering_direction_8x8_c, 8)));
INSTANTIATE_TEST_CASE_P(NEON, DeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&od_dir_find8_neon,
                                                     &od_dir_find8_c)));
#endif

#endif  // defined(_WIN64) || !defined(_MSC_VER)
}  // namespace

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
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "aom_ports/aom_timer.h"
#include "./av1_rtcd.h"
#include "test/util.h"
#include "test/acm_random.h"

using std::tr1::make_tuple;

using libaom_test::ACMRandom;

#define NUM_ITERATIONS (100)
#define NUM_ITERATIONS_SPEED (INT16_MAX)

#define ALL_SIZES_CFL(function)                                     \
  make_tuple(4, 4, &function), make_tuple(8, 4, &function),         \
      make_tuple(4, 8, &function), make_tuple(8, 8, &function),     \
      make_tuple(16, 8, &function), make_tuple(8, 16, &function),   \
      make_tuple(16, 16, &function), make_tuple(32, 16, &function), \
      make_tuple(16, 32, &function), make_tuple(32, 32, &function)

namespace {
typedef void (*subtract_fn)(int16_t *pred_buf_q3, int width, int height,
                            int16_t avg_q3);

typedef std::tr1::tuple<int, int, subtract_fn> subtract_param;

static void assertFaster(int ref_elapsed_time, int elapsed_time) {
  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CFLSubtractSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

static void printSpeed(int ref_elapsed_time, int elapsed_time, int width,
                       int height) {
  std::cout.precision(2);
  std::cout << "[          ] " << width << "x" << height
            << ": C time = " << ref_elapsed_time
            << " us, SIMD time = " << elapsed_time << " us"
            << " (~" << ref_elapsed_time / (double)elapsed_time << "x) "
            << std::endl;
}

class CFLSubtractTest : public ::testing::TestWithParam<subtract_param> {
 public:
  virtual ~CFLSubtractTest() {}
  virtual void SetUp() { subtract = GET_PARAM(2); }

 protected:
  int16_t pred_buf_data[CFL_BUF_SQUARE];
  int16_t pred_buf_data_ref[CFL_BUF_SQUARE];
  subtract_fn subtract;
  int Width() const { return GET_PARAM(0); }
  int Height() const { return GET_PARAM(1); }
  void init(int width, int height) {
    int k = 0;
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i++) {
        pred_buf_data[j * CFL_BUF_LINE + i] = k;
        pred_buf_data_ref[j * CFL_BUF_LINE + i] = k++;
      }
    }
  }
};

TEST_P(CFLSubtractTest, SubtractTest) {
  const int width = Width();
  const int height = Height();

  ACMRandom rnd(ACMRandom::DeterministicSeed());

  for (int it = 0; it < NUM_ITERATIONS; it++) {
    init(width, height);
    int16_t k = rnd.Rand15Signed();
    subtract(pred_buf_data, width, height, k);
    av1_cfl_subtract_c(pred_buf_data_ref, width, height, k);
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i++) {
        ASSERT_EQ(pred_buf_data[j * CFL_BUF_LINE + i],
                  pred_buf_data_ref[j * CFL_BUF_LINE + i]);
        ASSERT_EQ(pred_buf_data[j * CFL_BUF_LINE + i], -k);
        k--;
      }
    }
  }
}

TEST_P(CFLSubtractTest, DISABLED_SubtractSpeedTest) {
  const int width = Width();
  const int height = Height();

  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  init(width, height);
  aom_usec_timer_start(&ref_timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    av1_cfl_subtract_c(pred_buf_data_ref, width, height, k);
  }
  aom_usec_timer_mark(&ref_timer);
  const int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    subtract(pred_buf_data, width, height, k);
  }
  aom_usec_timer_mark(&timer);
  const int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

#if 1
  printSpeed(ref_elapsed_time, elapsed_time, width, height);
#endif
  assertFaster(ref_elapsed_time, elapsed_time);
}

#if HAVE_SSE2
const subtract_param subtract_sizes_sse2[] = { ALL_SIZES_CFL(
    av1_cfl_subtract_sse2) };

INSTANTIATE_TEST_CASE_P(SSE2, CFLSubtractTest,
                        ::testing::ValuesIn(subtract_sizes_sse2));
#endif

#if HAVE_AVX2
const subtract_param subtract_sizes_avx2[] = { ALL_SIZES_CFL(
    av1_cfl_subtract_avx2) };

INSTANTIATE_TEST_CASE_P(AVX2, CFLSubtractTest,
                        ::testing::ValuesIn(subtract_sizes_avx2));
#endif
}  // namespace

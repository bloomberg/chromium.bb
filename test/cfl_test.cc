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

using ::testing::make_tuple;

using libaom_test::ACMRandom;

#define NUM_ITERATIONS (100)
#define NUM_ITERATIONS_SPEED (INT16_MAX)

#define ALL_CFL_TX_SIZES(function)                                     \
  make_tuple(TX_4X4, &function), make_tuple(TX_4X8, &function),        \
      make_tuple(TX_4X16, &function), make_tuple(TX_8X4, &function),   \
      make_tuple(TX_8X8, &function), make_tuple(TX_8X16, &function),   \
      make_tuple(TX_8X32, &function), make_tuple(TX_16X4, &function),  \
      make_tuple(TX_16X8, &function), make_tuple(TX_16X16, &function), \
      make_tuple(TX_16X32, &function), make_tuple(TX_32X8, &function), \
      make_tuple(TX_32X16, &function), make_tuple(TX_32X32, &function)

namespace {
typedef cfl_subsample_lbd_fn (*get_subsample_fn)(TX_SIZE tx_size);

typedef cfl_predict_lbd_fn (*get_predict_fn)(TX_SIZE tx_size);

typedef cfl_predict_hbd_fn (*get_predict_fn_hbd)(TX_SIZE tx_size);

typedef cfl_subtract_average_fn (*sub_avg_fn)(TX_SIZE tx_size);

typedef ::testing::tuple<TX_SIZE, get_subsample_fn> subsample_param;

typedef ::testing::tuple<TX_SIZE, get_predict_fn> predict_param;

typedef ::testing::tuple<TX_SIZE, get_predict_fn_hbd> predict_param_hbd;

typedef ::testing::tuple<TX_SIZE, sub_avg_fn> sub_avg_param;

template <typename A>
static void assert_eq(const A *a, const A *b, int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      ASSERT_EQ(a[j * CFL_BUF_LINE + i], b[j * CFL_BUF_LINE + i]);
    }
  }
}

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

template <typename F>
class CFLTest
    : public ::testing::TestWithParam< ::testing::tuple<TX_SIZE, F> > {
 public:
  virtual ~CFLTest() {}
  virtual void SetUp() {
    tx_size = ::testing::get<0>(this->GetParam());
    width = tx_size_wide[tx_size];
    height = tx_size_high[tx_size];
    fun_under_test = ::testing::get<1>(this->GetParam());
    rnd(ACMRandom::DeterministicSeed());
  }

 protected:
  ACMRandom rnd;
  F fun_under_test;
  TX_SIZE tx_size;
  int width;
  int height;
};

template <typename F, typename I>
class CFLTestWithData : public CFLTest<F> {
 public:
  virtual ~CFLTestWithData() {}

 protected:
  I data[CFL_BUF_SQUARE];
  I data_ref[CFL_BUF_SQUARE];

  void init(I (ACMRandom::*random)()) {
    for (int j = 0; j < this->height; j++) {
      for (int i = 0; i < this->width; i++) {
        const I d = (this->rnd.*random)();
        data[j * CFL_BUF_LINE + i] = d;
        data_ref[j * CFL_BUF_LINE + i] = d;
      }
    }
  }
};

template <typename F, typename I>
class CFLTestWithAlignedData : public CFLTest<F> {
 public:
  virtual ~CFLTestWithAlignedData() {}
  virtual void SetUp() {
    CFLTest<F>::SetUp();
    chroma_pels_ref =
        reinterpret_cast<I *>(aom_memalign(32, sizeof(I) * CFL_BUF_SQUARE));
    chroma_pels =
        reinterpret_cast<I *>(aom_memalign(32, sizeof(I) * CFL_BUF_SQUARE));
    sub_luma_pels_ref = reinterpret_cast<int16_t *>(
        aom_memalign(32, sizeof(int16_t) * CFL_BUF_SQUARE));
    sub_luma_pels = reinterpret_cast<int16_t *>(
        aom_memalign(32, sizeof(int16_t) * CFL_BUF_SQUARE));
    memset(chroma_pels_ref, 0, sizeof(I) * CFL_BUF_SQUARE);
    memset(chroma_pels, 0, sizeof(I) * CFL_BUF_SQUARE);
    memset(sub_luma_pels_ref, 0, sizeof(int16_t) * CFL_BUF_SQUARE);
    memset(sub_luma_pels, 0, sizeof(int16_t) * CFL_BUF_SQUARE);
  }

  virtual void TearDown() {
    aom_free(chroma_pels_ref);
    aom_free(sub_luma_pels_ref);
    aom_free(chroma_pels);
    aom_free(sub_luma_pels);
  }

 protected:
  I *chroma_pels_ref;
  I *chroma_pels;
  int16_t *sub_luma_pels_ref;
  int16_t *sub_luma_pels;
  int alpha_q3;
  I dc;
  void init(int bd) {
    alpha_q3 = this->rnd(33) - 16;
    dc = this->rnd(1 << bd);
    for (int j = 0; j < this->height; j++) {
      for (int i = 0; i < this->width; i++) {
        chroma_pels[j * CFL_BUF_LINE + i] = dc;
        chroma_pels_ref[j * CFL_BUF_LINE + i] = dc;
        sub_luma_pels_ref[j * CFL_BUF_LINE + i] =
            sub_luma_pels[j * CFL_BUF_LINE + i] = this->rnd.Rand15Signed();
      }
    }
  }
};

class CFLSubAvgTest : public CFLTestWithData<sub_avg_fn, int16_t> {
 public:
  virtual ~CFLSubAvgTest() {}
};

class CFLSubsampleTest : public CFLTestWithData<get_subsample_fn, uint8_t> {
 public:
  virtual ~CFLSubsampleTest() {}
};

class CFLPredictTest : public CFLTestWithAlignedData<get_predict_fn, uint8_t> {
 public:
  virtual ~CFLPredictTest() {}
};

class CFLPredictHBDTest
    : public CFLTestWithAlignedData<get_predict_fn_hbd, uint16_t> {
 public:
  virtual ~CFLPredictHBDTest() {}
};

TEST_P(CFLSubAvgTest, SubAvgTest) {
  const cfl_subtract_average_fn ref_sub = get_subtract_average_fn_c(tx_size);
  const cfl_subtract_average_fn sub = fun_under_test(tx_size);
  for (int it = 0; it < NUM_ITERATIONS; it++) {
    init(&ACMRandom::Rand15Signed);
    sub(data);
    ref_sub(data_ref);
    assert_eq<int16_t>(data, data_ref, width, height);
  }
}

TEST_P(CFLSubAvgTest, DISABLED_SubAvgSpeedTest) {
  const cfl_subtract_average_fn ref_sub = get_subtract_average_fn_c(tx_size);
  const cfl_subtract_average_fn sub = fun_under_test(tx_size);

  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  init(&ACMRandom::Rand15Signed);
  aom_usec_timer_start(&ref_timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    ref_sub(data_ref);
  }
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    sub(data);
  }
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

  printSpeed(ref_elapsed_time, elapsed_time, width, height);
  assertFaster(ref_elapsed_time, elapsed_time);
}

TEST_P(CFLSubsampleTest, SubsampleTest) {
  int16_t sub_luma_pels[CFL_BUF_SQUARE];
  int16_t sub_luma_pels_ref[CFL_BUF_SQUARE];
  const int sub_width = width >> 1;
  const int sub_height = height >> 1;

  for (int it = 0; it < NUM_ITERATIONS; it++) {
    init(&ACMRandom::Rand8);
    fun_under_test(tx_size)(data, CFL_BUF_LINE, sub_luma_pels);
    cfl_get_luma_subsampling_420_lbd_c(tx_size)(data_ref, CFL_BUF_LINE,
                                                sub_luma_pels_ref);
    assert_eq<int16_t>(sub_luma_pels, sub_luma_pels_ref, sub_width, sub_height);
  }
}

TEST_P(CFLSubsampleTest, DISABLED_SubsampleSpeedTest) {
  int16_t sub_luma_pels[CFL_BUF_SQUARE];
  int16_t sub_luma_pels_ref[CFL_BUF_SQUARE];
  cfl_subsample_lbd_fn subsample = fun_under_test(tx_size);
  cfl_subsample_lbd_fn subsample_ref =
      cfl_get_luma_subsampling_420_lbd_c(tx_size);
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  init(&ACMRandom::Rand8);
  aom_usec_timer_start(&ref_timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    subsample_ref(data_ref, CFL_BUF_LINE, sub_luma_pels);
  }
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    subsample(data, CFL_BUF_LINE, sub_luma_pels_ref);
  }
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

  printSpeed(ref_elapsed_time, elapsed_time, width, height);
  assertFaster(ref_elapsed_time, elapsed_time);
}

TEST_P(CFLPredictTest, PredictTest) {
  for (int it = 0; it < NUM_ITERATIONS; it++) {
    init(8);
    fun_under_test(tx_size)(sub_luma_pels, chroma_pels, CFL_BUF_LINE, alpha_q3);
    get_predict_lbd_fn_c(tx_size)(sub_luma_pels_ref, chroma_pels_ref,
                                  CFL_BUF_LINE, alpha_q3);

    assert_eq<uint8_t>(chroma_pels, chroma_pels_ref, width, height);
  }
}

TEST_P(CFLPredictTest, DISABLED_PredictSpeedTest) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  init(8);
  cfl_predict_lbd_fn predict_impl = get_predict_lbd_fn_c(tx_size);
  aom_usec_timer_start(&ref_timer);

  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    predict_impl(sub_luma_pels_ref, chroma_pels_ref, CFL_BUF_LINE, alpha_q3);
  }
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  predict_impl = fun_under_test(tx_size);
  aom_usec_timer_start(&timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    predict_impl(sub_luma_pels, chroma_pels, CFL_BUF_LINE, alpha_q3);
  }
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

  printSpeed(ref_elapsed_time, elapsed_time, width, height);
  assertFaster(ref_elapsed_time, elapsed_time);
}

TEST_P(CFLPredictHBDTest, PredictHBDTest) {
  int bd = 12;
  for (int it = 0; it < NUM_ITERATIONS; it++) {
    init(bd);
    fun_under_test(tx_size)(sub_luma_pels, chroma_pels, CFL_BUF_LINE, alpha_q3,
                            bd);
    get_predict_hbd_fn_c(tx_size)(sub_luma_pels_ref, chroma_pels_ref,
                                  CFL_BUF_LINE, alpha_q3, bd);

    assert_eq<uint16_t>(chroma_pels, chroma_pels_ref, width, height);
  }
}

TEST_P(CFLPredictHBDTest, DISABLED_PredictHBDSpeedTest) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;
  int bd = 12;
  init(bd);
  cfl_predict_hbd_fn predict_impl = get_predict_hbd_fn_c(tx_size);
  aom_usec_timer_start(&ref_timer);

  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    predict_impl(sub_luma_pels_ref, chroma_pels_ref, CFL_BUF_LINE, alpha_q3,
                 bd);
  }
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  predict_impl = fun_under_test(tx_size);
  aom_usec_timer_start(&timer);
  for (int k = 0; k < NUM_ITERATIONS_SPEED; k++) {
    predict_impl(sub_luma_pels, chroma_pels, CFL_BUF_LINE, alpha_q3, bd);
  }
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

  printSpeed(ref_elapsed_time, elapsed_time, width, height);
  assertFaster(ref_elapsed_time, elapsed_time);
}

#if HAVE_SSE2
const sub_avg_param sub_avg_sizes_sse2[] = { ALL_CFL_TX_SIZES(
    get_subtract_average_fn_sse2) };

INSTANTIATE_TEST_CASE_P(SSE2, CFLSubAvgTest,
                        ::testing::ValuesIn(sub_avg_sizes_sse2));

#endif

#if HAVE_SSSE3

const subsample_param subsample_sizes_ssse3[] = { ALL_CFL_TX_SIZES(
    cfl_get_luma_subsampling_420_lbd_ssse3) };

const predict_param predict_sizes_ssse3[] = { ALL_CFL_TX_SIZES(
    get_predict_lbd_fn_ssse3) };

const predict_param_hbd predict_sizes_hbd_ssse3[] = { ALL_CFL_TX_SIZES(
    get_predict_hbd_fn_ssse3) };

INSTANTIATE_TEST_CASE_P(SSSE3, CFLSubsampleTest,
                        ::testing::ValuesIn(subsample_sizes_ssse3));

INSTANTIATE_TEST_CASE_P(SSSE3, CFLPredictTest,
                        ::testing::ValuesIn(predict_sizes_ssse3));

INSTANTIATE_TEST_CASE_P(SSSE3, CFLPredictHBDTest,
                        ::testing::ValuesIn(predict_sizes_hbd_ssse3));
#endif

#if HAVE_AVX2
const sub_avg_param sub_avg_sizes_avx2[] = { ALL_CFL_TX_SIZES(
    get_subtract_average_fn_avx2) };

const subsample_param subsample_sizes_avx2[] = { ALL_CFL_TX_SIZES(
    cfl_get_luma_subsampling_420_lbd_avx2) };

const predict_param predict_sizes_avx2[] = { ALL_CFL_TX_SIZES(
    get_predict_lbd_fn_avx2) };

const predict_param_hbd predict_sizes_hbd_avx2[] = { ALL_CFL_TX_SIZES(
    get_predict_hbd_fn_avx2) };

INSTANTIATE_TEST_CASE_P(AVX2, CFLSubAvgTest,
                        ::testing::ValuesIn(sub_avg_sizes_avx2));

INSTANTIATE_TEST_CASE_P(AVX2, CFLSubsampleTest,
                        ::testing::ValuesIn(subsample_sizes_avx2));

INSTANTIATE_TEST_CASE_P(AVX2, CFLPredictTest,
                        ::testing::ValuesIn(predict_sizes_avx2));

INSTANTIATE_TEST_CASE_P(AVX2, CFLPredictHBDTest,
                        ::testing::ValuesIn(predict_sizes_hbd_avx2));
#endif

#if HAVE_NEON
const sub_avg_param sub_avg_sizes_neon[] = { ALL_CFL_TX_SIZES(
    get_subtract_average_fn_neon) };

const subsample_param subsample_sizes_neon[] = { ALL_CFL_TX_SIZES(
    cfl_get_luma_subsampling_420_lbd_neon) };

INSTANTIATE_TEST_CASE_P(NEON, CFLSubAvgTest,
                        ::testing::ValuesIn(sub_avg_sizes_neon));

INSTANTIATE_TEST_CASE_P(NEON, CFLSubsampleTest,
                        ::testing::ValuesIn(subsample_sizes_neon));

#endif
}  // namespace

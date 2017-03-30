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

#ifndef TEST_WARP_FILTER_TEST_UTIL_H_
#define TEST_WARP_FILTER_TEST_UTIL_H_

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"

#include "av1/common/mv.h"

namespace libaom_test {

namespace AV1WarpFilter {

typedef void (*warp_affine_func)(int32_t *mat, uint8_t *ref, int width,
                                 int height, int stride, uint8_t *pred,
                                 int p_col, int p_row, int p_width,
                                 int p_height, int p_stride, int subsampling_x,
                                 int subsampling_y, int ref_frm, int32_t alpha,
                                 int32_t beta, int32_t gamma, int32_t delta);

typedef std::tr1::tuple<int, int, int> WarpTestParam;

::testing::internal::ParamGenerator<WarpTestParam> GetDefaultParams();

class AV1WarpFilterTest : public ::testing::TestWithParam<WarpTestParam> {
 public:
  virtual ~AV1WarpFilterTest();
  virtual void SetUp();

  virtual void TearDown();

 protected:
  int32_t random_param(int bits);
  void generate_model(int32_t *mat, int32_t *alpha, int32_t *beta,
                      int32_t *gamma, int32_t *delta);

  void RunCheckOutput(warp_affine_func test_impl);

  libaom_test::ACMRandom rnd_;
};

}  // namespace AV1WarpFilter

}  // namespace libaom_test

#endif  // TEST_WARP_FILTER_TEST_UTIL_H_

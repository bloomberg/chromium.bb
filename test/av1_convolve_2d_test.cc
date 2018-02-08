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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/av1_convolve_2d_test_util.h"

using libaom_test::ACMRandom;
using libaom_test::AV1Convolve2D::AV1Convolve2DSrTest;
using libaom_test::AV1Convolve2D::AV1Convolve2DTest;
using std::tr1::make_tuple;
using std::tr1::tuple;
#if CONFIG_JNT_COMP
using libaom_test::AV1Convolve2D::AV1JntConvolve2DTest;
#endif
using libaom_test::AV1HighbdConvolve2D::AV1HighbdConvolve2DTest;
#if CONFIG_JNT_COMP
using libaom_test::AV1HighbdConvolve2D::AV1HighbdJntConvolve2DTest;
#endif

namespace {

TEST_P(AV1Convolve2DTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

TEST_P(AV1Convolve2DTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

INSTANTIATE_TEST_CASE_P(
    C_COPY, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_2d_copy_c, 0, 0, 1));

INSTANTIATE_TEST_CASE_P(SSE2_COPY, AV1Convolve2DTest,
                        libaom_test::AV1Convolve2D::BuildParams(
                            av1_convolve_2d_copy_sse2, 0, 0, 1));

INSTANTIATE_TEST_CASE_P(
    C_X, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_x_c, 1, 0, 1));

INSTANTIATE_TEST_CASE_P(
    SSE2_X, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_x_sse2, 1, 0, 1));

INSTANTIATE_TEST_CASE_P(
    C_Y, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_y_c, 0, 1, 1));

INSTANTIATE_TEST_CASE_P(
    SSE2_Y, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_y_sse2, 0, 1, 1));

INSTANTIATE_TEST_CASE_P(
    SSE2, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_2d_sse2, 1, 1, 1));

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2_X, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_x_avx2, 1, 0, 1));

INSTANTIATE_TEST_CASE_P(
    AVX2_Y, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_y_avx2, 0, 1, 1));

INSTANTIATE_TEST_CASE_P(
    AVX2, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_2d_avx2, 1, 1, 1));
#endif

TEST_P(AV1Convolve2DSrTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

TEST_P(AV1Convolve2DSrTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

INSTANTIATE_TEST_CASE_P(SSE2_COPY, AV1Convolve2DSrTest,
                        libaom_test::AV1Convolve2D::BuildParams(
                            av1_convolve_2d_copy_sr_sse2, 0, 0, 1));

INSTANTIATE_TEST_CASE_P(
    C_X, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_x_sr_c, 1, 0, 0));

INSTANTIATE_TEST_CASE_P(
    SSE2_X, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_x_sr_sse2, 1, 0, 0));

INSTANTIATE_TEST_CASE_P(
    C_Y, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_y_sr_c, 0, 1, 0));

INSTANTIATE_TEST_CASE_P(
    SSE2_Y, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_y_sr_sse2, 0, 1, 0));

INSTANTIATE_TEST_CASE_P(
    SSE2, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_2d_sr_sse2, 1, 1, 0));

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2_X, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_x_sr_avx2, 1, 0, 0));

INSTANTIATE_TEST_CASE_P(
    AVX2_Y, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_y_sr_avx2, 0, 1, 0));

INSTANTIATE_TEST_CASE_P(
    AVX2, AV1Convolve2DSrTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_2d_sr_avx2, 1, 1, 0));
#endif

#if CONFIG_JNT_COMP && HAVE_SSE4_1
TEST_P(AV1JntConvolve2DTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

INSTANTIATE_TEST_CASE_P(C_COPY, AV1JntConvolve2DTest,
                        libaom_test::AV1Convolve2D::BuildParams(
                            av1_jnt_convolve_2d_copy_c, 0, 0, 1));

INSTANTIATE_TEST_CASE_P(SSE2_COPY, AV1JntConvolve2DTest,
                        libaom_test::AV1Convolve2D::BuildParams(
                            av1_jnt_convolve_2d_copy_sse2, 0, 0, 1));

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1JntConvolve2DTest,
                        libaom_test::AV1Convolve2D::BuildParams(
                            av1_jnt_convolve_2d_sse4_1, 1, 1, 1));
#endif

#if HAVE_SSSE3
TEST_P(AV1HighbdConvolve2DTest, CheckOutput) { RunCheckOutput(GET_PARAM(1)); }

INSTANTIATE_TEST_CASE_P(SSSE3, AV1HighbdConvolve2DTest,
                        libaom_test::AV1HighbdConvolve2D::BuildParams(
                            av1_highbd_convolve_2d_ssse3));
#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2, AV1HighbdConvolve2DTest,
    libaom_test::AV1HighbdConvolve2D::BuildParams(av1_highbd_convolve_2d_avx2));
#endif

#if CONFIG_JNT_COMP && HAVE_SSE4_1
TEST_P(AV1HighbdJntConvolve2DTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

INSTANTIATE_TEST_CASE_P(SSE4_1, AV1HighbdJntConvolve2DTest,
                        libaom_test::AV1HighbdConvolve2D::BuildParams(
                            av1_highbd_jnt_convolve_2d_sse4_1));
#endif  // CONFIG_JNT_COMP
#endif

}  // namespace

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

#include <math.h>
#include <stdlib.h>
#include <new>

#include "./aom_config.h"
#include "aom_dsp/ssim.h"
#include "aom_ports/mem.h"
#include "aom_ports/msvc.h"
#include "aom_scale/yv12config.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

using libaom_test::ACMRandom;

namespace {

typedef double (*LBDMetricFunc)(const YV12_BUFFER_CONFIG *source,
                                const YV12_BUFFER_CONFIG *dest, double *weight);
typedef double (*HBDMetricFunc)(const YV12_BUFFER_CONFIG *source,
                                const YV12_BUFFER_CONFIG *dest, double *weight,
                                unsigned int bd);

class HBDMetricsTestBase {
 public:
  virtual ~HBDMetricsTestBase() {}

 protected:
  void RunAccuracyCheck() {
    const int width = 1920;
    const int height = 1080;
    int i = 0;
    const uint8_t kPixFiller = 128;
    YV12_BUFFER_CONFIG lbd_src, lbd_dst;
    YV12_BUFFER_CONFIG hbd_src, hbd_dst;
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    double lbd_score, hbd_score, lbd_db, hbd_db, lbd_w, hbd_w;

    memset(&lbd_src, 0, sizeof(lbd_src));
    memset(&lbd_dst, 0, sizeof(lbd_dst));
    memset(&hbd_src, 0, sizeof(hbd_src));
    memset(&hbd_dst, 0, sizeof(hbd_dst));

    aom_alloc_frame_buffer(&lbd_src, width, height, 1, 1, 0, 32, 16);
    aom_alloc_frame_buffer(&lbd_dst, width, height, 1, 1, 0, 32, 16);
    aom_alloc_frame_buffer(&hbd_src, width, height, 1, 1, 1, 32, 16);
    aom_alloc_frame_buffer(&hbd_dst, width, height, 1, 1, 1, 32, 16);

    memset(lbd_src.buffer_alloc, kPixFiller, lbd_src.buffer_alloc_sz);
    while (i < lbd_src.buffer_alloc_sz) {
      uint16_t spel, dpel;
      spel = lbd_src.buffer_alloc[i];
      // Create some distortion for dst buffer.
      lbd_dst.buffer_alloc[i] = rnd.Rand8();
      dpel = lbd_dst.buffer_alloc[i];
      ((uint16_t *)(hbd_src.buffer_alloc))[i] = spel << (bit_depth_ - 8);
      ((uint16_t *)(hbd_dst.buffer_alloc))[i] = dpel << (bit_depth_ - 8);
      i++;
    }

    lbd_score = lbd_metric_(&lbd_src, &lbd_dst, &lbd_w);
    hbd_score = hbd_metric_(&hbd_src, &hbd_dst, &hbd_w, bit_depth_);

    lbd_db = 100 * pow(lbd_score / lbd_w, 8.0);
    hbd_db = 100 * pow(hbd_score / hbd_w, 8.0);

    aom_free_frame_buffer(&lbd_src);
    aom_free_frame_buffer(&lbd_dst);
    aom_free_frame_buffer(&hbd_src);
    aom_free_frame_buffer(&hbd_dst);

    EXPECT_LE(fabs(lbd_db - hbd_db), threshold_);
  }

  int bit_depth_;
  double threshold_;
  LBDMetricFunc lbd_metric_;
  HBDMetricFunc hbd_metric_;
};

typedef std::tr1::tuple<LBDMetricFunc, HBDMetricFunc, int, double>
    MetricTestTParam;
class HBDMetricsTest : public HBDMetricsTestBase,
                       public ::testing::TestWithParam<MetricTestTParam> {
 public:
  virtual void SetUp() {
    lbd_metric_ = GET_PARAM(0);
    hbd_metric_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    threshold_ = GET_PARAM(3);
  }
  virtual void TearDown() {}
};

TEST_P(HBDMetricsTest, RunAccuracyCheck) { RunAccuracyCheck(); }

// Allow small variation due to floating point operations.
static const double kSsim_thresh = 0.001;

INSTANTIATE_TEST_CASE_P(
    C, HBDMetricsTest,
    ::testing::Values(MetricTestTParam(&aom_calc_ssim, &aom_highbd_calc_ssim,
                                       10, kSsim_thresh),
                      MetricTestTParam(&aom_calc_ssim, &aom_highbd_calc_ssim,
                                       12, kSsim_thresh)));
}  // namespace

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
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
namespace {

class ForwardKeyTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  ForwardKeyTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        kf_max_dist_(GET_PARAM(2)) {}
  virtual ~ForwardKeyTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cpu_used_ = 2;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.rc_target_bitrate = 200;
    cfg_.g_lag_in_frames = 10;
    cfg_.fwd_kf_enabled = 1;
    cfg_.g_threads = 0;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
    }
  }

  ::libaom_test::TestMode encoding_mode_;
  int kf_max_dist_;
  int cpu_used_;
};

TEST_P(ForwardKeyTest, ForwardKeyEncodeTest) {
  const int n_frames = 20;
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, n_frames);
  cfg_.kf_max_dist = kf_max_dist_;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // TODO(sarahparker) Add functionality to assert the minimum number of
  // keyframes were placed.
}

AV1_INSTANTIATE_TEST_CASE(ForwardKeyTest,
                          ::testing::Values(::libaom_test::kTwoPassGood),
                          ::testing::Values(4, 6, 8, 12, 16, 18));
}  // namespace

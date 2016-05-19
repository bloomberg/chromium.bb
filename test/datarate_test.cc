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

#include "./aom_config.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom/aom_codec.h"

namespace {

class DatarateTestLarge
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWithParam<libaom_test::TestMode> {
 public:
  DatarateTestLarge() : EncoderTest(GET_PARAM(0)) {}

  virtual ~DatarateTestLarge() {}

 protected:
  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    first_drop_ = 0;
    bits_total_ = 0;
    duration_ = 0.0;
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0)
      encoder->Control(AOME_SET_NOISE_SENSITIVITY, denoiser_on_);

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
      encoder->Control(AOME_SET_NOISE_SENSITIVITY, denoiser_on_);
    }

    const aom_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    aom_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    // TODO(jimbankoski): Remove these lines when the issue:
    // http://code.google.com/p/webm/issues/detail?id=496 is fixed.
    // For now the codec assumes buffer starts at starting buffer rate
    // plus one frame's time.
    if (last_pts_ == 0) duration = 1;

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    /* Test the buffer model here before subtracting the frame. Do so because
     * the way the leaky bucket model works in libaom is to allow the buffer to
     * empty - and then stop showing frames until we've got enough bits to
     * show one. As noted in comment below (issue 495), this does not currently
     * apply to key frames. For now exclude key frames in condition below. */
    const bool key_frame =
        (pkt->data.frame.flags & AOM_FRAME_IS_KEY) ? true : false;
    if (!key_frame) {
      ASSERT_GE(bits_in_buffer_model_, 0) << "Buffer Underrun at frame "
                                          << pkt->data.frame.pts;
    }

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Subtract from the buffer the bits associated with a played back frame.
    bits_in_buffer_model_ -= frame_size_in_bits;

    // Update the running total of bits for end of test datarate checks.
    bits_total_ += frame_size_in_bits;

    // If first drop not set and we have a drop set it to this time.
    if (!first_drop_ && duration > 1) first_drop_ = last_pts_ + 1;

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;

    // We update this so that we can calculate the datarate minus the last
    // frame encoded in the file.
    bits_in_last_frame_ = frame_size_in_bits;

    ++frame_number_;
  }

  virtual void EndPassHook(void) {
    if (bits_total_) {
      const double file_size_in_kb = bits_total_ / 1000.;  // bits per kilobit

      duration_ = (last_pts_ + 1) * timebase_;

      // Effective file datarate includes the time spent prebuffering.
      effective_datarate_ = (bits_total_ - bits_in_last_frame_) / 1000.0 /
                            (cfg_.rc_buf_initial_sz / 1000.0 + duration_);

      file_datarate_ = file_size_in_kb / duration_;
    }
  }

  aom_codec_pts_t last_pts_;
  int64_t bits_in_buffer_model_;
  double timebase_;
  int frame_number_;
  aom_codec_pts_t first_drop_;
  int64_t bits_total_;
  double duration_;
  double file_datarate_;
  double effective_datarate_;
  size_t bits_in_last_frame_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
};

#if CONFIG_TEMPORAL_DENOISING
// Check basic datarate targeting, for a single bitrate, but loop over the
// various denoiser settings.
TEST_P(DatarateTestLarge, DenoiserLevels) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = AOM_CBR;
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int j = 1; j < 5; ++j) {
    // Run over the denoiser levels.
    // For the temporal denoiser (#if CONFIG_TEMPORAL_DENOISING) the level j
    // refers to the 4 denoiser modes: denoiserYonly, denoiserOnYUV,
    // denoiserOnAggressive, and denoiserOnAdaptive.
    // For the spatial denoiser (if !CONFIG_TEMPORAL_DENOISING), the level j
    // refers to the blur thresholds: 20, 40, 60 80.
    // The j = 0 case (denoiser off) is covered in the tests below.
    denoiser_on_ = j;
    cfg_.rc_target_bitrate = 300;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";

    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.3)
        << " The datarate for the file missed the target!";
  }
}

// Check basic datarate targeting, for a single bitrate, when denoiser is off
// and on.
TEST_P(DatarateTestLarge, DenoiserOffOn) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = AOM_CBR;
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 299);
  cfg_.rc_target_bitrate = 300;
  ResetModel();
  // The denoiser is off by default.
  denoiser_on_ = 0;
  // Set the offon test flag.
  denoiser_offon_test_ = 1;
  denoiser_offon_period_ = 100;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
      << " The datarate for the file exceeds the target!";
  ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.3)
      << " The datarate for the file missed the target!";
}
#endif  // CONFIG_TEMPORAL_DENOISING

TEST_P(DatarateTestLarge, BasicBufferModel) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_max_quantizer = 56;
  cfg_.rc_end_usage = AOM_CBR;
  // 2 pass cbr datarate control has a bug hidden by the small # of
  // frames selected in this encode. The problem is that even if the buffer is
  // negative we produce a keyframe on a cutscene. Ignoring datarate
  // constraints
  // TODO(jimbankoski): ( Fix when issue
  // http://code.google.com/p/webm/issues/detail?id=495 is addressed. )
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  // There is an issue for low bitrates in real-time mode, where the
  // effective_datarate slightly overshoots the target bitrate.
  // This is same the issue as noted about (#495).
  // TODO(jimbankoski/marpan): Update test to run for lower bitrates (< 100),
  // when the issue is resolved.
  for (int i = 100; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(cfg_.rc_target_bitrate, effective_datarate_ * 0.95)
        << " The datarate for the file exceeds the target!";

    ASSERT_LE(cfg_.rc_target_bitrate, file_datarate_ * 1.3)
        << " The datarate for the file missed the target!";
  }
}

TEST_P(DatarateTestLarge, ChangingDropFrameThresh) {
  denoiser_on_ = 0;
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_max_quantizer = 36;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.kf_mode = AOM_KF_DISABLED;

  const int frame_count = 40;
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, frame_count);

  // Here we check that the first dropped frame gets earlier and earlier
  // as the drop frame threshold is increased.

  const int kDropFrameThreshTestStep = 30;
  aom_codec_pts_t last_drop = frame_count;
  for (int i = 1; i < 91; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
  }
}

class DatarateTestAV1Large
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int> {
 public:
  DatarateTestAV1Large() : EncoderTest(GET_PARAM(0)) {}

 protected:
  virtual ~DatarateTestAV1Large() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    tot_frame_number_ = 0;
    first_drop_ = 0;
    num_drops_ = 0;
    // Denoiser is off by default.
    denoiser_on_ = 0;
    // For testing up to 3 layers.
    for (int i = 0; i < 3; ++i) {
      bits_total_[i] = 0;
    }
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
  }

  int SetFrameFlags(int frame_num, int num_temp_layers) {
    int frame_flags = 0;
    return frame_flags;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
    }

    encoder->Control(AV1E_SET_NOISE_SENSITIVITY, denoiser_on_);

    const aom_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    aom_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    if (duration > 1) {
      // If first drop not set and we have a drop set it to this time.
      if (!first_drop_) first_drop_ = last_pts_ + 1;
      // Update the number of frame drops.
      num_drops_ += static_cast<int>(duration - 1);
      // Update counter for total number of frames (#frames input to encoder).
      tot_frame_number_ += static_cast<int>(duration - 1);
    }

    int layer = 0;

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    // Buffer should not go negative.
    ASSERT_GE(bits_in_buffer_model_, 0) << "Buffer Underrun at frame "
                                        << pkt->data.frame.pts;

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Update the total encoded bits. For temporal layers, update the cumulative
    // encoded bits per layer.
    for (int i = layer; i < static_cast<int>(cfg_.ts_number_layers); ++i) {
      bits_total_[i] += frame_size_in_bits;
    }

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++frame_number_;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) {
    for (int layer = 0; layer < static_cast<int>(cfg_.ts_number_layers);
         ++layer) {
      duration_ = (last_pts_ + 1) * timebase_;
      if (bits_total_[layer]) {
        // Effective file datarate:
        effective_datarate_[layer] = (bits_total_[layer] / 1000.0) / duration_;
      }
    }
  }

  aom_codec_pts_t last_pts_;
  double timebase_;
  int frame_number_;      // Counter for number of non-dropped/encoded frames.
  int tot_frame_number_;  // Counter for total number of input frames.
  int64_t bits_total_[3];
  double duration_;
  double effective_datarate_[3];
  int set_cpu_used_;
  int64_t bits_in_buffer_model_;
  aom_codec_pts_t first_drop_;
  int num_drops_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
};

// Check basic rate targeting,
TEST_P(DatarateTestAV1Large, BasicRateTargeting) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int i = 150; i < 800; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting,
TEST_P(DatarateTestAV1Large, BasicRateTargeting444) {
  ::libaom_test::Y4mVideoSource video("rush_hour_444.y4m", 0, 140);

  cfg_.g_profile = 1;
  cfg_.g_timebase = video.timebase();

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = AOM_CBR;

  for (int i = 250; i < 900; i += 200) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_[0] * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_[0] * 1.15)
        << " The datarate for the file missed the target!"
        << cfg_.rc_target_bitrate << " " << effective_datarate_;
  }
}

// Check that (1) the first dropped frame gets earlier and earlier
// as the drop frame threshold is increased, and (2) that the total number of
// frame drops does not decrease as we increase frame drop threshold.
// Use a lower qp-max to force some frame drops.
TEST_P(DatarateTestAV1Large, ChangingDropFrameThresh) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 50;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.g_lag_in_frames = 0;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);

  const int kDropFrameThreshTestStep = 30;
  aom_codec_pts_t last_drop = 140;
  int last_num_drops = 0;
  for (int i = 10; i < 100; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_[0], cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_[0], cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    ASSERT_GE(num_drops_, last_num_drops * 0.85)
        << " The number of dropped frames for drop_thresh " << i
        << " < number of dropped frames for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
    last_num_drops = num_drops_;
  }
}

AV1_INSTANTIATE_TEST_CASE(DatarateTestAV1Large,
                          ::testing::Values(::libaom_test::kOnePassGood,
                                            ::libaom_test::kRealTime),
                          ::testing::Range(2, 7));

}  // namespace

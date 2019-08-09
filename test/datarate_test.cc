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

#include "config/aom_config.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom/aom_codec.h"

namespace {

// Params: test mode, speed, aq mode and index for bitrate array.
class DatarateTestLarge
    : public ::libaom_test::CodecTestWith4Params<libaom_test::TestMode, int,
                                                 unsigned int, int>,
      public ::libaom_test::EncoderTest {
 public:
  DatarateTestLarge()
      : EncoderTest(GET_PARAM(0)), set_cpu_used_(GET_PARAM(2)),
        aq_mode_(GET_PARAM(3)) {}

 protected:
  virtual ~DatarateTestLarge() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
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
    bits_total_ = 0;
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
    number_temporal_layers_ = 1;
    for (int i = 0; i < 3; i++) {
      target_layer_bitrate_[i] = 0;
      effective_datarate_tl[i] = 0.0;
    }
    memset(&layer_id_, 0, sizeof(aom_svc_layer_id_t));
    memset(&svc_params_, 0, sizeof(aom_svc_params_t));
    memset(&ref_frame_config_, 0, sizeof(aom_svc_ref_frame_config_t));
  }

  // Layer pattern configuration.
  int set_layer_pattern(int frame_cnt, aom_svc_layer_id_t *layer_id,
                        aom_svc_ref_frame_config_t *ref_frame_config) {
    // No spatial layers in this test.
    layer_id->spatial_layer_id = 0;
    // Set the referende map buffer idx for the 7 references:
    // LAST_FRAME (0), LAST2_FRAME(1), LAST3_FRAME(2), GOLDEN_FRAME(3),
    // BWDREF_FRAME(4), ALTREF2_FRAME(5), ALTREF_FRAME(6).
    for (int i = 0; i < 7; i++) ref_frame_config->ref_idx[i] = i;
    for (int i = 0; i < 8; i++) ref_frame_config->refresh[i] = 0;
    // Note only use LAST and GF for prediction in non-rd mode (speed 8).
    int layer_flags = AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                      AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
                      AOM_EFLAG_NO_REF_ARF2;
    // 3-layer:
    //   1    3   5    7
    //     2        6
    // 0        4        8
    if (frame_cnt % 4 == 0) {
      // Base layer.
      layer_id->temporal_layer_id = 0;
      // Update LAST on layer 0, reference LAST and GF.
      ref_frame_config->refresh[0] = 1;
    } else if ((frame_cnt - 1) % 4 == 0) {
      layer_id->temporal_layer_id = 2;
      // First top layer: no updates, only reference LAST (TL0).
      layer_flags |= AOM_EFLAG_NO_REF_GF;
    } else if ((frame_cnt - 2) % 4 == 0) {
      layer_id->temporal_layer_id = 1;
      // Middle layer (TL1): update LAST2, only reference LAST (TL0).
      ref_frame_config->refresh[1] = 1;
      layer_flags |= AOM_EFLAG_NO_REF_GF;
    } else if ((frame_cnt - 3) % 4 == 0) {
      layer_id->temporal_layer_id = 2;
      // Second top layer: no updates, only reference LAST.
      // Set buffer idx for LAST to slot 1, since that was the slot
      // updated in previous frame. So LAST is TL1 frame.
      ref_frame_config->ref_idx[0] = 1;
      ref_frame_config->ref_idx[1] = 0;
      layer_flags |= AOM_EFLAG_NO_REF_GF;
    }
    return layer_flags;
  }

  void initialize_svc(int number_temporal_layers, aom_svc_params *svc_params) {
    svc_params->number_spatial_layers = 1;
    svc_params->scaling_factor_num[0] = 1;
    svc_params->scaling_factor_den[0] = 1;
    svc_params->number_temporal_layers = number_temporal_layers;
    for (int i = 0; i < number_temporal_layers; ++i) {
      svc_params->max_quantizers[i] = 56;
      svc_params->min_quantizers[i] = 2;
      svc_params->layer_target_bitrate[i] = target_layer_bitrate_[i];
    }
    svc_params->framerate_factor[0] = 1;
    if (number_temporal_layers == 2) {
      svc_params->framerate_factor[0] = 2;
      svc_params->framerate_factor[1] = 1;
    } else if (number_temporal_layers == 3) {
      svc_params->framerate_factor[0] = 4;
      svc_params->framerate_factor[1] = 2;
      svc_params->framerate_factor[2] = 1;
    }
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      if (number_temporal_layers_ > 1) {
        initialize_svc(number_temporal_layers_, &svc_params_);
        encoder->Control(AV1E_SET_SVC_PARAMS, &svc_params_);
      }
    }

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
    }

    encoder->Control(AV1E_SET_NOISE_SENSITIVITY, denoiser_on_);

    if (number_temporal_layers_ > 1) {
      // Set the reference/update flags, layer_id, and reference_map
      // buffer index.
      frame_flags_ =
          set_layer_pattern(video->frame(), &layer_id_, &ref_frame_config_);
      encoder->Control(AV1E_SET_SVC_LAYER_ID, &layer_id_);
      encoder->Control(AV1E_SET_SVC_REF_FRAME_CONFIG, &ref_frame_config_);
    }

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
      // Needed for setting the proper layer_id below.
      tot_frame_number_ += static_cast<int>(duration - 1);
    }

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    // Buffer should not go negative.
    ASSERT_GE(bits_in_buffer_model_, 0)
        << "Buffer Underrun at frame " << pkt->data.frame.pts;

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Update the total encoded bits.
    bits_total_ += frame_size_in_bits;

    if (number_temporal_layers_ > 1) {
      // Update the layer cumulative  bitrate.
      for (int i = layer_id_.temporal_layer_id; i < number_temporal_layers_;
           i++)
        effective_datarate_tl[i] += 1.0 * frame_size_in_bits;
    }
    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++frame_number_;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) {
    duration_ = (last_pts_ + 1) * timebase_;
    // Effective file datarate:
    effective_datarate_ = (bits_total_ / 1000.0) / duration_;
    if (number_temporal_layers_ > 1) {
      for (int i = 0; i < number_temporal_layers_; i++)
        effective_datarate_tl[i] =
            (effective_datarate_tl[i] / 1000) / duration_;
    }
  }

  virtual void BasicRateTargetingVBRTest() {
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.g_error_resilient = 0;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 140);
    const int bitrate_array[2] = { 400, 800 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.7)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_, cfg_.rc_target_bitrate * 1.3)
        << " The datarate for the file is greater than target by too much!";
  }

  virtual void BasicRateTargetingCBRTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 1;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 140);
    const int bitrate_array[2] = { 150, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_, cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
  }

  virtual void BasicRateTargeting444CBRTest() {
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

    const int bitrate_array[2] = { 250, 650 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_ * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_ * 1.15)
        << " The datarate for the file missed the target!"
        << cfg_.rc_target_bitrate << " " << effective_datarate_;
  }

  virtual void ChangingDropFrameThreshTest() {
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
    cfg_.g_error_resilient = 1;
    // TODO(marpan): Investigate datarate target failures with a smaller
    // keyframe interval (128).
    cfg_.kf_max_dist = 9999;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 100);

    const int kDropFrameThreshTestStep = 30;
    aom_codec_pts_t last_drop = 140;
    int last_num_drops = 0;
    for (int i = 40; i < 100; i += kDropFrameThreshTestStep) {
      cfg_.rc_dropframe_thresh = i;
      ResetModel();
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.85)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_, cfg_.rc_target_bitrate * 1.15)
          << " The datarate for the file is greater than target by too much!";
      if (last_drop > 0) {
        ASSERT_LE(first_drop_, last_drop)
            << " The first dropped frame for drop_thresh " << i
            << " > first dropped frame for drop_thresh "
            << i - kDropFrameThreshTestStep;
      }
      ASSERT_GE(num_drops_, last_num_drops * 0.85)
          << " The number of dropped frames for drop_thresh " << i
          << " < number of dropped frames for drop_thresh "
          << i - kDropFrameThreshTestStep;
      last_drop = first_drop_;
      last_num_drops = num_drops_;
    }
  }

  virtual void BasicRateTargetingCBR3TLTest() {
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 0;
    cfg_.g_usage = AOM_USAGE_REALTIME;
    cfg_.g_error_resilient = 1;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 300);
    const int bitrate_array[2] = { 150, 550 };
    cfg_.rc_target_bitrate = bitrate_array[GET_PARAM(4)];
    ResetModel();
    number_temporal_layers_ = 3;
    target_layer_bitrate_[0] = 50 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[1] = 70 * cfg_.rc_target_bitrate / 100;
    target_layer_bitrate_[2] = cfg_.rc_target_bitrate;
    framerate_factor_[0] = 4;
    framerate_factor_[1] = 2;
    framerate_factor_[2] = 1;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    for (int i = 0; i < number_temporal_layers_; i++) {
      ASSERT_GE(effective_datarate_tl[i], target_layer_bitrate_[i] * 0.80)
          << " The datarate for the file is lower than target by too much!";
      ASSERT_LE(effective_datarate_tl[i], target_layer_bitrate_[i] * 1.30)
          << " The datarate for the file is greater than target by too much!";
    }
  }

  aom_codec_pts_t last_pts_;
  double timebase_;
  int frame_number_;      // Counter for number of non-dropped/encoded frames.
  int tot_frame_number_;  // Counter for total number of input frames.
  int64_t bits_total_;
  double duration_;
  double effective_datarate_;
  int set_cpu_used_;
  int64_t bits_in_buffer_model_;
  aom_codec_pts_t first_drop_;
  int num_drops_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
  unsigned int aq_mode_;
  int number_temporal_layers_;
  // Allow for up to 3 temporal layers.
  int target_layer_bitrate_[3];
  aom_svc_params_t svc_params_;
  aom_svc_ref_frame_config_t ref_frame_config_;
  aom_svc_layer_id_t layer_id_;
  double effective_datarate_tl[3];
  int framerate_factor_[3];
};  // namespace

// Check basic rate targeting for VBR mode.
TEST_P(DatarateTestLarge, BasicRateTargetingVBR) {
  BasicRateTargetingVBRTest();
}

// Check basic rate targeting for CBR,
TEST_P(DatarateTestLarge, BasicRateTargetingCBR) {
  BasicRateTargetingCBRTest();
}

// Check basic rate targeting for CBR.
TEST_P(DatarateTestLarge, BasicRateTargeting444CBR) {
  BasicRateTargeting444CBRTest();
}

// Check that (1) the first dropped frame gets earlier and earlier
// as the drop frame threshold is increased, and (2) that the total number of
// frame drops does not decrease as we increase frame drop threshold.
// Use a lower qp-max to force some frame drops.
TEST_P(DatarateTestLarge, ChangingDropFrameThresh) {
  ChangingDropFrameThreshTest();
}

class DatarateTestRealtime : public DatarateTestLarge {};

// Check basic rate targeting for VBR mode.
TEST_P(DatarateTestRealtime, BasicRateTargetingVBR) {
  BasicRateTargetingVBRTest();
}

// Check basic rate targeting for CBR,
TEST_P(DatarateTestRealtime, BasicRateTargetingCBR) {
  BasicRateTargetingCBRTest();
}

// Check basic rate targeting for CBR.
TEST_P(DatarateTestRealtime, BasicRateTargeting444CBR) {
  BasicRateTargeting444CBRTest();
}

// Check that (1) the first dropped frame gets earlier and earlier
// as the drop frame threshold is increased, and (2) that the total number of
// frame drops does not decrease as we increase frame drop threshold.
// Use a lower qp-max to force some frame drops.
TEST_P(DatarateTestRealtime, ChangingDropFrameThresh) {
  ChangingDropFrameThreshTest();
}

// Check basic rate targeting for CBR, for 3 temporal layers.
TEST_P(DatarateTestRealtime, BasicRateTargetingCBR3TL) {
  BasicRateTargetingCBR3TLTest();
}

AV1_INSTANTIATE_TEST_CASE(DatarateTestLarge,
                          ::testing::Values(::libaom_test::kOnePassGood,
                                            ::libaom_test::kRealTime),
                          ::testing::Range(2, 7),
                          ::testing::Range<unsigned int>(0, 4),
                          ::testing::Values(0, 1));

AV1_INSTANTIATE_TEST_CASE(DatarateTestRealtime,
                          ::testing::Values(::libaom_test::kRealTime),
                          ::testing::Range(7, 9),
                          ::testing::Range<unsigned int>(0, 4),
                          ::testing::Values(0, 1));

}  // namespace

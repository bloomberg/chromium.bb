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

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const int kMaxErrorFrames = 12;
const int kMaxDroppableFrames = 12;

class ErrorResilienceTestLarge
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, bool> {
 protected:
  ErrorResilienceTestLarge()
      : EncoderTest(GET_PARAM(0)), psnr_(0.0), nframes_(0), mismatch_psnr_(0.0),
        mismatch_nframes_(0), encoding_mode_(GET_PARAM(1)) {
    Reset();
  }

  virtual ~ErrorResilienceTestLarge() {}

  void Reset() {
    error_nframes_ = 0;
    droppable_nframes_ = 0;
    pattern_switch_ = 0;
  }

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    psnr_ = 0.0;
    nframes_ = 0;
    mismatch_psnr_ = 0.0;
    mismatch_nframes_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  virtual void PreEncodeFrameHook(libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    frame_flags_ &=
        ~(AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF);
    // For temporal layer case.
    if (droppable_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < droppable_nframes_; ++i) {
        if (droppable_frames_[i] == video->frame()) {
          std::cout << "Encoding droppable frame: " << droppable_frames_[i]
                    << "\n";
          frame_flags_ |= (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                           AOM_EFLAG_NO_UPD_ARF);
          return;
        }
      }
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetAverageMismatchPsnr() const {
    if (mismatch_nframes_) return mismatch_psnr_ / mismatch_nframes_;
    return 0.0;
  }

  virtual bool DoDecode() const {
    if (error_nframes_ > 0 &&
        (cfg_.g_pass == AOM_RC_LAST_PASS || cfg_.g_pass == AOM_RC_ONE_PASS)) {
      for (unsigned int i = 0; i < error_nframes_; ++i) {
        if (error_frames_[i] == nframes_ - 1) {
          std::cout << "             Skipping decoding frame: "
                    << error_frames_[i] << "\n";
          return 0;
        }
      }
    }
    return 1;
  }

  virtual void MismatchHook(const aom_image_t *img1, const aom_image_t *img2) {
    double mismatch_psnr = compute_psnr(img1, img2);
    mismatch_psnr_ += mismatch_psnr;
    ++mismatch_nframes_;
    // std::cout << "Mismatch frame psnr: " << mismatch_psnr << "\n";
  }

  void SetErrorFrames(int num, unsigned int *list) {
    if (num > kMaxErrorFrames)
      num = kMaxErrorFrames;
    else if (num < 0)
      num = 0;
    error_nframes_ = num;
    for (unsigned int i = 0; i < error_nframes_; ++i)
      error_frames_[i] = list[i];
  }

  void SetDroppableFrames(int num, unsigned int *list) {
    if (num > kMaxDroppableFrames)
      num = kMaxDroppableFrames;
    else if (num < 0)
      num = 0;
    droppable_nframes_ = num;
    for (unsigned int i = 0; i < droppable_nframes_; ++i)
      droppable_frames_[i] = list[i];
  }

  unsigned int GetMismatchFrames() { return mismatch_nframes_; }

  void SetPatternSwitch(int frame_switch) { pattern_switch_ = frame_switch; }

 private:
  double psnr_;
  unsigned int nframes_;
  unsigned int error_nframes_;
  unsigned int droppable_nframes_;
  unsigned int pattern_switch_;
  double mismatch_psnr_;
  unsigned int mismatch_nframes_;
  unsigned int error_frames_[kMaxErrorFrames];
  unsigned int droppable_frames_[kMaxDroppableFrames];
  libaom_test::TestMode encoding_mode_;
};

TEST_P(ErrorResilienceTestLarge, OnVersusOff) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 2000;
  cfg_.g_lag_in_frames = 10;

  init_flags_ = AOM_CODEC_USE_PSNR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 30);

  // Error resilient mode OFF.
  cfg_.g_error_resilient = 0;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_resilience_off = GetAveragePsnr();
  EXPECT_GT(psnr_resilience_off, 25.0);

  // Error resilient mode ON.
  cfg_.g_error_resilient = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_resilience_on = GetAveragePsnr();
  EXPECT_GT(psnr_resilience_on, 25.0);

  // Test that turning on error resilient mode hurts by 10% at most.
  if (psnr_resilience_off > 0.0) {
    const double psnr_ratio = psnr_resilience_on / psnr_resilience_off;
    EXPECT_GE(psnr_ratio, 0.9);
    EXPECT_LE(psnr_ratio, 1.1);
  }
}

// Check for successful decoding and no encoder/decoder mismatch
// if we lose (i.e., drop before decoding) a set of droppable
// frames (i.e., frames that don't update any reference buffers).
// Check both isolated and consecutive loss.
TEST_P(ErrorResilienceTestLarge, DropFramesWithoutRecovery) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 500;
  // FIXME(debargha): Fix this to work for any lag.
  // Currently this test only works for lag = 0
  cfg_.g_lag_in_frames = 0;

  init_flags_ = AOM_CODEC_USE_PSNR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 40);

  // Error resilient mode ON.
  cfg_.g_error_resilient = 1;
  cfg_.kf_mode = AOM_KF_DISABLED;

  // Set an arbitrary set of error frames same as droppable frames.
  // In addition to isolated loss/drop, add a long consecutive series
  // (of size 9) of dropped frames.
  unsigned int num_droppable_frames = 11;
  unsigned int droppable_frame_list[] = { 5,  16, 22, 23, 24, 25,
                                          26, 27, 28, 29, 30 };
  SetDroppableFrames(num_droppable_frames, droppable_frame_list);
  SetErrorFrames(num_droppable_frames, droppable_frame_list);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  // Test that no mismatches have been found
  std::cout << "             Mismatch frames: " << GetMismatchFrames() << "\n";
  EXPECT_EQ(GetMismatchFrames(), (unsigned int)0);

  // Reset previously set of error/droppable frames.
  Reset();

#if 0
  // TODO(jkoleszar): This test is disabled for the time being as too
  // sensitive. It's not clear how to set a reasonable threshold for
  // this behavior.

  // Now set an arbitrary set of error frames that are non-droppable
  unsigned int num_error_frames = 3;
  unsigned int error_frame_list[] = {3, 10, 20};
  SetErrorFrames(num_error_frames, error_frame_list);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // Test that dropping an arbitrary set of inter frames does not hurt too much
  // Note the Average Mismatch PSNR is the average of the PSNR between
  // decoded frame and encoder's version of the same frame for all frames
  // with mismatch.
  const double psnr_resilience_mismatch = GetAverageMismatchPsnr();
  std::cout << "             Mismatch PSNR: "
            << psnr_resilience_mismatch << "\n";
  EXPECT_GT(psnr_resilience_mismatch, 20.0);
#endif
}

class ErrorResilienceTestLargeCodecControls
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWithParam<libaom_test::TestMode> {
 protected:
  ErrorResilienceTestLargeCodecControls()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)) {
    Reset();
  }

  virtual ~ErrorResilienceTestLargeCodecControls() {}

  void Reset() {
    last_pts_ = 0;
    tot_frame_number_ = 0;
    bits_total_ = 0;
    duration_ = 0.0;
  }

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
  }

  //
  // Frame flags and layer id for temporal layers.
  //

  // For two layers, test pattern is:
  //   1     3
  // 0    2     .....
  // For three layers, test pattern is:
  //   1      3    5      7
  //      2           6
  // 0          4            ....
  // LAST is always update on base/layer 0, GOLDEN is updated on layer 1,
  // and ALTREF is updated on top layer for 3 layer pattern.
  int SetFrameFlags(int frame_num, int num_temp_layers) {
    int frame_flags = 0;
    if (num_temp_layers == 2) {
      if (frame_num % 2 == 0) {
        // Layer 0: predict from L and ARF, update L.
        frame_flags =
            AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF;
      } else {
        // Layer 1: predict from L, G and ARF, and update G.
        frame_flags = AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_LAST |
                      AOM_EFLAG_NO_UPD_ENTROPY;
      }
    } else if (num_temp_layers == 3) {
      if (frame_num % 4 == 0) {
        // Layer 0: predict from L, update L.
        frame_flags = AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF |
                      AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF;
      } else if ((frame_num - 2) % 4 == 0) {
        // Layer 1: predict from L, G,  update G.
        frame_flags =
            AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_REF_ARF;
      } else if ((frame_num - 1) % 2 == 0) {
        // Layer 2: predict from L, G, ARF; update ARG.
        frame_flags = AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_LAST;
      }
    }
    return frame_flags;
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    aom_codec_pts_t duration = pkt->data.frame.pts - last_pts_;
    if (duration > 1) {
      // Update counter for total number of frames (#frames input to encoder).
      // Needed for setting the proper layer_id below.
      tot_frame_number_ += static_cast<int>(duration - 1);
    }
    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;
    // Update the total encoded bits. For temporal layers, update the cumulative
    // encoded bits per layer.
    bits_total_ += frame_size_in_bits;
    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) { duration_ = (last_pts_ + 1) * timebase_; }

 private:
  libaom_test::TestMode encoding_mode_;
  aom_codec_pts_t last_pts_;
  double timebase_;
  int64_t bits_total_;
  double duration_;
  int tot_frame_number_;
};

AV1_INSTANTIATE_TEST_CASE(ErrorResilienceTestLarge, ONE_PASS_TEST_MODES,
                          ::testing::Values(false));
}  // namespace

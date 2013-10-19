// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_vpx.h"

#include "media/base/video_frame.h"
#include "remoting/codec/codec_test.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class VideoDecoderVpxTest : public testing::Test {
 protected:
  scoped_ptr<VideoEncoderVpx> encoder_;
  scoped_ptr<VideoDecoderVpx> decoder_;

  VideoDecoderVpxTest() : encoder_(VideoEncoderVpx::CreateForVP8()),
                          decoder_(VideoDecoderVpx::CreateForVP8()) {
  }

  void TestGradient(int screen_width, int screen_height,
                    int view_width, int view_height,
                    double max_error_limit, double mean_error_limit) {
    TestVideoEncoderDecoderGradient(
        encoder_.get(), decoder_.get(),
        webrtc::DesktopSize(screen_width, screen_height),
        webrtc::DesktopSize(view_width, view_height),
        max_error_limit, mean_error_limit);
  }
};

TEST_F(VideoDecoderVpxTest, VideoEncodeAndDecode) {
  TestVideoEncoderDecoder(encoder_.get(), decoder_.get(), false);
}

// Check that encoding and decoding a particular frame doesn't change the
// frame too much. The frame used is a gradient, which does not contain sharp
// transitions, so encoding lossiness should not be too high.
TEST_F(VideoDecoderVpxTest, Gradient) {
  TestGradient(320, 240, 320, 240, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleUpEvenToEven) {
  TestGradient(320, 240, 640, 480, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleUpEvenToOdd) {
  TestGradient(320, 240, 641, 481, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleUpOddToEven) {
  TestGradient(321, 241, 640, 480, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleUpOddToOdd) {
  TestGradient(321, 241, 641, 481, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleDownEvenToEven) {
  TestGradient(320, 240, 160, 120, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleDownEvenToOdd) {
  // The maximum error is non-deterministic. The mean error is not too high,
  // which suggests that the problem is restricted to a small area of the output
  // image. See crbug.com/139437 and crbug.com/139633.
  TestGradient(320, 240, 161, 121, 1.0, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleDownOddToEven) {
  TestGradient(321, 241, 160, 120, 0.04, 0.02);
}

TEST_F(VideoDecoderVpxTest, GradientScaleDownOddToOdd) {
  TestGradient(321, 241, 161, 121, 0.04, 0.02);
}

}  // namespace remoting

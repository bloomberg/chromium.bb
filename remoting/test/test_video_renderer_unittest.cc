// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_video_renderer.h"

#include <cmath>

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "media/base/video_frame.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace {
const int kBytesPerPixel = 4;
const int kDefaultScreenWidth = 1024;
const int kDefaultScreenHeight = 768;
const double kDefaultErrorLimit = 0.02;
}

namespace remoting {
namespace test {

// Provides basic functionality for for the TestVideoRenderer Tests below.
// This fixture also creates an MessageLoop to test decoding video packets.
class TestVideoRendererTest : public testing::Test {
 public:
  TestVideoRendererTest();
  ~TestVideoRendererTest() override;

  // Generate a frame containing a gradient and test decoding of
  // TestVideoRenderer. The original frame is compared to the one obtained from
  // decoding the video packet, and the error at each pixel is the root mean
  // square of the errors in the R, G and B components, each normalized to
  // [0, 1]. This routine checks that the mean error over all pixels do not
  // exceed a given limit.
  void TestVideoPacketProcessing(int screen_width, int screen_height,
                                 double error_limit);

  // Generate a basic desktop frame containing a gradient.
  scoped_ptr<webrtc::DesktopFrame> CreateDesktopFrameWithGradient(
      int screen_width, int screen_height) const;

 protected:
  // Used to post tasks to the message loop.
  scoped_ptr<base::RunLoop> run_loop_;

  // Used to set timeouts and delays.
  scoped_ptr<base::Timer> timer_;

  // Manages the decoder and process generated video packets.
  scoped_ptr<TestVideoRenderer> test_video_renderer_;

  // Used to encode desktop frames to generate video packets.
  scoped_ptr<VideoEncoder> encoder_;

 private:
  // testing::Test interface.
  void SetUp() override;

  // return the mean error of two frames.
  double CalculateError(const webrtc::DesktopFrame* original_frame,
                        const webrtc::DesktopFrame* decoded_frame) const;

  // Fill a desktop frame with a gradient.
  void FillFrameWithGradient(webrtc::DesktopFrame* frame) const;

  // The thread's message loop. Valid only when the thread is alive.
  scoped_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoRendererTest);
};

TestVideoRendererTest::TestVideoRendererTest()
    : timer_(new base::Timer(true, false)) {}

TestVideoRendererTest::~TestVideoRendererTest() {}

void TestVideoRendererTest::SetUp() {
  if (!base::MessageLoop::current()) {
    // Create a temporary message loop if the current thread does not already
    // have one.
    message_loop_.reset(new base::MessageLoop);
  }
  test_video_renderer_.reset(new TestVideoRenderer());
}

void TestVideoRendererTest::TestVideoPacketProcessing(int screen_width,
                                                      int screen_height,
                                                      double error_limit) {
  DCHECK(encoder_);
  DCHECK(test_video_renderer_);

  scoped_ptr<webrtc::DesktopFrame> original_frame =
      CreateDesktopFrameWithGradient(screen_width, screen_height);
  EXPECT_TRUE(original_frame);
  scoped_ptr<VideoPacket> packet = encoder_->Encode(*original_frame.get());
  DCHECK(!run_loop_ || !run_loop_->running());
  run_loop_.reset(new base::RunLoop());

  // Wait for the video packet to be processed and rendered to buffer.
  test_video_renderer_->ProcessVideoPacket(packet.Pass(),
                                           run_loop_->QuitClosure());
  run_loop_->Run();

  scoped_ptr<webrtc::DesktopFrame> buffer_copy =
      test_video_renderer_->GetBufferForTest();
  EXPECT_NE(buffer_copy, nullptr);
  double error = CalculateError(original_frame.get(), buffer_copy.get());
  EXPECT_LT(error, error_limit);
}

double TestVideoRendererTest::CalculateError(
    const webrtc::DesktopFrame* original_frame,
    const webrtc::DesktopFrame* decoded_frame) const {
  DCHECK(original_frame);
  DCHECK(decoded_frame);

  // Check size remains the same after encoding and decoding.
  EXPECT_EQ(original_frame->size().width(), decoded_frame->size().width());
  EXPECT_EQ(original_frame->size().height(), decoded_frame->size().height());
  EXPECT_EQ(original_frame->stride(), decoded_frame->stride());
  int screen_width = original_frame->size().width();
  int screen_height = original_frame->size().height();

  // Error is calculated as the sum of the square error at each pixel in the
  // R, G and B components, each normalized to [0, 1].
  double error_sum_squares = 0.0;

  // The mapping between the position of a pixel on 3-dimensional image
  // (origin at top left corner) and its position in 1-dimensional buffer.
  //
  //  _______________
  // |      |        |      stride = 4 * width;
  // |      |        |
  // |      | height |      height * stride + width + 0; Red channel.
  // |      |        |  =>  height * stride + width + 1; Green channel.
  // |-------        |      height * stride + width + 2; Blue channel.
  // | width         |
  // |_______________|
  //
  for (int height = 0; height < screen_height; ++height) {
    uint8_t* original_ptr = original_frame->data() +
                            height * original_frame->stride();
    uint8_t* decoded_ptr = decoded_frame->data() +
                           height * decoded_frame->stride();

    for (int width = 0; width < screen_width; ++width) {
      // Errors are calculated in the R, G, B components.
      for (int j = 0; j < 3; ++j) {
        int offset = kBytesPerPixel * width + j;
        double original_value = static_cast<double>(*(original_ptr + offset));
        double decoded_value = static_cast<double>(*(decoded_ptr + offset));
        double error = original_value - decoded_value;

        // Normalize the error to [0, 1].
        error /= 255.0;
        error_sum_squares += error * error;
      }
    }
  }
  return sqrt(error_sum_squares / (3 * screen_width * screen_height));
}

scoped_ptr<webrtc::DesktopFrame>
    TestVideoRendererTest::CreateDesktopFrameWithGradient(
        int screen_width, int screen_height) const {
  webrtc::DesktopSize screen_size(screen_width, screen_height);
  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(screen_size));
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(screen_size));
  FillFrameWithGradient(frame.get());
  return frame.Pass();
}

void TestVideoRendererTest::FillFrameWithGradient(
    webrtc::DesktopFrame* frame) const {
  for (int y = 0; y < frame->size().height(); ++y) {
    uint8* p = frame->data() + y * frame->stride();
    for (int x = 0; x < frame->size().width(); ++x) {
      *p++ = (255.0 * x) / frame->size().width();
      *p++ = (164.0 * y) / frame->size().height();
      *p++ = (82.0 * (x + y)) /
          (frame->size().width() + frame->size().height());
      *p++ = 0;
    }
  }
}

// Verify video decoding for VP8 Codec.
TEST_F(TestVideoRendererTest, VerifyVideoDecodingForVP8) {
  encoder_ = VideoEncoderVpx::CreateForVP8();
  test_video_renderer_->SetCodecForDecoding(
      protocol::ChannelConfig::CODEC_VP8);
  TestVideoPacketProcessing(kDefaultScreenWidth, kDefaultScreenHeight,
                            kDefaultErrorLimit);
}

// Verify video decoding for VP9 Codec.
TEST_F(TestVideoRendererTest, VerifyVideoDecodingForVP9) {
  encoder_ = VideoEncoderVpx::CreateForVP9();
  test_video_renderer_->SetCodecForDecoding(
      protocol::ChannelConfig::CODEC_VP9);
  TestVideoPacketProcessing(kDefaultScreenWidth, kDefaultScreenHeight,
                            kDefaultErrorLimit);
}


// Verify video decoding for VERBATIM Codec.
TEST_F(TestVideoRendererTest, VerifyVideoDecodingForVERBATIM) {
  encoder_.reset(new VideoEncoderVerbatim());
  test_video_renderer_->SetCodecForDecoding(
      protocol::ChannelConfig::CODEC_VERBATIM);
  TestVideoPacketProcessing(kDefaultScreenWidth, kDefaultScreenHeight,
                            kDefaultErrorLimit);
}

// Verify a set of video packets are processed correctly.
TEST_F(TestVideoRendererTest, VerifyMultipleVideoProcessing) {
  encoder_ = VideoEncoderVpx::CreateForVP8();
  test_video_renderer_->SetCodecForDecoding(
      protocol::ChannelConfig::CODEC_VP8);

  // Post multiple tasks to |test_video_renderer_|, and it should not crash.
  // 20 is chosen because it's large enough to make sure that there will be
  // more than one task on the video decode thread, while not too large to wait
  // for too long for the unit test to complete.
  const int task_num = 20;
  ScopedVector<VideoPacket> video_packets;
  for (int i = 0; i < task_num; ++i) {
    scoped_ptr<webrtc::DesktopFrame> original_frame =
        CreateDesktopFrameWithGradient(kDefaultScreenWidth,
                                       kDefaultScreenHeight);
    video_packets.push_back(encoder_->Encode(*original_frame.get()));
  }

  for (int i = 0; i < task_num; ++i) {
    // Transfer ownership of video packet.
    VideoPacket* packet = video_packets[i];
    video_packets[i] = nullptr;
    test_video_renderer_->ProcessVideoPacket(make_scoped_ptr(packet),
                                             base::Bind(&base::DoNothing));
  }
}

// Verify video packet size change is handled properly.
TEST_F(TestVideoRendererTest, VerifyVideoPacketSizeChange) {
  encoder_ = VideoEncoderVpx::CreateForVP8();
  test_video_renderer_->SetCodecForDecoding(
      protocol::ChannelConfig::Codec::CODEC_VP8);

  TestVideoPacketProcessing(kDefaultScreenWidth, kDefaultScreenHeight,
                            kDefaultErrorLimit);

  TestVideoPacketProcessing(2 * kDefaultScreenWidth, 2 * kDefaultScreenHeight,
                            kDefaultErrorLimit);

  TestVideoPacketProcessing(kDefaultScreenWidth / 2, kDefaultScreenHeight / 2,
                            kDefaultErrorLimit);
}

}  // namespace test
}  // namespace remoting

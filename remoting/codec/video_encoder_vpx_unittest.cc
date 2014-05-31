// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_vpx.h"

#include <limits>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "remoting/codec/codec_test.h"
#include "remoting/proto/video.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// xRGB pixel colors for use by tests.
const uint32 kBlueColor = 0x0000ff;
const uint32 kGreenColor = 0x00ff00;

// Creates a frame stippled between blue and red pixels, which is useful for
// lossy/lossless encode and color tests.
static scoped_ptr<webrtc::DesktopFrame> CreateTestFrame(
    const webrtc::DesktopSize& frame_size) {
  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(frame_size));
  for (int x = 0; x < frame_size.width(); ++x) {
    for (int y = 0; y < frame_size.height(); ++y) {
      uint8* pixel_u8 = frame->data() + (y * frame->stride()) +
          (x * webrtc::DesktopFrame::kBytesPerPixel);
      *(reinterpret_cast<uint32*>(pixel_u8)) =
          ((x + y) & 1) ? kGreenColor : kBlueColor;
    }
  }
  return frame.Pass();
}

TEST(VideoEncoderVpxTest, TestVp8VideoEncoder) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());
  TestVideoEncoder(encoder.get(), false);
}

TEST(VideoEncoderVpxTest, TestVp9VideoEncoder) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());
  // VP9 encoder defaults to lossless encode and lossy (I420) color.
  TestVideoEncoder(encoder.get(), false);
}

// Test that the VP9 encoder can switch between lossy & lossless encode.
TEST(VideoEncoderVpxTest, TestVp9VideoEncoderLossyEncode) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());

  webrtc::DesktopSize frame_size(1024, 768);
  scoped_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(frame_size));

  // Lossy encode the first frame.
  encoder->SetLosslessEncode(false);
  scoped_ptr<VideoPacket> lossy_packet = encoder->Encode(*frame);

  // Lossless encode the second frame.
  encoder->SetLosslessEncode(true);
  scoped_ptr<VideoPacket> lossless_packet = encoder->Encode(*frame);
  EXPECT_GT(lossless_packet->data().size(), lossy_packet->data().size());

  // Lossy encode one more frame.
  encoder->SetLosslessEncode(false);
  lossy_packet = encoder->Encode(*frame);
  EXPECT_LT(lossy_packet->data().size(), lossless_packet->data().size());
}

// Test that the VP9 encoder can switch between lossy & lossless color.
TEST(VideoEncoderVpxTest, TestVp9VideoEncoderLossyColor) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());

  webrtc::DesktopSize frame_size(1024, 768);
  scoped_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(frame_size));

  // Lossy encode the first frame.
  encoder->SetLosslessColor(false);
  scoped_ptr<VideoPacket> lossy_packet = encoder->Encode(*frame);

  // Lossless encode the second frame.
  encoder->SetLosslessColor(true);
  scoped_ptr<VideoPacket> lossless_packet = encoder->Encode(*frame);
  EXPECT_GT(lossless_packet->data().size(), lossy_packet->data().size());

  // Lossy encode one more frame.
  encoder->SetLosslessColor(false);
  lossy_packet = encoder->Encode(*frame);
  EXPECT_LT(lossy_packet->data().size(), lossless_packet->data().size());
}

// Test that the VP8 encoder ignores lossless modes without crashing.
TEST(VideoEncoderVpxTest, TestVp8VideoEncoderIgnoreLossy) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  webrtc::DesktopSize frame_size(1024, 768);
  scoped_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(frame_size));

  // Encode a frame, to give the encoder a chance to crash if misconfigured.
  encoder->SetLosslessEncode(true);
  encoder->SetLosslessColor(true);
  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);
}

// Test that calling Encode with a differently-sized media::ScreenCaptureData
// does not leak memory.
TEST(VideoEncoderVpxTest, TestSizeChangeNoLeak) {
  webrtc::DesktopSize frame_size(1000, 1000);

  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  // Create first frame & encode it.
  scoped_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(frame_size));
  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);

  // Halve the size of the frame, and updated region, and encode again.
  frame_size.set(frame_size.width(), frame_size.height() / 2);
  frame = CreateTestFrame(frame_size);
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(frame_size));
  packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);
}

// Test that the DPI information is correctly propagated from the
// media::ScreenCaptureData to the VideoPacket.
TEST(VideoEncoderVpxTest, TestDpiPropagation) {
  webrtc::DesktopSize frame_size(32, 32);

  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  scoped_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  frame->set_dpi(webrtc::DesktopVector(96, 97));
  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_EQ(packet->format().x_dpi(), 96);
  EXPECT_EQ(packet->format().y_dpi(), 97);
}

}  // namespace remoting

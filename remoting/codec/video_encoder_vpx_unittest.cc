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

namespace {

const int kIntMax = std::numeric_limits<int>::max();

}  // namespace

namespace remoting {

TEST(VideoEncoderVp8Test, TestVideoEncoder) {
  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());
  TestVideoEncoder(encoder.get(), false);
}

// Test that calling Encode with a differently-sized media::ScreenCaptureData
// does not leak memory.
TEST(VideoEncoderVp8Test, TestSizeChangeNoLeak) {
  int height = 1000;
  int width = 1000;

  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(width, height)));

  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);

  height /= 2;
  frame.reset(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(width, height)));
  packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);
}

// Test that the DPI information is correctly propagated from the
// media::ScreenCaptureData to the VideoPacket.
TEST(VideoEncoderVp8Test, TestDpiPropagation) {
  int height = 32;
  int width = 32;

  scoped_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  scoped_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(width, height)));
  frame->set_dpi(webrtc::DesktopVector(96, 97));
  scoped_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_EQ(packet->format().x_dpi(), 96);
  EXPECT_EQ(packet->format().y_dpi(), 97);
}

}  // namespace remoting

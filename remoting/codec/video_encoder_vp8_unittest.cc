// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_vp8.h"

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
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
  VideoEncoderVp8 encoder;
  TestVideoEncoder(&encoder, false);
}

class VideoEncoderCallback {
 public:
  void DataAvailable(scoped_ptr<VideoPacket> packet) {
  }
};

// Test that calling Encode with a differently-sized media::ScreenCaptureData
// does not leak memory.
TEST(VideoEncoderVp8Test, TestSizeChangeNoLeak) {
  int height = 1000;
  int width = 1000;

  VideoEncoderVp8 encoder;
  VideoEncoderCallback callback;

  scoped_ptr<webrtc::DesktopFrame> frame(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(width, height)));

  encoder.Encode(frame.get(), base::Bind(&VideoEncoderCallback::DataAvailable,
                                         base::Unretained(&callback)));

  height /= 2;
  frame.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(width, height)));
  encoder.Encode(frame.get(), base::Bind(&VideoEncoderCallback::DataAvailable,
                                         base::Unretained(&callback)));
}

class VideoEncoderDpiCallback {
 public:
  void DataAvailable(scoped_ptr<VideoPacket> packet) {
    EXPECT_EQ(packet->format().x_dpi(), 96);
    EXPECT_EQ(packet->format().y_dpi(), 97);
  }
};

// Test that the DPI information is correctly propagated from the
// media::ScreenCaptureData to the VideoPacket.
TEST(VideoEncoderVp8Test, TestDpiPropagation) {
  int height = 32;
  int width = 32;

  VideoEncoderVp8 encoder;
  VideoEncoderDpiCallback callback;

  scoped_ptr<webrtc::DesktopFrame> frame(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(width, height)));
  frame->set_dpi(webrtc::DesktopVector(96, 97));
  encoder.Encode(frame.get(),
                 base::Bind(&VideoEncoderDpiCallback::DataAvailable,
                            base::Unretained(&callback)));
}

}  // namespace remoting

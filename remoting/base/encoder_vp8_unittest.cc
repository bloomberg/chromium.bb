// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/encoder_vp8.h"
#include "remoting/proto/video.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kIntMax = std::numeric_limits<int>::max();

}  // namespace

namespace remoting {

TEST(EncoderVp8Test, TestEncoder) {
  EncoderVp8 encoder;
  TestEncoder(&encoder, false);
}

class EncoderCallback {
 public:
  void DataAvailable(VideoPacket *packet) {
    delete packet;
  }
};

// Test that calling Encode with a differently-sized CaptureData does not
// leak memory.
TEST(EncoderVp8Test, TestSizeChangeNoLeak) {
  int height = 1000;
  int width = 1000;
  const int kBytesPerPixel = 4;

  EncoderVp8 encoder;
  EncoderCallback callback;

  std::vector<uint8> buffer(width * height * kBytesPerPixel);
  DataPlanes planes;
  planes.data[0] = &buffer.front();
  planes.strides[0] = width;

  scoped_refptr<CaptureData> capture_data(new CaptureData(
      planes, SkISize::Make(width, height), media::VideoFrame::RGB32));
  encoder.Encode(capture_data, false,
                 NewCallback(&callback, &EncoderCallback::DataAvailable));

  height /= 2;
  capture_data = new CaptureData(planes, SkISize::Make(width, height),
                                 media::VideoFrame::RGB32);
  encoder.Encode(capture_data, false,
                 NewCallback(&callback, &EncoderCallback::DataAvailable));
}

TEST(EncoderVp8Test, AlignAndClipRect) {
  // Simple test case (no clipping).
  SkIRect r1(SkIRect::MakeXYWH(100, 200, 300, 400));
  EXPECT_EQ(EncoderVp8::AlignAndClipRect(r1, kIntMax, kIntMax), r1);

  // Should expand outward to r1.
  SkIRect r2(SkIRect::MakeXYWH(101, 201, 298, 398));
  EXPECT_EQ(EncoderVp8::AlignAndClipRect(r2, kIntMax, kIntMax), r1);

  // Test clipping to screen size.
  EXPECT_EQ(EncoderVp8::AlignAndClipRect(r1, 110, 220),
            SkIRect::MakeXYWH(100, 200, 10, 20));

  // Rectangle completely off-screen.
  EXPECT_TRUE(EncoderVp8::AlignAndClipRect(r1, 50, 50).isEmpty());

  // Clipping to odd-sized screen.  An unlikely case, and we might not deal
  // with it cleanly in the encoder (we possibly lose 1px at right & bottom
  // of screen).
  EXPECT_EQ(EncoderVp8::AlignAndClipRect(r1, 199, 299),
            SkIRect::MakeXYWH(100, 200, 98, 98));
}

}  // namespace remoting

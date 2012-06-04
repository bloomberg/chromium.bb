// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <vector>

#include "base/bind.h"
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
  void DataAvailable(scoped_ptr<VideoPacket> packet) {
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
                 base::Bind(&EncoderCallback::DataAvailable,
                            base::Unretained(&callback)));

  height /= 2;
  capture_data = new CaptureData(planes, SkISize::Make(width, height),
                                 media::VideoFrame::RGB32);
  encoder.Encode(capture_data, false,
                 base::Bind(&EncoderCallback::DataAvailable,
                            base::Unretained(&callback)));
}

}  // namespace remoting

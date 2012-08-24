// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_row_based.h"

#include "remoting/codec/codec_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(VideoEncoderZlibTest, TestVideoEncoder) {
  scoped_ptr<VideoEncoderRowBased> encoder(
      VideoEncoderRowBased::CreateZlibEncoder());
  TestVideoEncoder(encoder.get(), true);
}

TEST(VideoEncoderZlibTest, TestVideoEncoderSmallOutputBuffer) {
  scoped_ptr<VideoEncoderRowBased> encoder(
      VideoEncoderRowBased::CreateZlibEncoder(16));
  TestVideoEncoder(encoder.get(), true);
}

}  // namespace remoting

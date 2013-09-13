// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_verbatim.h"

#include "remoting/codec/codec_test.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(VideoEncoderVerbatimTest, TestVideoEncoder) {
  scoped_ptr<VideoEncoderVerbatim> encoder(new VideoEncoderVerbatim());
  TestVideoEncoder(encoder.get(), true);
}

TEST(VideoEncoderVerbatimTest, EncodeAndDecode) {
  scoped_ptr<VideoEncoderVerbatim> encoder(new VideoEncoderVerbatim());
  scoped_ptr<VideoDecoderVerbatim> decoder(new VideoDecoderVerbatim());
  TestVideoEncoderDecoder(encoder.get(), decoder.get(), true);
}

}  // namespace remoting

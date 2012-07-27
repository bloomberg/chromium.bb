// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/decoder_vp8.h"
#include "remoting/base/encoder_vp8.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(DecoderVp8Test, EncodeAndDecode) {
  EncoderVp8 encoder;
  DecoderVp8 decoder;
  TestEncoderDecoder(&encoder, &decoder, false);
}

// Check that encoding and decoding a particular frame doesn't change the
// frame too much. The frame used is a gradient, which does not contain sharp
// transitions, so encoding lossiness should not be too high.
TEST(DecoderVp8Test, Gradient) {
  EncoderVp8 encoder;
  DecoderVp8 decoder;
  TestEncoderDecoderGradient(&encoder, &decoder, 0.03, 0.01);
}

}  // namespace remoting

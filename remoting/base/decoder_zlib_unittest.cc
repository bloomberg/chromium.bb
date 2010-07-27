// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/decoder_zlib.h"
#include "remoting/base/decompressor_zlib.h"
#include "remoting/base/encoder_zlib.h"
#include "remoting/client/mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace remoting {

TEST(DecoderZlibTest, EncodeAndDecode) {
  EncoderZlib encoder;
  DecoderZlib decoder;
  TestEncoderDecoder(&encoder, &decoder, true);
}

TEST(DecoderZlibTest, EncodeAndDecodeSmallOutputBuffer) {
  EncoderZlib encoder(64);
  DecoderZlib decoder;
  TestEncoderDecoder(&encoder, &decoder, true);
}

}  // namespace remoting

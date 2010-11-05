// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/decoder_row_based.h"
#include "remoting/base/encoder_zlib.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncodeDecodeTest, EncodeAndDecodeZlib) {
  EncoderZlib encoder;
  scoped_ptr<DecoderRowBased> decoder(DecoderRowBased::CreateZlibDecoder());
  decoder->set_reverse_rows(false);
  TestEncoderDecoder(&encoder, decoder.get(), true);
}

TEST(EncodeDecodeTest, EncodeAndDecodeSmallOutputBufferZlib) {
  EncoderZlib encoder(64);
  scoped_ptr<DecoderRowBased> decoder(DecoderRowBased::CreateZlibDecoder());
  decoder->set_reverse_rows(false);
  TestEncoderDecoder(&encoder, decoder.get(), true);
}

TEST(EncodeDecodeTest, EncodeAndDecodeNoneStrictZlib) {
  EncoderZlib encoder;
  scoped_ptr<DecoderRowBased> decoder(DecoderRowBased::CreateZlibDecoder());
  TestEncoderDecoder(&encoder, decoder.get(), false);
}

}  // namespace remoting

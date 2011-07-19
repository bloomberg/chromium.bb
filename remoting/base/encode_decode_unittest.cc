// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/decoder_row_based.h"
#include "remoting/base/encoder_row_based.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncodeDecodeTest, EncodeAndDecodeZlib) {
  scoped_ptr<EncoderRowBased> encoder(EncoderRowBased::CreateZlibEncoder());
  scoped_ptr<DecoderRowBased> decoder(DecoderRowBased::CreateZlibDecoder());
  TestEncoderDecoder(encoder.get(), decoder.get(), true);
}

TEST(EncodeDecodeTest, EncodeAndDecodeSmallOutputBufferZlib) {
  scoped_ptr<EncoderRowBased> encoder(EncoderRowBased::CreateZlibEncoder(64));
  scoped_ptr<DecoderRowBased> decoder(DecoderRowBased::CreateZlibDecoder());
  TestEncoderDecoder(encoder.get(), decoder.get(), true);
}

TEST(EncodeDecodeTest, EncodeAndDecodeNoneStrictZlib) {
  scoped_ptr<EncoderRowBased> encoder(EncoderRowBased::CreateZlibEncoder());
  scoped_ptr<DecoderRowBased> decoder(DecoderRowBased::CreateZlibDecoder());
  TestEncoderDecoder(encoder.get(), decoder.get(), false);
}

}  // namespace remoting

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_row_based.h"

#include "media/base/video_frame.h"
#include "remoting/codec/codec_test.h"
#include "remoting/codec/video_decoder_row_based.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncodeDecodeTest, EncodeAndDecodeZlib) {
  scoped_ptr<VideoEncoderRowBased> encoder(
      VideoEncoderRowBased::CreateZlibEncoder());
  scoped_ptr<VideoDecoderRowBased> decoder(
      VideoDecoderRowBased::CreateZlibDecoder());
  TestVideoEncoderDecoder(encoder.get(), decoder.get(), true);
}

TEST(EncodeDecodeTest, EncodeAndDecodeSmallOutputBufferZlib) {
  scoped_ptr<VideoEncoderRowBased> encoder(
      VideoEncoderRowBased::CreateZlibEncoder(64));
  scoped_ptr<VideoDecoderRowBased> decoder(
      VideoDecoderRowBased::CreateZlibDecoder());
  TestVideoEncoderDecoder(encoder.get(), decoder.get(), true);
}

TEST(EncodeDecodeTest, EncodeAndDecodeNoneStrictZlib) {
  scoped_ptr<VideoEncoderRowBased> encoder(
      VideoEncoderRowBased::CreateZlibEncoder());
  scoped_ptr<VideoDecoderRowBased> decoder(
      VideoDecoderRowBased::CreateZlibDecoder());
  TestVideoEncoderDecoder(encoder.get(), decoder.get(), false);
}

}  // namespace remoting

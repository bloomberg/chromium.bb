// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "remoting/base/codec_test.h"
#include "remoting/base/decoder_vp8.h"
#include "remoting/base/encoder_vp8.h"
#include "remoting/client/mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace remoting {

TEST(DecoderVp8Test, EncodeAndDecode) {
  EncoderVp8 encoder;
  DecoderVp8 decoder;
  TestEncoderDecoder(&encoder, &decoder, false);
}

}  // namespace remoting

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/codec_test.h"
#include "remoting/base/encoder_zlib.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncoderZlibTest, TestEncoder) {
  EncoderZlib encoder;
  TestEncoder(&encoder, true);
}


TEST(EncoderZlibTest, TestEncoderSmallOutputBuffer) {
  EncoderZlib encoder(16);
  TestEncoder(&encoder, true);
}

}  // namespace remoting

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/codec_test.h"
#include "remoting/base/encoder_verbatim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncoderVerbatimTest, TestEncoder) {
  EncoderVerbatim encoder;
  TestEncoder(&encoder, true);
}

}  // namespace remoting

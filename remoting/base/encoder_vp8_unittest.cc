// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/codec_test.h"
#include "remoting/base/encoder_vp8.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncoderVp8Test, TestEncoder) {
  EncoderVp8 encoder;
  TestEncoder(&encoder, false);
}

}  // namespace remoting

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/codec_test.h"
#include "remoting/base/encoder_row_based.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EncoderZlibTest, TestEncoder) {
  scoped_ptr<EncoderRowBased> encoder(EncoderRowBased::CreateZlibEncoder());
  TestEncoder(encoder.get(), true);
}

TEST(EncoderZlibTest, TestEncoderSmallOutputBuffer) {
  scoped_ptr<EncoderRowBased> encoder(EncoderRowBased::CreateZlibEncoder(16));
  TestEncoder(encoder.get(), true);
}

}  // namespace remoting

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PaintInvalidationReason.h"

#include <sstream>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PaintInvalidationReasonTest, StreamOutput) {
  {
    std::stringstream stringStream;
    PaintInvalidationReason reason = PaintInvalidationNone;
    stringStream << reason;
    EXPECT_EQ("none", stringStream.str());
  }
  {
    std::stringstream stringStream;
    PaintInvalidationReason reason = PaintInvalidationDelayedFull;
    stringStream << reason;
    EXPECT_EQ("delayed full", stringStream.str());
  }
}

}  // namespace blink

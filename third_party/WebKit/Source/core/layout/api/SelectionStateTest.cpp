// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/api/SelectionState.h"

#include <sstream>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SelectionStateTest, StreamOutput) {
  // Just explicitly sanity check a couple of values.
  {
    std::stringstream stringStream;
    stringStream << SelectionNone;
    EXPECT_EQ("None", stringStream.str());
  }
  {
    std::stringstream stringStream;
    stringStream << SelectionBoth;
    EXPECT_EQ("Both", stringStream.str());
  }
}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/PositionWithAffinity.h"

#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class PositionWithAffinityTest : public EditingTestBase {};

TEST_F(PositionWithAffinityTest, OperatorBool) {
  SetBodyContent("foo");
  EXPECT_FALSE(static_cast<bool>(PositionWithAffinity()));
  EXPECT_TRUE(static_cast<bool>(
      PositionWithAffinity(Position(GetDocument().body(), 0))));
}

}  // namespace blink

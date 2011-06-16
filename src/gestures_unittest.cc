// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "gestures/include/gestures.h"

namespace gestures {

class GesturesTest : public ::testing::Test {};

TEST(GesturesTest, SimpleTest) {
  // Simple allocate/free test
  scoped_ptr<GestureInterpreter> gs(NewGestureInterpreter());
  EXPECT_NE(static_cast<GestureInterpreter*>(NULL), gs.get());
}

}  // namespace gestures

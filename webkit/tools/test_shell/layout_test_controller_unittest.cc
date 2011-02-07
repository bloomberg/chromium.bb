// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "webkit/tools/test_shell/layout_test_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// A subclass of LayoutTestController, with additional accessors.
class TestLayoutTestController : public LayoutTestController {
 public:
  TestLayoutTestController() : LayoutTestController(NULL) {
  }

  size_t MethodCount() {
    return methods_.size();
  }

  void Reset() {
    LayoutTestController::Reset();
  }
};

class LayoutTestControllerTest : public testing::Test {
};
} // namespace

TEST(LayoutTestControllerTest, MethodMapIsInitialized) {
  const char* test_methods[] = {
    "waitUntilDone",
    "notifyDone",
    "queueLoad",
    "windowCount",
    NULL
  };
  TestLayoutTestController controller;
  for (const char** method = test_methods; *method; ++method) {
     EXPECT_TRUE(controller.IsMethodRegistered(*method));
  }

  // One more case, to test our test.
  EXPECT_FALSE(controller.IsMethodRegistered("nonexistent_method"));
}

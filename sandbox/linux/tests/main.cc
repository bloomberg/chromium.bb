// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  // Always go through re-execution for death tests.
  // This makes gtest only marginally slower for us and has the
  // additional side effect of getting rid of gtest warnings about fork()
  // safety.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  return RUN_ALL_TESTS();
}

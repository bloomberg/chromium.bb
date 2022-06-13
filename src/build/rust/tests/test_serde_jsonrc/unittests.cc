// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_serde_jsonrc/lib.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RustTest, SerdeJsonTest) {
  EXPECT_EQ(true, serde_works());
}

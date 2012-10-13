// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/pin_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(IsPinValidTest, Normal) {
  EXPECT_EQ(true, IsPinValid("123456"));
}

TEST(IsPinValidTest, Short) {
  EXPECT_EQ(false, IsPinValid("12345"));
}

TEST(IsPinValidTest, Long) {
  EXPECT_EQ(true, IsPinValid("1234567"));
}

TEST(IsPinValidTest, BadCharacter) {
  EXPECT_EQ(false, IsPinValid("12345/"));
  EXPECT_EQ(false, IsPinValid("123456/"));
  EXPECT_EQ(false, IsPinValid("/123456"));
  EXPECT_EQ(false, IsPinValid("12345:"));
  EXPECT_EQ(false, IsPinValid("123456:"));
  EXPECT_EQ(false, IsPinValid(":123456"));
  EXPECT_EQ(false, IsPinValid("12345a"));
  EXPECT_EQ(false, IsPinValid("123456a"));
  EXPECT_EQ(false, IsPinValid("a123456"));
}

}  // namespace remoting

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/network_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

TEST(ParsePortRange, Basic) {
  uint16 min, max;

  // Valid range
  EXPECT_TRUE(NetworkSettings::ParsePortRange("1-65535", &min, &max));
  EXPECT_EQ(1u, min);
  EXPECT_EQ(65535u, max);

  EXPECT_TRUE(NetworkSettings::ParsePortRange(" 1 - 65535 ", &min, &max));
  EXPECT_EQ(1u, min);
  EXPECT_EQ(65535u, max);

  EXPECT_TRUE(NetworkSettings::ParsePortRange("12400-12400", &min, &max));
  EXPECT_EQ(12400u, min);
  EXPECT_EQ(12400u, max);

  // Invalid
  EXPECT_FALSE(NetworkSettings::ParsePortRange("", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("-65535", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("1-", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("-", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("-1-65535", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("1--65535", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("1-65535-", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("0-65535", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("1-65536", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("1-4294967295", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("10-1", &min, &max));
  EXPECT_FALSE(NetworkSettings::ParsePortRange("1foo-2bar", &min, &max));
}

}  // namespace protocol
}  // namespace remoting

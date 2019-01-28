// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_packet_number.h"

#include "net/third_party/quic/platform/api/quic_test.h"

namespace quic {

namespace test {

namespace {

TEST(QuicPacketNumberTest, BasicTest) {
  QuicPacketNumber num;
  EXPECT_FALSE(num.IsInitialized());

  QuicPacketNumber num2(10);
  EXPECT_TRUE(num2.IsInitialized());
  EXPECT_EQ(10u, num2.ToUint64());
  EXPECT_EQ(10u, num2.Hash());
  num2.Clear();
  EXPECT_FALSE(num2.IsInitialized());

  QuicPacketNumber num3(std::numeric_limits<uint64_t>::max());
  EXPECT_TRUE(num3.IsInitialized());
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(), num3.ToUint64());
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(), num3.Hash());
  num3.Clear();
  EXPECT_FALSE(num3.IsInitialized());
}

TEST(QuicPacketNumberTest, Operators) {
  QuicPacketNumber num(100);
  EXPECT_EQ(QuicPacketNumber(100), num++);
  EXPECT_EQ(QuicPacketNumber(101), num);
  EXPECT_EQ(QuicPacketNumber(101), num--);
  EXPECT_EQ(QuicPacketNumber(100), num);

  EXPECT_EQ(QuicPacketNumber(101), ++num);
  EXPECT_EQ(QuicPacketNumber(100), --num);
}

}  // namespace

}  // namespace test

}  // namespace quic

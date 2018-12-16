// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_connection_id.h"

#include <cstdint>
#include <cstring>

#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic {

namespace {

class QuicConnectionIdTest : public QuicTest {};

TEST_F(QuicConnectionIdTest, Empty) {
  QuicConnectionId connection_id_empty = EmptyQuicConnectionId();
  EXPECT_TRUE(QuicConnectionIdIsEmpty(connection_id_empty));
}

TEST_F(QuicConnectionIdTest, NotEmpty) {
  QuicConnectionId connection_id = QuicConnectionIdFromUInt64(1);
  EXPECT_FALSE(QuicConnectionIdIsEmpty(connection_id));
}

TEST_F(QuicConnectionIdTest, DoubleConvert) {
  QuicConnectionId connection_id64_1 = QuicConnectionIdFromUInt64(1);
  QuicConnectionId connection_id64_2 = QuicConnectionIdFromUInt64(42);
  QuicConnectionId connection_id64_3 =
      QuicConnectionIdFromUInt64(UINT64_C(0xfedcba9876543210));
  EXPECT_EQ(
      connection_id64_1,
      QuicConnectionIdFromUInt64(QuicConnectionIdToUInt64(connection_id64_1)));
  EXPECT_EQ(
      connection_id64_2,
      QuicConnectionIdFromUInt64(QuicConnectionIdToUInt64(connection_id64_2)));
  EXPECT_EQ(
      connection_id64_3,
      QuicConnectionIdFromUInt64(QuicConnectionIdToUInt64(connection_id64_3)));
  EXPECT_NE(connection_id64_1, connection_id64_2);
  EXPECT_NE(connection_id64_1, connection_id64_3);
  EXPECT_NE(connection_id64_2, connection_id64_3);
}

TEST_F(QuicConnectionIdTest, Hash) {
  QuicConnectionId connection_id64_1 = QuicConnectionIdFromUInt64(1);
  QuicConnectionId connection_id64_1b = QuicConnectionIdFromUInt64(1);
  QuicConnectionId connection_id64_2 = QuicConnectionIdFromUInt64(42);
  QuicConnectionId connection_id64_3 =
      QuicConnectionIdFromUInt64(UINT64_C(0xfedcba9876543210));
  EXPECT_EQ(connection_id64_1.Hash(), connection_id64_1b.Hash());
  EXPECT_NE(connection_id64_1.Hash(), connection_id64_2.Hash());
  EXPECT_NE(connection_id64_1.Hash(), connection_id64_3.Hash());
  EXPECT_NE(connection_id64_2.Hash(), connection_id64_3.Hash());
}

}  // namespace

}  // namespace quic

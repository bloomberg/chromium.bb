// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_manager.h"

#include "cast/common/channel/proto/cast_channel.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {
namespace {

static_assert(::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0 ==
                  static_cast<int>(VirtualConnection::ProtocolVersion::kV2_1_0),
              "V2 1.0 constants must be equal");
static_assert(::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_1 ==
                  static_cast<int>(VirtualConnection::ProtocolVersion::kV2_1_1),
              "V2 1.1 constants must be equal");
static_assert(::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_2 ==
                  static_cast<int>(VirtualConnection::ProtocolVersion::kV2_1_2),
              "V2 1.2 constants must be equal");
static_assert(::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_3 ==
                  static_cast<int>(VirtualConnection::ProtocolVersion::kV2_1_3),
              "V2 1.3 constants must be equal");

using ::testing::_;
using ::testing::Invoke;

class VirtualConnectionManagerTest : public ::testing::Test {
 protected:
  VirtualConnectionManager manager_;

  VirtualConnection vc1_{"local1", "peer1", 75};
  VirtualConnection vc2_{"local2", "peer2", 76};
  VirtualConnection vc3_{"local1", "peer3", 75};
};

}  // namespace

TEST_F(VirtualConnectionManagerTest, NoConnections) {
  EXPECT_FALSE(manager_.GetConnectionData(vc1_));
  EXPECT_FALSE(manager_.GetConnectionData(vc2_));
  EXPECT_FALSE(manager_.GetConnectionData(vc3_));
}

TEST_F(VirtualConnectionManagerTest, AddConnections) {
  VirtualConnection::AssociatedData data1 = {};

  manager_.AddConnection(vc1_, std::move(data1));
  EXPECT_TRUE(manager_.GetConnectionData(vc1_));
  EXPECT_FALSE(manager_.GetConnectionData(vc2_));
  EXPECT_FALSE(manager_.GetConnectionData(vc3_));

  VirtualConnection::AssociatedData data2 = {};
  manager_.AddConnection(vc2_, std::move(data2));
  EXPECT_TRUE(manager_.GetConnectionData(vc1_));
  EXPECT_TRUE(manager_.GetConnectionData(vc2_));
  EXPECT_FALSE(manager_.GetConnectionData(vc3_));

  VirtualConnection::AssociatedData data3 = {};
  manager_.AddConnection(vc3_, std::move(data3));
  EXPECT_TRUE(manager_.GetConnectionData(vc1_));
  EXPECT_TRUE(manager_.GetConnectionData(vc2_));
  EXPECT_TRUE(manager_.GetConnectionData(vc3_));
}

TEST_F(VirtualConnectionManagerTest, RemoveConnections) {
  VirtualConnection::AssociatedData data1 = {};
  VirtualConnection::AssociatedData data2 = {};
  VirtualConnection::AssociatedData data3 = {};

  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_TRUE(manager_.RemoveConnection(
      vc1_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.GetConnectionData(vc1_));
  EXPECT_TRUE(manager_.GetConnectionData(vc2_));
  EXPECT_TRUE(manager_.GetConnectionData(vc3_));

  EXPECT_TRUE(manager_.RemoveConnection(
      vc2_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.GetConnectionData(vc1_));
  EXPECT_FALSE(manager_.GetConnectionData(vc2_));
  EXPECT_TRUE(manager_.GetConnectionData(vc3_));

  EXPECT_TRUE(manager_.RemoveConnection(
      vc3_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.GetConnectionData(vc1_));
  EXPECT_FALSE(manager_.GetConnectionData(vc2_));
  EXPECT_FALSE(manager_.GetConnectionData(vc3_));

  EXPECT_FALSE(manager_.RemoveConnection(
      vc1_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.RemoveConnection(
      vc2_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.RemoveConnection(
      vc3_, VirtualConnection::CloseReason::kClosedBySelf));
}

TEST_F(VirtualConnectionManagerTest, RemoveConnectionsByIds) {
  VirtualConnection::AssociatedData data1 = {};
  VirtualConnection::AssociatedData data2 = {};
  VirtualConnection::AssociatedData data3 = {};

  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_EQ(manager_.RemoveConnectionsByLocalId(
                "local1", VirtualConnection::CloseReason::kClosedBySelf),
            2u);
  EXPECT_FALSE(manager_.GetConnectionData(vc1_));
  EXPECT_TRUE(manager_.GetConnectionData(vc2_));
  EXPECT_FALSE(manager_.GetConnectionData(vc3_));

  data1 = {};
  data2 = {};
  data3 = {};
  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));
  EXPECT_EQ(manager_.RemoveConnectionsBySocketId(
                76, VirtualConnection::CloseReason::kClosedBySelf),
            1u);
  EXPECT_TRUE(manager_.GetConnectionData(vc1_));
  EXPECT_FALSE(manager_.GetConnectionData(vc2_));
  EXPECT_TRUE(manager_.GetConnectionData(vc3_));

  EXPECT_EQ(manager_.RemoveConnectionsBySocketId(
                75, VirtualConnection::CloseReason::kClosedBySelf),
            2u);
  EXPECT_FALSE(manager_.GetConnectionData(vc1_));
  EXPECT_FALSE(manager_.GetConnectionData(vc2_));
  EXPECT_FALSE(manager_.GetConnectionData(vc3_));
}

}  // namespace cast
}  // namespace openscreen

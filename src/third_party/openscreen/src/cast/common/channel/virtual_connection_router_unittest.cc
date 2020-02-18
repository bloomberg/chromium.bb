// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/test/fake_cast_socket.h"
#include "cast/common/channel/test/mock_cast_message_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;

class MockSocketErrorHandler
    : public VirtualConnectionRouter::SocketErrorHandler {
 public:
  MOCK_METHOD(void, OnClose, (CastSocket * socket), (override));
  MOCK_METHOD(void, OnError, (CastSocket * socket, Error error), (override));
};

class VirtualConnectionRouterTest : public ::testing::Test {
 public:
  void SetUp() override {
    socket_id_ = fake_cast_socket_pair_.socket->socket_id();
    router_.TakeSocket(&mock_error_handler_,
                       std::move(fake_cast_socket_pair_.socket));
  }

 protected:
  CastSocket& peer_socket() { return *fake_cast_socket_pair_.peer_socket; }

  FakeCastSocketPair fake_cast_socket_pair_;
  uint32_t socket_id_;

  MockSocketErrorHandler mock_error_handler_;
  MockCastMessageHandler mock_message_handler_;

  VirtualConnectionManager manager_;
  VirtualConnectionRouter router_{&manager_};
};

}  // namespace

TEST_F(VirtualConnectionRouterTest, LocalIdHandler) {
  router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler_);
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-9873", socket_id_}, {});

  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _, _));
  peer_socket().SendMessage(message);

  EXPECT_CALL(mock_message_handler_, OnMessage(_, _, _));
  peer_socket().SendMessage(message);

  message.set_destination_id("receiver-4321");
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _, _)).Times(0);
  peer_socket().SendMessage(message);
}

TEST_F(VirtualConnectionRouterTest, RemoveLocalIdHandler) {
  router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler_);
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-9873", socket_id_}, {});

  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _, _));
  peer_socket().SendMessage(message);

  router_.RemoveHandlerForLocalId("receiver-1234");

  EXPECT_CALL(mock_message_handler_, OnMessage(_, _, _)).Times(0);
  peer_socket().SendMessage(message);
}

TEST_F(VirtualConnectionRouterTest, SendMessage) {
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-4321", socket_id_}, {});

  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("receiver-1234");
  message.set_destination_id("sender-4321");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(fake_cast_socket_pair_.mock_peer_client, OnMessage(_, _))
      .WillOnce(Invoke([](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message.namespace_(), "zrqvn");
        EXPECT_EQ(message.source_id(), "receiver-1234");
        EXPECT_EQ(message.destination_id(), "sender-4321");
        ASSERT_EQ(message.payload_type(), CastMessage_PayloadType_STRING);
        EXPECT_EQ(message.payload_utf8(), "cnlybnq");
      }));
  router_.SendMessage(
      VirtualConnection{"receiver-1234", "sender-4321", socket_id_},
      std::move(message));
}

}  // namespace channel
}  // namespace cast

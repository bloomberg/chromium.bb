// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include <utility>

#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/testing/fake_cast_socket.h"
#include "cast/common/channel/testing/mock_cast_message_handler.h"
#include "cast/common/channel/testing/mock_socket_error_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/public/cast_socket.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {
namespace {

using ::cast::channel::CastMessage;
using ::testing::_;
using ::testing::Invoke;
using ::testing::SaveArg;
using ::testing::WithArg;

class VirtualConnectionRouterTest : public ::testing::Test {
 public:
  void SetUp() override {
    local_socket_ = fake_cast_socket_pair_.socket.get();
    local_router_.TakeSocket(&mock_error_handler_,
                             std::move(fake_cast_socket_pair_.socket));

    remote_socket_ = fake_cast_socket_pair_.peer_socket.get();
    remote_router_.TakeSocket(&mock_error_handler_,
                              std::move(fake_cast_socket_pair_.peer_socket));
  }

 protected:
  FakeCastSocketPair fake_cast_socket_pair_;
  CastSocket* local_socket_;
  CastSocket* remote_socket_;

  MockSocketErrorHandler mock_error_handler_;

  VirtualConnectionManager local_manager_;
  VirtualConnectionRouter local_router_{&local_manager_};

  VirtualConnectionManager remote_manager_;
  VirtualConnectionRouter remote_router_{&remote_manager_};
};

}  // namespace

TEST_F(VirtualConnectionRouterTest, LocalIdHandler) {
  MockCastMessageHandler mock_message_handler;
  local_router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler);
  local_manager_.AddConnection(VirtualConnection{"receiver-1234", "sender-9873",
                                                 local_socket_->socket_id()},
                               {});

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(mock_message_handler, OnMessage(_, local_socket_, _));
  EXPECT_TRUE(remote_socket_->Send(message).ok());

  EXPECT_CALL(mock_message_handler, OnMessage(_, local_socket_, _));
  EXPECT_TRUE(remote_socket_->Send(message).ok());

  message.set_destination_id("receiver-4321");
  EXPECT_CALL(mock_message_handler, OnMessage(_, _, _)).Times(0);
  EXPECT_TRUE(remote_socket_->Send(message).ok());

  local_router_.RemoveHandlerForLocalId("receiver-1234");
}

TEST_F(VirtualConnectionRouterTest, RemoveLocalIdHandler) {
  MockCastMessageHandler mock_message_handler;
  local_router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler);
  local_manager_.AddConnection(VirtualConnection{"receiver-1234", "sender-9873",
                                                 local_socket_->socket_id()},
                               {});

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(mock_message_handler, OnMessage(_, local_socket_, _));
  EXPECT_TRUE(remote_socket_->Send(message).ok());

  local_router_.RemoveHandlerForLocalId("receiver-1234");

  EXPECT_CALL(mock_message_handler, OnMessage(_, local_socket_, _)).Times(0);
  EXPECT_TRUE(remote_socket_->Send(message).ok());

  local_router_.RemoveHandlerForLocalId("receiver-1234");
}

TEST_F(VirtualConnectionRouterTest, SendMessage) {
  local_manager_.AddConnection(VirtualConnection{"receiver-1234", "sender-4321",
                                                 local_socket_->socket_id()},
                               {});

  MockCastMessageHandler destination;
  remote_router_.AddHandlerForLocalId("sender-4321", &destination);
  remote_manager_.AddConnection(
      VirtualConnection{"sender-4321", "receiver-1234",
                        remote_socket_->socket_id()},
      {});

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("receiver-1234");
  message.set_destination_id("sender-4321");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  ASSERT_TRUE(message.IsInitialized());

  EXPECT_CALL(destination, OnMessage(&remote_router_, remote_socket_, _))
      .WillOnce(
          WithArg<2>(Invoke([&message](CastMessage message_at_destination) {
            ASSERT_TRUE(message_at_destination.IsInitialized());
            EXPECT_EQ(message.SerializeAsString(),
                      message_at_destination.SerializeAsString());
          })));
  local_router_.Send(VirtualConnection{"receiver-1234", "sender-4321",
                                       local_socket_->socket_id()},
                     message);
}

TEST_F(VirtualConnectionRouterTest, CloseSocketRemovesVirtualConnections) {
  local_manager_.AddConnection(VirtualConnection{"receiver-1234", "sender-4321",
                                                 local_socket_->socket_id()},
                               {});

  EXPECT_CALL(mock_error_handler_, OnClose(local_socket_)).Times(1);

  int id = local_socket_->socket_id();
  local_router_.CloseSocket(id);
  EXPECT_FALSE(local_manager_.GetConnectionData(
      VirtualConnection{"receiver-1234", "sender-4321", id}));
}

// Tests that VirtualConnectionRouter::Send() broadcasts a message from a local
// source to both: 1) all other local peers; and 2) all remote peers.
TEST_F(VirtualConnectionRouterTest, BroadcastsFromLocalSource) {
  // Local peers.
  MockCastMessageHandler alice, bob;
  local_router_.AddHandlerForLocalId("alice", &alice);
  local_router_.AddHandlerForLocalId("bob", &bob);

  // Remote peers.
  MockCastMessageHandler charlie, dave, eve;
  remote_router_.AddHandlerForLocalId("charlie", &charlie);
  remote_router_.AddHandlerForLocalId("dave", &dave);
  remote_router_.AddHandlerForLocalId("eve", &eve);

  // The local broadcaster, which should never receive her own messages.
  MockCastMessageHandler wendy;
  local_router_.AddHandlerForLocalId("wendy", &wendy);
  EXPECT_CALL(wendy, OnMessage(_, _, _)).Times(0);

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");

  CastMessage message_alice_got, message_bob_got, message_charlie_got,
      message_dave_got, message_eve_got;
  EXPECT_CALL(alice, OnMessage(&local_router_, nullptr, _))
      .WillOnce(SaveArg<2>(&message_alice_got))
      .RetiresOnSaturation();
  EXPECT_CALL(bob, OnMessage(&local_router_, nullptr, _))
      .WillOnce(SaveArg<2>(&message_bob_got))
      .RetiresOnSaturation();
  EXPECT_CALL(charlie, OnMessage(&remote_router_, remote_socket_, _))
      .WillOnce(SaveArg<2>(&message_charlie_got))
      .RetiresOnSaturation();
  EXPECT_CALL(dave, OnMessage(&remote_router_, remote_socket_, _))
      .WillOnce(SaveArg<2>(&message_dave_got))
      .RetiresOnSaturation();
  EXPECT_CALL(eve, OnMessage(&remote_router_, remote_socket_, _))
      .WillOnce(SaveArg<2>(&message_eve_got))
      .RetiresOnSaturation();
  ASSERT_TRUE(local_router_.BroadcastFromLocalPeer("wendy", message).ok());

  // Confirm message data is correct.
  message.set_source_id("wendy");
  message.set_destination_id(kBroadcastId);
  ASSERT_TRUE(message.IsInitialized());
  ASSERT_TRUE(message_alice_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(), message_alice_got.SerializeAsString());
  ASSERT_TRUE(message_bob_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(), message_bob_got.SerializeAsString());
  ASSERT_TRUE(message_charlie_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(),
            message_charlie_got.SerializeAsString());
  ASSERT_TRUE(message_dave_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(), message_dave_got.SerializeAsString());
  ASSERT_TRUE(message_eve_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(), message_eve_got.SerializeAsString());

  // Remove one local peer and one remote peer, and confirm only the correct
  // entities receive a broadcast message.
  local_router_.RemoveHandlerForLocalId("bob");
  remote_router_.RemoveHandlerForLocalId("charlie");
  EXPECT_CALL(alice, OnMessage(&local_router_, nullptr, _)).Times(1);
  EXPECT_CALL(bob, OnMessage(_, _, _)).Times(0);
  EXPECT_CALL(charlie, OnMessage(_, _, _)).Times(0);
  EXPECT_CALL(dave, OnMessage(&remote_router_, remote_socket_, _)).Times(1);
  EXPECT_CALL(eve, OnMessage(&remote_router_, remote_socket_, _)).Times(1);
  ASSERT_TRUE(local_router_.BroadcastFromLocalPeer("wendy", message).ok());
}

// Tests that VirtualConnectionRouter::OnMessage() broadcasts a message from a
// remote source to all local peers.
TEST_F(VirtualConnectionRouterTest, BroadcastsFromRemoteSource) {
  // Local peers.
  MockCastMessageHandler alice, bob, charlie;
  local_router_.AddHandlerForLocalId("alice", &alice);
  local_router_.AddHandlerForLocalId("bob", &bob);
  local_router_.AddHandlerForLocalId("charlie", &charlie);

  // The remote broadcaster, which should never receive her own messages.
  MockCastMessageHandler wendy;
  remote_router_.AddHandlerForLocalId("wendy", &wendy);
  EXPECT_CALL(wendy, OnMessage(_, _, _)).Times(0);

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");

  CastMessage message_alice_got, message_bob_got, message_charlie_got;
  EXPECT_CALL(alice, OnMessage(&local_router_, local_socket_, _))
      .WillOnce(SaveArg<2>(&message_alice_got))
      .RetiresOnSaturation();
  EXPECT_CALL(bob, OnMessage(&local_router_, local_socket_, _))
      .WillOnce(SaveArg<2>(&message_bob_got))
      .RetiresOnSaturation();
  EXPECT_CALL(charlie, OnMessage(&local_router_, local_socket_, _))
      .WillOnce(SaveArg<2>(&message_charlie_got))
      .RetiresOnSaturation();
  ASSERT_TRUE(remote_router_.BroadcastFromLocalPeer("wendy", message).ok());

  // Confirm message data is correct.
  message.set_source_id("wendy");
  message.set_destination_id(kBroadcastId);
  ASSERT_TRUE(message.IsInitialized());
  ASSERT_TRUE(message_alice_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(), message_alice_got.SerializeAsString());
  ASSERT_TRUE(message_bob_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(), message_bob_got.SerializeAsString());
  ASSERT_TRUE(message_charlie_got.IsInitialized());
  EXPECT_EQ(message.SerializeAsString(),
            message_charlie_got.SerializeAsString());

  // Remove one local peer, and confirm only the two remaining local peers
  // receive a broadcast message from the remote source.
  local_router_.RemoveHandlerForLocalId("bob");
  EXPECT_CALL(alice, OnMessage(&local_router_, local_socket_, _)).Times(1);
  EXPECT_CALL(bob, OnMessage(_, _, _)).Times(0);
  EXPECT_CALL(charlie, OnMessage(&local_router_, local_socket_, _)).Times(1);
  ASSERT_TRUE(remote_router_.BroadcastFromLocalPeer("wendy", message).ok());
}

}  // namespace cast
}  // namespace openscreen

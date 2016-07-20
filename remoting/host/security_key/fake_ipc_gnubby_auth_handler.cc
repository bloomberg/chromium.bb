// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_ipc_gnubby_auth_handler.h"

#include <memory>

#include "base/time/time.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/chromoting_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeIpcGnubbyAuthHandler::FakeIpcGnubbyAuthHandler() {}

FakeIpcGnubbyAuthHandler::~FakeIpcGnubbyAuthHandler() {}

void FakeIpcGnubbyAuthHandler::CreateGnubbyConnection() {
  ipc_server_channel_ = IPC::Channel::CreateNamedServer(
      IPC::ChannelHandle(ipc_server_channel_name_), this);
  ASSERT_NE(nullptr, ipc_server_channel_);
  ASSERT_TRUE(ipc_server_channel_->Connect());
}

bool FakeIpcGnubbyAuthHandler::IsValidConnectionId(int connection_id) const {
  return false;
}

void FakeIpcGnubbyAuthHandler::SendClientResponse(
    int connection_id,
    const std::string& response_data) {}

void FakeIpcGnubbyAuthHandler::SendErrorAndCloseConnection(int connection_id) {}

void FakeIpcGnubbyAuthHandler::SetSendMessageCallback(
    const SendMessageCallback& callback) {}

size_t FakeIpcGnubbyAuthHandler::GetActiveConnectionCountForTest() const {
  return 0;
}

void FakeIpcGnubbyAuthHandler::SetRequestTimeoutForTest(
    base::TimeDelta timeout) {}

bool FakeIpcGnubbyAuthHandler::OnMessageReceived(const IPC::Message& message) {
  // This class does handle any IPC messages sent by the client.
  return false;
}

void FakeIpcGnubbyAuthHandler::OnChannelConnected(int32_t peer_pid) {
  ASSERT_TRUE(ipc_server_channel_->Send(
      new ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionDetails(
          ipc_security_key_channel_name_)));
}

void FakeIpcGnubbyAuthHandler::OnChannelError() {}

}  // namespace remoting

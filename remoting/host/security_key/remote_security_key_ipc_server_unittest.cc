// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_ipc_server.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "remoting/host/security_key/fake_remote_security_key_ipc_client.h"
#include "remoting/host/security_key/remote_security_key_ipc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kTestConnectionId = 42;
const int kInitialConnectTimeoutMs = 250;
const int kConnectionTimeoutErrorDeltaMs = 100;
const int kLargeMessageSizeBytes = 256 * 1024;
}  // namespace

namespace remoting {

class RemoteSecurityKeyIpcServerTest : public testing::Test {
 public:
  RemoteSecurityKeyIpcServerTest();
  ~RemoteSecurityKeyIpcServerTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete();

  // Used as a callback to signal receipt of a security key request message.
  void SendRequestToClient(int connection_id, const std::string& data);

 protected:
  // Returns a unique IPC channel name which prevents conflicts when running
  // tests concurrently.
  std::string GetUniqueTestChannelName();

  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // IPC tests require a valid MessageLoop to run.
  base::MessageLoopForIO message_loop_;

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The object under test.
  std::unique_ptr<RemoteSecurityKeyIpcServer> remote_security_key_ipc_server_;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC channel.
  int last_connection_id_received_ = -1;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyIpcServerTest);
};

RemoteSecurityKeyIpcServerTest::RemoteSecurityKeyIpcServerTest()
    : run_loop_(new base::RunLoop()) {
  uint32_t peer_session_id = UINT32_MAX;
#if defined(OS_WIN)
  EXPECT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(),
                                   reinterpret_cast<DWORD*>(&peer_session_id)));
#endif  // defined(OS_WIN)

  remote_security_key_ipc_server_ =
      remoting::RemoteSecurityKeyIpcServer::Create(
          kTestConnectionId, peer_session_id,
          base::TimeDelta::FromMilliseconds(kInitialConnectTimeoutMs),
          base::Bind(&RemoteSecurityKeyIpcServerTest::SendRequestToClient,
                     base::Unretained(this)),
          base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                     base::Unretained(this)));
}

RemoteSecurityKeyIpcServerTest::~RemoteSecurityKeyIpcServerTest() {}

void RemoteSecurityKeyIpcServerTest::OperationComplete() {
  run_loop_->Quit();
}

void RemoteSecurityKeyIpcServerTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void RemoteSecurityKeyIpcServerTest::SendRequestToClient(
    int connection_id,
    const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete();
}

std::string RemoteSecurityKeyIpcServerTest::GetUniqueTestChannelName() {
  return GetChannelNamePathPrefixForTest() + "Super_Awesome_Test_Channel." +
         IPC::Channel::GenerateUniqueRandomChannelID();
}

TEST_F(RemoteSecurityKeyIpcServerTest, HandleSingleGnubbyRequest) {
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data("Blergh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data("Blargh!");
  ASSERT_TRUE(remote_security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcServerTest, HandleLargeGnubbyRequest) {
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data(kLargeMessageSizeBytes, 'Y');
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data(kLargeMessageSizeBytes, 'Z');
  ASSERT_TRUE(remote_security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcServerTest, HandleReallyLargeGnubbyRequest) {
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data(kLargeMessageSizeBytes * 2, 'Y');
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data(kLargeMessageSizeBytes * 2, 'Z');
  ASSERT_TRUE(remote_security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcServerTest, HandleMultipleGnubbyRequests) {
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data_1("Blergh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data_1);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data_1, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data_1("Blargh!");
  ASSERT_TRUE(remote_security_key_ipc_server_->SendResponse(response_data_1));
  WaitForOperationComplete();

  // Verify the response was received.
  ASSERT_EQ(response_data_1, fake_ipc_client.last_message_received());

  // Send a request from the IPC client to the IPC server.
  std::string request_data_2("Bleh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data_2);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data_2, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data_2("Meh!");
  ASSERT_TRUE(remote_security_key_ipc_server_->SendResponse(response_data_2));
  WaitForOperationComplete();

  // Verify the response was received.
  ASSERT_EQ(response_data_2, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcServerTest, InitialIpcConnectionTimeout) {
  // Create a channel, then wait for the done callback to be called indicating
  // the connection was closed.  This test simulates the IPC Server being
  // created but the client failing to connect to it.
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), kInitialConnectTimeoutMs,
              kConnectionTimeoutErrorDeltaMs);
}

TEST_F(RemoteSecurityKeyIpcServerTest, NoGnubbyRequestTimeout) {
  // Create a channel and connect to it via IPC but do not send a request.
  // The channel should be closed and cleaned up if the IPC client does not
  // issue a request within the specified timeout period.
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Now that a connection has been established, we wait for the timeout.
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), kInitialConnectTimeoutMs,
              kConnectionTimeoutErrorDeltaMs);
}

TEST_F(RemoteSecurityKeyIpcServerTest, GnubbyResponseTimeout) {
  // Create a channel, connect to it via IPC, and issue a request, but do
  // not send a response.  This simulates a client-side timeout.
  base::TimeDelta request_timeout(base::TimeDelta::FromMilliseconds(50));
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(channel_name,
                                                             request_timeout));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Now that a connection has been established, we issue a request and
  // then wait for the timeout.
  std::string request_data("I can haz Auth?");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Leave the request hanging until it times out...
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), request_timeout.InMilliseconds(),
              kConnectionTimeoutErrorDeltaMs);
}

TEST_F(RemoteSecurityKeyIpcServerTest, SendResponseTimeout) {
  // Create a channel, connect to it via IPC, issue a request, and send
  // a response, but do not close the channel after that.  The connection
  // should be terminated after the initial timeout period has elapsed.
  base::TimeDelta request_timeout(base::TimeDelta::FromMilliseconds(500));
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(
      channel_name, request_timeout));

  // Create a fake client and connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Issue a request.
  std::string request_data("Auth me yo!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Send a response from the IPC server to the IPC client.
  std::string response_data("OK, the secret code is 1-2-3-4-5");
  ASSERT_TRUE(remote_security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Now wait for the timeout period for the connection to be torn down.
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), request_timeout.InMilliseconds(),
              kConnectionTimeoutErrorDeltaMs);
}

#if defined(OS_WIN)
TEST_F(RemoteSecurityKeyIpcServerTest, IpcConnectionFailsFromInvalidSession) {
  uint32_t peer_session_id = UINT32_MAX;
  ASSERT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(),
                                   reinterpret_cast<DWORD*>(&peer_session_id)));
  peer_session_id++;

  // Reinitialize the object under test.
  remote_security_key_ipc_server_ =
      remoting::RemoteSecurityKeyIpcServer::Create(
          kTestConnectionId, peer_session_id,
          base::TimeDelta::FromMilliseconds(kInitialConnectTimeoutMs),
          base::Bind(&RemoteSecurityKeyIpcServerTest::SendRequestToClient,
                     base::Unretained(this)),
          base::Bind(&base::DoNothing));

  base::TimeDelta request_timeout(base::TimeDelta::FromMilliseconds(500));
  std::string channel_name(GetUniqueTestChannelName());
  ASSERT_TRUE(remote_security_key_ipc_server_->CreateChannel(channel_name,
                                                             request_timeout));

  // Create a fake client and attempt to connect to the IPC server channel.
  FakeRemoteSecurityKeyIpcClient fake_ipc_client(
      base::Bind(&RemoteSecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(channel_name));
  WaitForOperationComplete();
  WaitForOperationComplete();

  // Verify the connection failed.
  ASSERT_FALSE(fake_ipc_client.ipc_channel_connected());
}
#endif  // defined(OS_WIN)

}  // namespace remoting

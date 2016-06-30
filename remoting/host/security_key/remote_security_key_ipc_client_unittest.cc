// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_ipc_client.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "remoting/host/security_key/fake_ipc_gnubby_auth_handler.h"
#include "remoting/host/security_key/fake_remote_security_key_ipc_server.h"
#include "remoting/host/security_key/remote_security_key_ipc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kTestConnectionId = 1;
const char kNonexistentIpcChannelName[] = "Nonexistent_IPC_Channel";
const char kValidIpcChannelName[] =
    "Remote_Security_Key_Ipc_Client_Test_Channel.";
const int kLargeMessageSizeBytes = 256 * 1024;
}  // namespace

namespace remoting {

class RemoteSecurityKeyIpcClientTest : public testing::Test {
 public:
  RemoteSecurityKeyIpcClientTest();
  ~RemoteSecurityKeyIpcClientTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete(bool failed);

  // Used as a callback given to the object under test, expected to be called
  // back when a security key request is received by it.
  void SendMessageToClient(int connection_id, const std::string& data);

  // Used as a callback given to the object under test, expected to be called
  // back when a security key response is sent.
  void ClientMessageReceived(const std::string& response_payload);

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // Sets up an active IPC connection between |remote_security_key_ipc_client_|
  // and |fake_ipc_server_|.  |expect_success| defines whether the operation
  // is expected to succeed or fail.
  void EstablishConnection(bool expect_success);

  // Sends a security key request from |remote_security_key_ipc_client_| and
  // a response from |fake_ipc_server_| and verifies the payloads for both.
  void SendRequestAndResponse(const std::string& request_data,
                              const std::string& response_data);

  // Creates a unique IPC channel name to use for testing.
  std::string GenerateUniqueTestChannelName();

  // IPC tests require a valid MessageLoop to run.
  base::MessageLoopForIO message_loop_;

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The object under test.
  RemoteSecurityKeyIpcClient remote_security_key_ipc_client_;

  // Provides a connection details message to |remote_security_key_ipc_client_|
  // for testing.
  FakeIpcGnubbyAuthHandler fake_gnubby_auth_handler_;

  // Used to send/receive security key IPC messages for testing.
  FakeRemoteSecurityKeyIpcServer fake_ipc_server_;

  // Stores the current session ID on supported platforms.
  uint32_t session_id_ = 0;

  // Tracks the success/failure of the last async operation.
  bool operation_failed_ = false;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC channel.
  int last_connection_id_received_ = -1;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyIpcClientTest);
};

RemoteSecurityKeyIpcClientTest::RemoteSecurityKeyIpcClientTest()
    : run_loop_(new base::RunLoop()),
      fake_ipc_server_(
          kTestConnectionId,
          /*peer_session_id=*/UINT32_MAX,
          /*initial_connect_timeout=*/base::TimeDelta::FromMilliseconds(500),
          base::Bind(&RemoteSecurityKeyIpcClientTest::SendMessageToClient,
                     base::Unretained(this)),
          base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                     base::Unretained(this),
                     /*failed=*/false)) {}

RemoteSecurityKeyIpcClientTest::~RemoteSecurityKeyIpcClientTest() {}

void RemoteSecurityKeyIpcClientTest::SetUp() {
#if defined(OS_WIN)
  DWORD session_id = 0;
  // If we are on Windows, then we need to set the correct session ID or the
  // IPC connection will not be created successfully.
  ASSERT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &session_id));
  session_id_ = session_id;
  remote_security_key_ipc_client_.SetExpectedIpcServerSessionIdForTest(
      session_id_);
#endif  // defined(OS_WIN)
}

void RemoteSecurityKeyIpcClientTest::OperationComplete(bool failed) {
  operation_failed_ |= failed;
  run_loop_->Quit();
}

void RemoteSecurityKeyIpcClientTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void RemoteSecurityKeyIpcClientTest::SendMessageToClient(
    int connection_id,
    const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete(/*failed=*/false);
}

void RemoteSecurityKeyIpcClientTest::ClientMessageReceived(
    const std::string& response_payload) {
  last_message_received_ = response_payload;
  OperationComplete(/*failed=*/false);
}

std::string RemoteSecurityKeyIpcClientTest::GenerateUniqueTestChannelName() {
  return GetChannelNamePathPrefixForTest() + kValidIpcChannelName +
         IPC::Channel::GenerateUniqueRandomChannelID();
}

void RemoteSecurityKeyIpcClientTest::EstablishConnection(bool expect_success) {
  // Start up the security key forwarding session IPC channel first, that way
  // we can provide the channel using the fake GnubbyAuthHandler later on.
  std::string ipc_session_channel_name = GenerateUniqueTestChannelName();
  ASSERT_TRUE(fake_ipc_server_.CreateChannel(
      ipc_session_channel_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));
  ASSERT_TRUE(IPC::Channel::IsNamedServerInitialized(ipc_session_channel_name));
  fake_gnubby_auth_handler_.set_ipc_security_key_channel_name(
      ipc_session_channel_name);

  // Set up the channel name for the initial IPC channel.
  std::string ipc_server_channel_name = GenerateUniqueTestChannelName();
  fake_gnubby_auth_handler_.set_ipc_server_channel_name(
      ipc_server_channel_name);
  remote_security_key_ipc_client_.SetInitialIpcChannelNameForTest(
      ipc_server_channel_name);

  // Create the initial IPC channel and verify it was set up correctly.
  ASSERT_FALSE(
      remote_security_key_ipc_client_.WaitForSecurityKeyIpcServerChannel());
  fake_gnubby_auth_handler_.CreateGnubbyConnection();
  ASSERT_TRUE(IPC::Channel::IsNamedServerInitialized(ipc_server_channel_name));
  ASSERT_TRUE(
      remote_security_key_ipc_client_.WaitForSecurityKeyIpcServerChannel());

  // Establish the IPC channel so we can begin sending and receiving security
  // key messages.
  remote_security_key_ipc_client_.EstablishIpcConnection(
      base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/false),
      base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();
  ASSERT_NE(operation_failed_, expect_success);
}

void RemoteSecurityKeyIpcClientTest::SendRequestAndResponse(
    const std::string& request_data,
    const std::string& response_data) {
  ASSERT_TRUE(remote_security_key_ipc_client_.SendSecurityKeyRequest(
      request_data,
      base::Bind(&RemoteSecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);
  ASSERT_EQ(last_connection_id_received_, kTestConnectionId);
  ASSERT_EQ(last_message_received_, request_data);

  ASSERT_TRUE(fake_ipc_server_.SendResponse(response_data));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);
  ASSERT_EQ(last_message_received_, response_data);
}

TEST_F(RemoteSecurityKeyIpcClientTest, GenerateSingleGnubbyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse("Auth me!", "You've been authed!");

  remote_security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcClientTest, GenerateLargeGnubbyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes, 'Y'),
                         std::string(kLargeMessageSizeBytes, 'Z'));

  remote_security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcClientTest, GenerateReallyLargeGnubbyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes * 2, 'Y'),
                         std::string(kLargeMessageSizeBytes * 2, 'Z'));

  remote_security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcClientTest, GenerateMultipleGnubbyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse("Auth me 1!", "You've been authed once!");
  SendRequestAndResponse("Auth me 2!", "You've been authed twice!");
  SendRequestAndResponse("Auth me 3!", "You've been authed thrice!");

  remote_security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(RemoteSecurityKeyIpcClientTest,
       ServerClosesConnectionAfterRequestTimeout) {
  EstablishConnection(/*expect_success=*/true);
  fake_ipc_server_.CloseChannel();
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);
}

TEST_F(RemoteSecurityKeyIpcClientTest,
       SecondGnubbyRequestBeforeFirstResponseReceived) {
  EstablishConnection(/*expect_success=*/true);

  ASSERT_TRUE(remote_security_key_ipc_client_.SendSecurityKeyRequest(
      "First Request",
      base::Bind(&RemoteSecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_FALSE(remote_security_key_ipc_client_.SendSecurityKeyRequest(
      "Second Request",
      base::Bind(&RemoteSecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
}

TEST_F(RemoteSecurityKeyIpcClientTest, ReceiveGnubbyResponseWithEmptyPayload) {
  EstablishConnection(/*expect_success=*/true);

  ASSERT_TRUE(remote_security_key_ipc_client_.SendSecurityKeyRequest(
      "Valid request",
      base::Bind(&RemoteSecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_TRUE(fake_ipc_server_.SendResponse(""));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(RemoteSecurityKeyIpcClientTest,
       SendRequestBeforeEstablishingConnection) {
  // Sending a request will fail since the IPC connection has not been
  // established.
  ASSERT_FALSE(remote_security_key_ipc_client_.SendSecurityKeyRequest(
      "Too soon!!",
      base::Bind(&RemoteSecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
}

TEST_F(RemoteSecurityKeyIpcClientTest, NonExistentMainIpcServerChannel) {
  std::string ipc_server_channel_name(kNonexistentIpcChannelName);
  remote_security_key_ipc_client_.SetInitialIpcChannelNameForTest(
      ipc_server_channel_name);

  // Attempt to establish the conection (should fail since the IPC channel does
  // not exist).
  remote_security_key_ipc_client_.EstablishIpcConnection(
      base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/false),
      base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(RemoteSecurityKeyIpcClientTest, NonExistentIpcSessionChannel) {
  fake_gnubby_auth_handler_.set_ipc_security_key_channel_name(
      kNonexistentIpcChannelName);

  // Set up the channel name for the initial IPC channel.
  std::string ipc_server_channel_name = GenerateUniqueTestChannelName();
  fake_gnubby_auth_handler_.set_ipc_server_channel_name(
      ipc_server_channel_name);
  remote_security_key_ipc_client_.SetInitialIpcChannelNameForTest(
      ipc_server_channel_name);

  // Create the initial IPC channel and verify it was set up correctly.
  ASSERT_FALSE(
      remote_security_key_ipc_client_.WaitForSecurityKeyIpcServerChannel());
  fake_gnubby_auth_handler_.CreateGnubbyConnection();
  ASSERT_TRUE(IPC::Channel::IsNamedServerInitialized(ipc_server_channel_name));
  ASSERT_TRUE(
      remote_security_key_ipc_client_.WaitForSecurityKeyIpcServerChannel());

  // Attempt to establish the conection (should fail since the IPC channel does
  // not exist).
  remote_security_key_ipc_client_.EstablishIpcConnection(
      base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/false),
      base::Bind(&RemoteSecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

#if defined(OS_WIN)
TEST_F(RemoteSecurityKeyIpcClientTest, GnubbyIpcServerRunningInWrongSession) {
  // Set the expected session Id to a different session than we are running in.
  remote_security_key_ipc_client_.SetExpectedIpcServerSessionIdForTest(
      session_id_ + 1);

  // Attempting to establish a connection should fail here since the IPC Server
  // is 'running' in a different session than expected.
  EstablishConnection(/*expect_success=*/false);
}
#endif  // defined(OS_WIN)

}  // namespace remoting

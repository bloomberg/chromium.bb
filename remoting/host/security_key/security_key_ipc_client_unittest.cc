// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_client.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/test/scoped_ipc_support.h"
#include "remoting/host/security_key/fake_security_key_ipc_server.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kTestConnectionId = 1;
const char kNonexistentIpcChannelName[] = "Nonexistent_IPC_Channel";
const char kValidIpcChannelName[] = "SecurityKeyIpcClientTest";
const int kLargeMessageSizeBytes = 256 * 1024;
}  // namespace

namespace remoting {

class SecurityKeyIpcClientTest : public testing::Test {
 public:
  SecurityKeyIpcClientTest();
  ~SecurityKeyIpcClientTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete(bool failed);

  // Callback used to signal when the IPC channel is ready for messages.
  void ConnectionStateHandler(bool established);

  // Callback used to drive the |fake_ipc_server_| connection behavior.
  void SendConnectionMessage();

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

  // Sets up an active IPC connection between |security_key_ipc_client_|
  // and |fake_ipc_server_|.  |expect_success| defines whether the operation
  // is expected to succeed or fail.
  void EstablishConnection(bool expect_success);

  // Sends a security key request from |security_key_ipc_client_| and
  // a response from |fake_ipc_server_| and verifies the payloads for both.
  void SendRequestAndResponse(const std::string& request_data,
                              const std::string& response_data);

  // Creates a unique IPC channel name to use for testing.
  std::string GenerateUniqueTestChannelName();

  // IPC tests require a valid MessageLoop to run.
  base::MessageLoopForIO message_loop_;

  mojo::edk::test::ScopedIPCSupport ipc_support_;

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The object under test.
  SecurityKeyIpcClient security_key_ipc_client_;

  // Used to send/receive security key IPC messages for testing.
  FakeSecurityKeyIpcServer fake_ipc_server_;

  // Stores the current session ID on supported platforms.
  uint32_t session_id_ = 0;

  // Tracks the success/failure of the last async operation.
  bool operation_failed_ = false;

  // Tracks whether the IPC channel connection has been established.
  bool connection_established_ = false;

  // Used to drive invalid session behavior for testing the client.
  bool simulate_invalid_session_ = false;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC channel.
  int last_connection_id_received_ = -1;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityKeyIpcClientTest);
};

SecurityKeyIpcClientTest::SecurityKeyIpcClientTest()
    : ipc_support_(message_loop_.task_runner()),
      run_loop_(new base::RunLoop()),
      fake_ipc_server_(
          kTestConnectionId,
          /*client_session_details=*/nullptr,
          /*initial_connect_timeout=*/base::TimeDelta::FromMilliseconds(500),
          base::Bind(&SecurityKeyIpcClientTest::SendMessageToClient,
                     base::Unretained(this)),
          base::Bind(&SecurityKeyIpcClientTest::SendConnectionMessage,
                     base::Unretained(this)),
          base::Bind(&SecurityKeyIpcClientTest::OperationComplete,
                     base::Unretained(this),
                     /*failed=*/false)) {}

SecurityKeyIpcClientTest::~SecurityKeyIpcClientTest() {}

void SecurityKeyIpcClientTest::SetUp() {
#if defined(OS_WIN)
  DWORD session_id = 0;
  // If we are on Windows, then we need to set the correct session ID or the
  // IPC connection will not be created successfully.
  ASSERT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &session_id));
  session_id_ = session_id;
  security_key_ipc_client_.SetExpectedIpcServerSessionIdForTest(session_id_);
#endif  // defined(OS_WIN)
}

void SecurityKeyIpcClientTest::OperationComplete(bool failed) {
  operation_failed_ |= failed;
  run_loop_->Quit();
}

void SecurityKeyIpcClientTest::ConnectionStateHandler(bool established) {
  connection_established_ = established;
  OperationComplete(/*failed=*/false);
}

void SecurityKeyIpcClientTest::SendConnectionMessage() {
  if (simulate_invalid_session_) {
    fake_ipc_server_.SendInvalidSessionMessage();
  } else {
    fake_ipc_server_.SendConnectionReadyMessage();
  }
}

void SecurityKeyIpcClientTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void SecurityKeyIpcClientTest::SendMessageToClient(int connection_id,
                                                   const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete(/*failed=*/false);
}

void SecurityKeyIpcClientTest::ClientMessageReceived(
    const std::string& response_payload) {
  last_message_received_ = response_payload;
  OperationComplete(/*failed=*/false);
}

std::string SecurityKeyIpcClientTest::GenerateUniqueTestChannelName() {
  return GetChannelNamePathPrefixForTest() + kValidIpcChannelName +
         IPC::Channel::GenerateUniqueRandomChannelID();
}

void SecurityKeyIpcClientTest::EstablishConnection(bool expect_success) {
  // Start up the security key forwarding session IPC channel first, that way
  // we can provide the channel using the fake SecurityKeyAuthHandler later on.
  mojo::edk::NamedPlatformHandle channel_handle(
      GenerateUniqueTestChannelName());
  security_key_ipc_client_.SetIpcChannelHandleForTest(channel_handle);
  ASSERT_TRUE(fake_ipc_server_.CreateChannel(
      channel_handle,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  ASSERT_TRUE(security_key_ipc_client_.CheckForSecurityKeyIpcServerChannel());

  // Establish the IPC channel so we can begin sending and receiving security
  // key messages.
  security_key_ipc_client_.EstablishIpcConnection(
      base::Bind(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                 base::Unretained(this)),
      base::Bind(&SecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();
  ASSERT_EQ(connection_established_, expect_success);
  ASSERT_NE(operation_failed_, expect_success);
}

void SecurityKeyIpcClientTest::SendRequestAndResponse(
    const std::string& request_data,
    const std::string& response_data) {
  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      request_data, base::Bind(&SecurityKeyIpcClientTest::ClientMessageReceived,
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

TEST_F(SecurityKeyIpcClientTest, GenerateSingleSecurityKeyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse("Auth me!", "You've been authed!");

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateLargeSecurityKeyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes, 'Y'),
                         std::string(kLargeMessageSizeBytes, 'Z'));

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateReallyLargeSecurityKeyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes * 2, 'Y'),
                         std::string(kLargeMessageSizeBytes * 2, 'Z'));

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateMultipleSecurityKeyRequest) {
  EstablishConnection(/*expect_success=*/true);

  SendRequestAndResponse("Auth me 1!", "You've been authed once!");
  SendRequestAndResponse("Auth me 2!", "You've been authed twice!");
  SendRequestAndResponse("Auth me 3!", "You've been authed thrice!");

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, ServerClosesConnectionAfterRequestTimeout) {
  EstablishConnection(/*expect_success=*/true);
  fake_ipc_server_.CloseChannel();
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest,
       SecondSecurityKeyRequestBeforeFirstResponseReceived) {
  EstablishConnection(/*expect_success=*/true);

  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      "First Request",
      base::Bind(&SecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_FALSE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Second Request",
      base::Bind(&SecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
}

TEST_F(SecurityKeyIpcClientTest, ReceiveSecurityKeyResponseWithEmptyPayload) {
  EstablishConnection(/*expect_success=*/true);

  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Valid request",
      base::Bind(&SecurityKeyIpcClientTest::ClientMessageReceived,
                 base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_TRUE(fake_ipc_server_.SendResponse(""));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest, SendRequestBeforeEstablishingConnection) {
  // Sending a request will fail since the IPC connection has not been
  // established.
  ASSERT_FALSE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Too soon!!", base::Bind(&SecurityKeyIpcClientTest::ClientMessageReceived,
                               base::Unretained(this))));
}

TEST_F(SecurityKeyIpcClientTest, NonExistentIpcServerChannel) {
  security_key_ipc_client_.SetIpcChannelHandleForTest(
      mojo::edk::NamedPlatformHandle(kNonexistentIpcChannelName));

  // Attempt to establish the conection (should fail since the IPC channel does
  // not exist).
  security_key_ipc_client_.EstablishIpcConnection(
      base::Bind(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                 base::Unretained(this)),
      base::Bind(&SecurityKeyIpcClientTest::OperationComplete,
                 base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

#if defined(OS_WIN)
TEST_F(SecurityKeyIpcClientTest, SecurityKeyIpcServerRunningInWrongSession) {
  // Set the expected session Id to a different session than we are running in.
  security_key_ipc_client_.SetExpectedIpcServerSessionIdForTest(session_id_ +
                                                                1);
  simulate_invalid_session_ = true;

  // Attempting to establish a connection should fail here since the IPC Server
  // is 'running' in a different session than expected.
  EstablishConnection(/*expect_success=*/false);
}
#endif  // defined(OS_WIN)

}  // namespace remoting

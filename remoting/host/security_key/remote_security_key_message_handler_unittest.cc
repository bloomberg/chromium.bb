// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/security_key/fake_remote_security_key_ipc_client.h"
#include "remoting/host/security_key/fake_remote_security_key_message_reader.h"
#include "remoting/host/security_key/fake_remote_security_key_message_writer.h"
#include "remoting/host/security_key/remote_security_key_ipc_constants.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class RemoteSecurityKeyMessageHandlerTest : public testing::Test {
 public:
  RemoteSecurityKeyMessageHandlerTest();
  ~RemoteSecurityKeyMessageHandlerTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an action.
  void OperationComplete();

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // Passed to |message_channel_| and called back when a message is received.
  void OnSecurityKeyMessage(RemoteSecurityKeyMessageType message_type,
                            const std::string& message_payload);

  bool last_operation_failed_ = false;
  RemoteSecurityKeyMessageType last_message_type_received_ =
      RemoteSecurityKeyMessageType::INVALID;
  std::string last_message_payload_received_;

  base::WeakPtr<FakeRemoteSecurityKeyIpcClient> ipc_client_weak_ptr_;
  base::WeakPtr<FakeRemoteSecurityKeyMessageReader> reader_weak_ptr_;
  base::WeakPtr<FakeRemoteSecurityKeyMessageWriter> writer_weak_ptr_;
  std::unique_ptr<RemoteSecurityKeyMessageHandler> message_handler_;

 private:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyMessageHandlerTest);
};

RemoteSecurityKeyMessageHandlerTest::RemoteSecurityKeyMessageHandlerTest() {}

RemoteSecurityKeyMessageHandlerTest::~RemoteSecurityKeyMessageHandlerTest() {}

void RemoteSecurityKeyMessageHandlerTest::OperationComplete() {
  run_loop_->Quit();
}

void RemoteSecurityKeyMessageHandlerTest::SetUp() {
  run_loop_.reset(new base::RunLoop());
  message_handler_.reset(new RemoteSecurityKeyMessageHandler());

  std::unique_ptr<FakeRemoteSecurityKeyIpcClient> ipc_client(
      new FakeRemoteSecurityKeyIpcClient(
          base::Bind(&RemoteSecurityKeyMessageHandlerTest::OperationComplete,
                     base::Unretained(this))));
  ipc_client_weak_ptr_ = ipc_client->AsWeakPtr();

  std::unique_ptr<FakeRemoteSecurityKeyMessageReader> reader(
      new FakeRemoteSecurityKeyMessageReader());
  reader_weak_ptr_ = reader->AsWeakPtr();

  std::unique_ptr<FakeRemoteSecurityKeyMessageWriter> writer(
      new FakeRemoteSecurityKeyMessageWriter(
          base::Bind(&RemoteSecurityKeyMessageHandlerTest::OperationComplete,
                     base::Unretained(this))));
  writer_weak_ptr_ = writer->AsWeakPtr();

  message_handler_->SetRemoteSecurityKeyMessageReaderForTest(std::move(reader));

  message_handler_->SetRemoteSecurityKeyMessageWriterForTest(std::move(writer));

  base::File read_file;
  base::File write_file;
  ASSERT_TRUE(MakePipe(&read_file, &write_file));
  message_handler_->Start(
      std::move(read_file), std::move(write_file), std::move(ipc_client),
      base::Bind(&RemoteSecurityKeyMessageHandlerTest::OperationComplete,
                 base::Unretained(this)));
}

void RemoteSecurityKeyMessageHandlerTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void RemoteSecurityKeyMessageHandlerTest::OnSecurityKeyMessage(
    RemoteSecurityKeyMessageType message_type,
    const std::string& message_payload) {
  last_message_type_received_ = message_type;
  last_message_payload_received_ = message_payload;

  OperationComplete();
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessConnectMessage_SessionExists_ConnectionAttemptSuccess) {
  ipc_client_weak_ptr_->set_wait_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(true);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::CONNECT, std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::CONNECT_RESPONSE,
            writer_weak_ptr_->last_message_type());
  ASSERT_EQ(std::string(1, kConnectResponseActiveSession),
            writer_weak_ptr_->last_message_payload());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessConnectMessage_SessionExists_WriteFails) {
  ipc_client_weak_ptr_->set_wait_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(true);
  writer_weak_ptr_->set_write_request_succeeded(/*should_succeed=*/false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::CONNECT, std::string()));
  WaitForOperationComplete();

  ASSERT_FALSE(ipc_client_weak_ptr_.get());
  ASSERT_FALSE(reader_weak_ptr_.get());
  ASSERT_FALSE(writer_weak_ptr_.get());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessConnectMessage_SessionExists_ConnectionAttemptFailure) {
  ipc_client_weak_ptr_->set_wait_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::CONNECT, std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::CONNECT_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessConnectMessage_NoSessionExists) {
  ipc_client_weak_ptr_->set_wait_for_ipc_channel_return_value(false);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::CONNECT, std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::CONNECT_RESPONSE,
            writer_weak_ptr_->last_message_type());
  ASSERT_EQ(std::string(1, kConnectResponseNoSession),
            writer_weak_ptr_->last_message_payload());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessConnectMessage_IncorrectPayload) {
  ipc_client_weak_ptr_->set_wait_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::CONNECT, "Invalid request payload"));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::CONNECT_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_IpcSendSuccess) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload("I AM A VALID RESPONSE PAYLOAD!");
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::REQUEST, request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::REQUEST_RESPONSE,
            writer_weak_ptr_->last_message_type());
  ASSERT_EQ(response_payload, writer_weak_ptr_->last_message_payload());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest, ProcessRequestMessage_WriteFails) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload("I AM A VALID RESPONSE PAYLOAD!");

  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);
  writer_weak_ptr_->set_write_request_succeeded(/*should_succeed=*/false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::REQUEST, request_payload));
  WaitForOperationComplete();

  ASSERT_FALSE(ipc_client_weak_ptr_.get());
  ASSERT_FALSE(reader_weak_ptr_.get());
  ASSERT_FALSE(writer_weak_ptr_.get());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_IpcSendFailure) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::REQUEST, request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_EmptyClientResponse) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload("");
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::REQUEST, request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_ClientResponseError) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload(kRemoteSecurityKeyConnectionError);
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::REQUEST, request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest,
       ProcessRequestMessage_InvalidPayload) {
  std::string invalid_payload("");
  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::REQUEST, invalid_payload));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(RemoteSecurityKeyMessageHandlerTest, ProcessUnknownMessage) {
  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          RemoteSecurityKeyMessageType::UNKNOWN_ERROR, std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(RemoteSecurityKeyMessageType::UNKNOWN_COMMAND,
            writer_weak_ptr_->last_message_type());
}

}  // namespace remoting

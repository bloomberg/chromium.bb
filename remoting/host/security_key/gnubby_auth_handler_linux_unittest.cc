// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "remoting/host/security_key/gnubby_socket.h"
#include "remoting/proto/internal.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const char kSocketFilename[] = "socket_for_testing";

// Test gnubby request data.
const unsigned char kRequestData[] = {
    0x00, 0x00, 0x00, 0x9a, 0x65, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x60, 0x90,
    0x24, 0x71, 0xf8, 0xf2, 0xe5, 0xdf, 0x7f, 0x81, 0xc7, 0x49, 0xc4, 0xa3,
    0x58, 0x5c, 0xf6, 0xcc, 0x40, 0x14, 0x28, 0x0c, 0xa0, 0xfa, 0x03, 0x18,
    0x38, 0xd8, 0x7d, 0x77, 0x2b, 0x3a, 0x00, 0x00, 0x00, 0x20, 0x64, 0x46,
    0x47, 0x2f, 0xdf, 0x6e, 0xed, 0x7b, 0xf3, 0xc3, 0x37, 0x20, 0xf2, 0x36,
    0x67, 0x6c, 0x36, 0xe1, 0xb4, 0x5e, 0xbe, 0x04, 0x85, 0xdb, 0x89, 0xa3,
    0xcd, 0xfd, 0xd2, 0x4b, 0xd6, 0x9f, 0x00, 0x00, 0x00, 0x40, 0x38, 0x35,
    0x05, 0x75, 0x1d, 0x13, 0x6e, 0xb3, 0x6b, 0x1d, 0x29, 0xae, 0xd3, 0x43,
    0xe6, 0x84, 0x8f, 0xa3, 0x9d, 0x65, 0x4e, 0x2f, 0x57, 0xe3, 0xf6, 0xe6,
    0x20, 0x3c, 0x00, 0xc6, 0xe1, 0x73, 0x34, 0xe2, 0x23, 0x99, 0xc4, 0xfa,
    0x91, 0xc2, 0xd5, 0x97, 0xc1, 0x8b, 0xd0, 0x3c, 0x13, 0xba, 0xf0, 0xd7,
    0x5e, 0xa3, 0xbc, 0x02, 0x5b, 0xec, 0xe4, 0x4b, 0xae, 0x0e, 0xf2, 0xbd,
    0xc8, 0xaa};

}  // namespace

class GnubbyAuthHandlerLinuxTest : public testing::Test {
 public:
  GnubbyAuthHandlerLinuxTest()
      : run_loop_(new base::RunLoop()), last_connection_id_received_(-1) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.path().Append(kSocketFilename);
    remoting::GnubbyAuthHandler::SetGnubbySocketName(socket_path_);

    send_message_callback_ =
        base::Bind(&GnubbyAuthHandlerLinuxTest::SendMessageToClient,
                   base::Unretained(this));
    auth_handler_ =
        remoting::GnubbyAuthHandler::Create(nullptr, send_message_callback_);
  }

  void WaitForSendMessageToClient() {
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop);
  }

  void SendMessageToClient(int connection_id, const std::string& data) {
    last_connection_id_received_ = connection_id;
    last_message_received_ = data;
    run_loop_->Quit();
  }

  void CheckHostDataMessage(int id, const std::string& expected_data) {
    ASSERT_EQ(id, last_connection_id_received_);
    ASSERT_EQ(expected_data.length(), last_message_received_.length());
    ASSERT_EQ(expected_data, last_message_received_);
  }

  void WriteRequestData(net::UnixDomainClientSocket* client_socket) {
    int request_len = sizeof(kRequestData);
    scoped_refptr<net::DrainableIOBuffer> request_buffer(
        new net::DrainableIOBuffer(
            new net::WrappedIOBuffer(
                reinterpret_cast<const char*>(kRequestData)),
            request_len));
    net::TestCompletionCallback write_callback;
    int bytes_written = 0;
    while (bytes_written < request_len) {
      int write_result = client_socket->Write(request_buffer.get(),
                                              request_buffer->BytesRemaining(),
                                              write_callback.callback());
      write_result = write_callback.GetResult(write_result);
      ASSERT_GT(write_result, 0);
      bytes_written += write_result;
      ASSERT_LE(bytes_written, request_len);
      request_buffer->DidConsume(write_result);
    }
    ASSERT_EQ(request_len, bytes_written);
  }

  void WaitForAndVerifyHostMessage(int connection_id) {
    WaitForSendMessageToClient();
    std::string expected_data;
    expected_data.reserve(sizeof(kRequestData) - 4);

    // Skip first four bytes and build up the response string.
    for (size_t i = 4; i < sizeof(kRequestData); ++i) {
      expected_data.append(1, static_cast<unsigned char>(kRequestData[i]));
    }

    CheckHostDataMessage(connection_id, expected_data);
  }

 protected:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // Object under test.
  std::unique_ptr<GnubbyAuthHandler> auth_handler_;

  GnubbyAuthHandler::SendMessageCallback send_message_callback_;

  int last_connection_id_received_;
  std::string last_message_received_;

  base::ScopedTempDir temp_dir_;
  base::FilePath socket_path_;
  base::Closure accept_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GnubbyAuthHandlerLinuxTest);
};

TEST_F(GnubbyAuthHandlerLinuxTest, NotClosedAfterRequest) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  auth_handler_->CreateGnubbyConnection();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage(1);

  // Verify the connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Verify that completing a request/response cycle didn't close the socket.
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, HandleTwoRequests) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  auth_handler_->CreateGnubbyConnection();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage(1);

  // Verify the connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Repeat the request/response cycle.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage(1);

  // Verify the connection is still valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Verify that completing two request/response cycles didn't close the
  // socket.
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, HandleTwoIndependentRequests) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  auth_handler_->CreateGnubbyConnection();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage(1);

  // Verify the first connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Disconnect and establish a new connection.
  client_socket.Disconnect();
  rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Repeat the request/response cycle.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage(2);

  // Verify the second connection is valid and the first is not.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(2));
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(1));

  // Verify that the initial socket was released properly.
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, DidReadTimeout) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
  auth_handler_->CreateGnubbyConnection();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));
  auth_handler_->SetRequestTimeoutForTest(base::TimeDelta());
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, ClientErrorMessageDelivered) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
  auth_handler_->CreateGnubbyConnection();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  auth_handler_->SendErrorAndCloseConnection(1);
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
}

}  // namespace remoting

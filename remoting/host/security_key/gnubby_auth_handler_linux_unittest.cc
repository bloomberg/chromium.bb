// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
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
#include "remoting/host/security_key/gnubby_auth_handler_linux.h"
#include "remoting/host/security_key/gnubby_socket.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/client_stub.h"
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

class TestClientStub : public protocol::ClientStub {
 public:
  TestClientStub() : loop_(new base::RunLoop) {}
  ~TestClientStub() override {}

  // protocol::ClientStub implementation.
  void SetCapabilities(const protocol::Capabilities& capabilities) override {}

  void SetPairingResponse(
      const protocol::PairingResponse& pairing_response) override {}

  void DeliverHostMessage(const protocol::ExtensionMessage& message) override {
    message_ = message;
    loop_->Quit();
  }

  // protocol::ClipboardStub implementation.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override {}

  // protocol::CursorShapeStub implementation.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override {}

  void WaitForDeliverHostMessage() {
    loop_->Run();
    loop_.reset(new base::RunLoop);
  }

  void CheckHostDataMessage(int id, const std::string& data) {
    std::string connection_id = base::StringPrintf("\"connectionId\":%d", id);
    std::string data_message = base::StringPrintf("\"data\":%s", data.c_str());

    ASSERT_TRUE(message_.type() == "gnubby-auth" ||
                message_.type() == "auth-v1");
    ASSERT_NE(message_.data().find("\"type\":\"data\""), std::string::npos);
    ASSERT_NE(message_.data().find(connection_id), std::string::npos);
    ASSERT_NE(message_.data().find(data_message), std::string::npos);
  }

 private:
  protocol::ExtensionMessage message_;
  scoped_ptr<base::RunLoop> loop_;

  DISALLOW_COPY_AND_ASSIGN(TestClientStub);
};

class GnubbyAuthHandlerLinuxTest : public testing::Test {
 public:
  GnubbyAuthHandlerLinuxTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.path().Append(kSocketFilename);
    auth_handler_posix_.reset(new GnubbyAuthHandlerLinux(&client_stub_));
    auth_handler_ = auth_handler_posix_.get();
    auth_handler_->SetGnubbySocketName(socket_path_);
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

  void WaitForAndVerifyHostMessage() {
    client_stub_.WaitForDeliverHostMessage();
    base::ListValue expected_data;
    // Skip first four bytes.
    for (size_t i = 4; i < sizeof(kRequestData); ++i) {
      expected_data.AppendInteger(kRequestData[i]);
    }

    std::string expected_data_json;
    base::JSONWriter::Write(expected_data, &expected_data_json);
    client_stub_.CheckHostDataMessage(1, expected_data_json);
  }

 protected:
  // Object under test.
  scoped_ptr<GnubbyAuthHandlerLinux> auth_handler_posix_;

  // GnubbyAuthHandler interface for |auth_handler_posix_|.
  GnubbyAuthHandler* auth_handler_;

  base::MessageLoopForIO message_loop_;
  TestClientStub client_stub_;
  base::ScopedTempDir temp_dir_;
  base::FilePath socket_path_;
  base::Closure accept_callback_;
};

TEST_F(GnubbyAuthHandlerLinuxTest, HostDataMessageDelivered) {
  auth_handler_->DeliverHostDataMessage(42, "test_msg");
  client_stub_.WaitForDeliverHostMessage();
  // Expects a JSON array of the ASCII character codes for "test_msg".
  client_stub_.CheckHostDataMessage(42, "[116,101,115,116,95,109,115,103]");
}

TEST_F(GnubbyAuthHandlerLinuxTest, NotClosedAfterRequest) {
  ASSERT_EQ(0u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());

  const char message_json[] = "{\"type\":\"control\",\"option\":\"auth-v1\"}";
  auth_handler_->DeliverClientMessage(message_json);

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage();

  // Verify that completing a request/response cycle didn't close the socket.
  ASSERT_EQ(1u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, HandleTwoRequests) {
  ASSERT_EQ(0u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());

  const char message_json[] = "{\"type\":\"control\",\"option\":\"auth-v1\"}";
  auth_handler_->DeliverClientMessage(message_json);

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage();

  // Repeat the request/response cycle.
  WriteRequestData(&client_socket);
  WaitForAndVerifyHostMessage();

  // Verify that completing two request/response cycles didn't close the socket.
  ASSERT_EQ(1u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, DidReadTimeout) {
  std::string message_json = "{\"type\":\"control\",\"option\":\"auth-v1\"}";

  ASSERT_EQ(0u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());
  auth_handler_->DeliverClientMessage(message_json);
  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));
  auth_handler_posix_->SetRequestTimeoutForTest(base::TimeDelta());
  ASSERT_EQ(0u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());
}

TEST_F(GnubbyAuthHandlerLinuxTest, ClientErrorMessageDelivered) {
  std::string message_json = "{\"type\":\"control\",\"option\":\"auth-v1\"}";

  ASSERT_EQ(0u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());
  auth_handler_->DeliverClientMessage(message_json);
  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  std::string error_json = "{\"type\":\"error\",\"connectionId\":1}";
  auth_handler_->DeliverClientMessage(error_json);
  ASSERT_EQ(0u, auth_handler_posix_->GetActiveSocketsMapSizeForTest());
}

}  // namespace remoting

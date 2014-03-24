// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/timer/mock_timer.h"
#include "net/socket/stream_listen_socket.h"
#include "remoting/host/gnubby_auth_handler_posix.h"
#include "remoting/host/gnubby_socket.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::MockClientStub;

using testing::_;
using testing::Return;

namespace {

// Test gnubby request data.
const unsigned char request_data[] = {
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

class MockStreamListenSocket : public net::StreamListenSocket {
 public:
  explicit MockStreamListenSocket(net::StreamListenSocket::Delegate* delegate)
      : StreamListenSocket(net::kInvalidSocket, delegate) {}

  virtual void Accept() OVERRIDE { NOTREACHED(); }

 private:
  virtual ~MockStreamListenSocket() {}
};

}  // namespace

class GnubbyAuthHandlerPosixTest : public testing::Test {
 public:
  GnubbyAuthHandlerPosixTest() {}

  virtual void SetUp() OVERRIDE;

 protected:
  // Object under test.
  scoped_ptr<GnubbyAuthHandlerPosix> auth_handler_posix_;

  // GnubbyAuthHandler interface for |auth_handler_posix_|.
  GnubbyAuthHandler* auth_handler_;

  // Stream delegate interface for |auth_handler_posix_|.
  net::StreamListenSocket::Delegate* delegate_;

  // Mock client stub.
  MockClientStub client_stub_;

  base::MessageLoop message_loop_;

 private:
  void OnConnect(int result);
};

void GnubbyAuthHandlerPosixTest::SetUp() {
  auth_handler_posix_.reset(new GnubbyAuthHandlerPosix(&client_stub_));
  auth_handler_ = auth_handler_posix_.get();
  delegate_ = auth_handler_posix_.get();
}

MATCHER_P2(EqualsDataMessage, id, data, "") {
  std::string connection_id = base::StringPrintf("\"connectionId\":%d", id);
  std::string data_message = base::StringPrintf("\"data\":%s", data);

  return (arg.type() == "gnubby-auth" &&
          arg.data().find("\"type\":\"data\"") != std::string::npos &&
          arg.data().find(connection_id) != std::string::npos &&
          arg.data().find(data_message) != std::string::npos);
}

TEST_F(GnubbyAuthHandlerPosixTest, HostDataMessageDelivered) {
  // Expects a JSON array of the ASCII character codes for "test_msg".
  EXPECT_CALL(client_stub_,
              DeliverHostMessage(
                  EqualsDataMessage(42, "[116,101,115,116,95,109,115,103]")));

  auth_handler_->DeliverHostDataMessage(42, "test_msg");
}

TEST_F(GnubbyAuthHandlerPosixTest, DidClose) {
  net::StreamListenSocket* socket = new MockStreamListenSocket(delegate_);

  delegate_->DidAccept(NULL, make_scoped_ptr(socket));
  ASSERT_TRUE(auth_handler_posix_->HasActiveSocketForTesting(socket));

  delegate_->DidClose(socket);
  ASSERT_FALSE(auth_handler_posix_->HasActiveSocketForTesting(socket));
}

TEST_F(GnubbyAuthHandlerPosixTest, DidRead) {
  EXPECT_CALL(client_stub_, DeliverHostMessage(_));

  net::StreamListenSocket* socket = new MockStreamListenSocket(delegate_);

  delegate_->DidAccept(NULL, make_scoped_ptr(socket));
  delegate_->DidRead(socket,
                     reinterpret_cast<const char*>(request_data),
                     sizeof(request_data));
}

TEST_F(GnubbyAuthHandlerPosixTest, DidReadByteByByte) {
  EXPECT_CALL(client_stub_, DeliverHostMessage(_));

  net::StreamListenSocket* socket = new MockStreamListenSocket(delegate_);

  delegate_->DidAccept(NULL, make_scoped_ptr(socket));
  for (unsigned int i = 0; i < sizeof(request_data); ++i) {
    delegate_->DidRead(
        socket, reinterpret_cast<const char*>(request_data + i), 1);
  }
}

TEST_F(GnubbyAuthHandlerPosixTest, DidReadTimeout) {
  net::StreamListenSocket* socket = new MockStreamListenSocket(delegate_);

  delegate_->DidAccept(NULL, make_scoped_ptr(socket));
  ASSERT_TRUE(auth_handler_posix_->HasActiveSocketForTesting(socket));

  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  auth_handler_posix_->GetGnubbySocketForTesting(socket)
      ->SetTimerForTesting(scoped_ptr<base::Timer>(mock_timer));
  delegate_->DidRead(socket, reinterpret_cast<const char*>(request_data), 1);
  mock_timer->Fire();

  ASSERT_FALSE(auth_handler_posix_->HasActiveSocketForTesting(socket));
}

TEST_F(GnubbyAuthHandlerPosixTest, ClientErrorMessageDelivered) {
  net::StreamListenSocket* socket = new MockStreamListenSocket(delegate_);

  delegate_->DidAccept(NULL, make_scoped_ptr(socket));

  std::string error_json = base::StringPrintf(
      "{\"type\":\"error\",\"connectionId\":%d}",
      auth_handler_posix_->GetConnectionIdForTesting(socket));

  ASSERT_TRUE(auth_handler_posix_->HasActiveSocketForTesting(socket));
  auth_handler_->DeliverClientMessage(error_json);
  ASSERT_FALSE(auth_handler_posix_->HasActiveSocketForTesting(socket));
}

}  // namespace remoting

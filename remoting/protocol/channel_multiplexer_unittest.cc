// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_multiplexer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;
using testing::InvokeWithoutArgs;

namespace remoting {
namespace protocol {

namespace {

const int kMessageSize = 1024;
const int kMessages = 100;
const char kMuxChannelName[] = "mux";

const char kTestChannelName[] = "test";
const char kTestChannelName2[] = "test2";


void QuitCurrentThread() {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

class MockSocketCallback {
 public:
  MOCK_METHOD1(OnDone, void(int result));
};

class MockConnectCallback {
 public:
  MOCK_METHOD1(OnConnectedPtr, void(net::StreamSocket* socket));
  void OnConnected(scoped_ptr<net::StreamSocket> socket) {
    OnConnectedPtr(socket.release());
  }
};

}  // namespace

class ChannelMultiplexerTest : public testing::Test {
 public:
  void DeleteAll() {
    host_socket1_.reset();
    host_socket2_.reset();
    client_socket1_.reset();
    client_socket2_.reset();
    host_mux_.reset();
    client_mux_.reset();
  }

  void DeleteAfterSessionFail() {
    host_mux_->CancelChannelCreation(kTestChannelName2);
    DeleteAll();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // Create pair of multiplexers and connect them to each other.
    host_mux_.reset(new ChannelMultiplexer(&host_session_, kMuxChannelName));
    client_mux_.reset(new ChannelMultiplexer(&client_session_,
                                             kMuxChannelName));
  }

  // Connect sockets to each other. Must be called after we've created at least
  // one channel with each multiplexer.
  void ConnectSockets() {
    FakeSocket* host_socket =
        host_session_.GetStreamChannel(ChannelMultiplexer::kMuxChannelName);
    FakeSocket* client_socket =
        client_session_.GetStreamChannel(ChannelMultiplexer::kMuxChannelName);
    host_socket->PairWith(client_socket);

    // Make writes asynchronous in one direction.
    host_socket->set_async_write(true);
  }

  void CreateChannel(const std::string& name,
                     scoped_ptr<net::StreamSocket>* host_socket,
                     scoped_ptr<net::StreamSocket>* client_socket) {
    int counter = 2;
    host_mux_->CreateStreamChannel(name, base::Bind(
        &ChannelMultiplexerTest::OnChannelConnected, base::Unretained(this),
        host_socket, &counter));
    client_mux_->CreateStreamChannel(name, base::Bind(
        &ChannelMultiplexerTest::OnChannelConnected, base::Unretained(this),
        client_socket, &counter));

    message_loop_.Run();

    EXPECT_TRUE(host_socket->get());
    EXPECT_TRUE(client_socket->get());
  }

  void OnChannelConnected(
      scoped_ptr<net::StreamSocket>* storage,
      int* counter,
      scoped_ptr<net::StreamSocket> socket) {
    *storage = socket.Pass();
    --(*counter);
    EXPECT_GE(*counter, 0);
    if (*counter == 0)
      QuitCurrentThread();
  }

  scoped_refptr<net::IOBufferWithSize> CreateTestBuffer(int size) {
    scoped_refptr<net::IOBufferWithSize> result =
        new net::IOBufferWithSize(size);
    for (int i = 0; i< size; ++i) {
      result->data()[i] = rand() % 256;
    }
    return result;
  }

  base::MessageLoop message_loop_;

  FakeSession host_session_;
  FakeSession client_session_;

  scoped_ptr<ChannelMultiplexer> host_mux_;
  scoped_ptr<ChannelMultiplexer> client_mux_;

  scoped_ptr<net::StreamSocket> host_socket1_;
  scoped_ptr<net::StreamSocket> client_socket1_;
  scoped_ptr<net::StreamSocket> host_socket2_;
  scoped_ptr<net::StreamSocket> client_socket2_;
};


TEST_F(ChannelMultiplexerTest, OneChannel) {
  scoped_ptr<net::StreamSocket> host_socket;
  scoped_ptr<net::StreamSocket> client_socket;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket, &client_socket));

  ConnectSockets();

  StreamConnectionTester tester(host_socket.get(), client_socket.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

TEST_F(ChannelMultiplexerTest, TwoChannels) {
  scoped_ptr<net::StreamSocket> host_socket1_;
  scoped_ptr<net::StreamSocket> client_socket1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  scoped_ptr<net::StreamSocket> host_socket2_;
  scoped_ptr<net::StreamSocket> client_socket2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  ConnectSockets();

  StreamConnectionTester tester1(host_socket1_.get(), client_socket1_.get(),
                                kMessageSize, kMessages);
  StreamConnectionTester tester2(host_socket2_.get(), client_socket2_.get(),
                                 kMessageSize, kMessages);
  tester1.Start();
  tester2.Start();
  while (!tester1.done() || !tester2.done()) {
    message_loop_.Run();
  }
  tester1.CheckResults();
  tester2.CheckResults();
}

// Four channels, two in each direction
TEST_F(ChannelMultiplexerTest, FourChannels) {
  scoped_ptr<net::StreamSocket> host_socket1_;
  scoped_ptr<net::StreamSocket> client_socket1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  scoped_ptr<net::StreamSocket> host_socket2_;
  scoped_ptr<net::StreamSocket> client_socket2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  scoped_ptr<net::StreamSocket> host_socket3;
  scoped_ptr<net::StreamSocket> client_socket3;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel("test3", &host_socket3, &client_socket3));

  scoped_ptr<net::StreamSocket> host_socket4;
  scoped_ptr<net::StreamSocket> client_socket4;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel("ch4", &host_socket4, &client_socket4));

  ConnectSockets();

  StreamConnectionTester tester1(host_socket1_.get(), client_socket1_.get(),
                                kMessageSize, kMessages);
  StreamConnectionTester tester2(host_socket2_.get(), client_socket2_.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester3(client_socket3.get(), host_socket3.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester4(client_socket4.get(), host_socket4.get(),
                                 kMessageSize, kMessages);
  tester1.Start();
  tester2.Start();
  tester3.Start();
  tester4.Start();
  while (!tester1.done() || !tester2.done() ||
         !tester3.done() || !tester4.done()) {
    message_loop_.Run();
  }
  tester1.CheckResults();
  tester2.CheckResults();
  tester3.CheckResults();
  tester4.CheckResults();
}

TEST_F(ChannelMultiplexerTest, WriteFailSync) {
  scoped_ptr<net::StreamSocket> host_socket1_;
  scoped_ptr<net::StreamSocket> client_socket1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  scoped_ptr<net::StreamSocket> host_socket2_;
  scoped_ptr<net::StreamSocket> client_socket2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  ConnectSockets();

  host_session_.GetStreamChannel(kMuxChannelName)->
      set_next_write_error(net::ERR_FAILED);
  host_session_.GetStreamChannel(kMuxChannelName)->
      set_async_write(false);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);

  MockSocketCallback cb1;
  MockSocketCallback cb2;

  EXPECT_CALL(cb1, OnDone(_))
      .Times(0);
  EXPECT_CALL(cb2, OnDone(_))
      .Times(0);

  EXPECT_EQ(net::ERR_FAILED,
            host_socket1_->Write(buf.get(),
                                 buf->size(),
                                 base::Bind(&MockSocketCallback::OnDone,
                                            base::Unretained(&cb1))));
  EXPECT_EQ(net::ERR_FAILED,
            host_socket2_->Write(buf.get(),
                                 buf->size(),
                                 base::Bind(&MockSocketCallback::OnDone,
                                            base::Unretained(&cb2))));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ChannelMultiplexerTest, WriteFailAsync) {
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  ConnectSockets();

  host_session_.GetStreamChannel(kMuxChannelName)->
      set_next_write_error(net::ERR_FAILED);
  host_session_.GetStreamChannel(kMuxChannelName)->
      set_async_write(true);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);

  MockSocketCallback cb1;
  MockSocketCallback cb2;
  EXPECT_CALL(cb1, OnDone(net::ERR_FAILED));
  EXPECT_CALL(cb2, OnDone(net::ERR_FAILED));

  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket1_->Write(buf.get(),
                                 buf->size(),
                                 base::Bind(&MockSocketCallback::OnDone,
                                            base::Unretained(&cb1))));
  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket2_->Write(buf.get(),
                                 buf->size(),
                                 base::Bind(&MockSocketCallback::OnDone,
                                            base::Unretained(&cb2))));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ChannelMultiplexerTest, DeleteWhenFailed) {
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  ConnectSockets();

  host_session_.GetStreamChannel(kMuxChannelName)->
      set_next_write_error(net::ERR_FAILED);
  host_session_.GetStreamChannel(kMuxChannelName)->
      set_async_write(true);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);

  MockSocketCallback cb1;
  MockSocketCallback cb2;

  EXPECT_CALL(cb1, OnDone(net::ERR_FAILED))
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(this, &ChannelMultiplexerTest::DeleteAll));
  EXPECT_CALL(cb2, OnDone(net::ERR_FAILED))
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(this, &ChannelMultiplexerTest::DeleteAll));

  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket1_->Write(buf.get(),
                                 buf->size(),
                                 base::Bind(&MockSocketCallback::OnDone,
                                            base::Unretained(&cb1))));
  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket2_->Write(buf.get(),
                                 buf->size(),
                                 base::Bind(&MockSocketCallback::OnDone,
                                            base::Unretained(&cb2))));

  base::RunLoop().RunUntilIdle();

  // Check that the sockets were destroyed.
  EXPECT_FALSE(host_mux_.get());
}

TEST_F(ChannelMultiplexerTest, SessionFail) {
  host_session_.set_async_creation(true);
  host_session_.set_error(AUTHENTICATION_FAILED);

  MockConnectCallback cb1;
  MockConnectCallback cb2;

  host_mux_->CreateStreamChannel(kTestChannelName, base::Bind(
      &MockConnectCallback::OnConnected, base::Unretained(&cb1)));
  host_mux_->CreateStreamChannel(kTestChannelName2, base::Bind(
      &MockConnectCallback::OnConnected, base::Unretained(&cb2)));

  EXPECT_CALL(cb1, OnConnectedPtr(NULL))
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(
          this, &ChannelMultiplexerTest::DeleteAfterSessionFail));
  EXPECT_CALL(cb2, OnConnectedPtr(_))
      .Times(0);

  base::RunLoop().RunUntilIdle();
}

}  // namespace protocol
}  // namespace remoting

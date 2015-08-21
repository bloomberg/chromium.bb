// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/quic_channel_factory.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/quic/p2p/quic_p2p_session.h"
#include "net/quic/p2p/quic_p2p_stream.h"
#include "net/socket/socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_datagram_socket.h"
#include "remoting/protocol/p2p_stream_socket.h"
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

const char kTestSessionId[] = "123123";
const char kTestChannelName[] = "test";
const char kTestChannelName2[] = "test2";

}  // namespace

class QuicChannelFactoryTest : public testing::Test,
                               public testing::WithParamInterface<bool> {
 public:
  void DeleteAll() {
    host_channel1_.reset();
    host_channel2_.reset();
    client_channel1_.reset();
    client_channel2_.reset();
    host_quic_.reset();
    client_quic_.reset();
  }

  void FailedReadDeleteAll(int result) {
    EXPECT_NE(net::OK, result);
    DeleteAll();
  }

  void OnChannelConnected(scoped_ptr<P2PStreamSocket>* storage,
                          int* counter,
                          base::RunLoop* run_loop,
                          scoped_ptr<P2PStreamSocket> socket) {
    *storage = socket.Pass();
    if (counter) {
      --(*counter);
      EXPECT_GE(*counter, 0);
      if (*counter == 0)
        run_loop->Quit();
    }
  }

  void OnChannelConnectedExpectFail(scoped_ptr<P2PStreamSocket> socket) {
    EXPECT_FALSE(socket);
    host_quic_->CancelChannelCreation(kTestChannelName2);
    DeleteAll();
  }

  void OnChannelConnectedNotReached(scoped_ptr<P2PStreamSocket> socket) {
    NOTREACHED();
  }

 protected:
  void TearDown() override {
    DeleteAll();
    // QuicChannelFactory destroys the internals asynchronously. Run all pending
    // tasks to avoid leaking memory.
    base::RunLoop().RunUntilIdle();
  }

  void Initialize() {
    host_base_channel_factory_.PairWith(&client_base_channel_factory_);
    host_base_channel_factory_.set_asynchronous_create(GetParam());
    client_base_channel_factory_.set_asynchronous_create(GetParam());

    host_quic_.reset(new QuicChannelFactory(kTestSessionId, true));
    client_quic_.reset(new QuicChannelFactory(kTestSessionId, false));

    session_initiate_message_ =
        client_quic_->CreateSessionInitiateConfigMessage();
    EXPECT_TRUE(host_quic_->ProcessSessionInitiateConfigMessage(
        session_initiate_message_));

    session_accept_message_ = host_quic_->CreateSessionAcceptConfigMessage();
    EXPECT_TRUE(client_quic_->ProcessSessionAcceptConfigMessage(
        session_accept_message_));

    const char kTestSharedSecret[] = "Shared Secret";
    host_quic_->Start(&host_base_channel_factory_, kTestSharedSecret);
    client_quic_->Start(&client_base_channel_factory_, kTestSharedSecret);

    FakeDatagramSocket* host_base_channel =
        host_base_channel_factory_.GetFakeChannel(kQuicChannelName);
    if (host_base_channel)
      host_base_channel->set_async_send(GetParam());

    FakeDatagramSocket* client_base_channel =
        client_base_channel_factory_.GetFakeChannel(kQuicChannelName);
    if (client_base_channel)
      client_base_channel->set_async_send(GetParam());
  }

  void CreateChannel(const std::string& name,
                     scoped_ptr<P2PStreamSocket>* host_channel,
                     scoped_ptr<P2PStreamSocket>* client_channel) {
    int counter = 2;
    base::RunLoop run_loop;
    host_quic_->CreateChannel(
        name,
        base::Bind(&QuicChannelFactoryTest::OnChannelConnected,
                   base::Unretained(this), host_channel, &counter, &run_loop));
    client_quic_->CreateChannel(
        name, base::Bind(&QuicChannelFactoryTest::OnChannelConnected,
                         base::Unretained(this), client_channel, &counter,
                         &run_loop));

    run_loop.Run();

    EXPECT_TRUE(host_channel->get());
    EXPECT_TRUE(client_channel->get());
  }

  scoped_refptr<net::IOBufferWithSize> CreateTestBuffer(int size) {
    scoped_refptr<net::IOBufferWithSize> result =
        new net::IOBufferWithSize(size);
    for (int i = 0; i < size; ++i) {
      result->data()[i] = rand() % 256;
    }
    return result;
  }

  base::MessageLoop message_loop_;

  FakeDatagramChannelFactory host_base_channel_factory_;
  FakeDatagramChannelFactory client_base_channel_factory_;

  scoped_ptr<QuicChannelFactory> host_quic_;
  scoped_ptr<QuicChannelFactory> client_quic_;

  scoped_ptr<P2PStreamSocket> host_channel1_;
  scoped_ptr<P2PStreamSocket> client_channel1_;
  scoped_ptr<P2PStreamSocket> host_channel2_;
  scoped_ptr<P2PStreamSocket> client_channel2_;

  std::string session_initiate_message_;
  std::string session_accept_message_;
};

INSTANTIATE_TEST_CASE_P(SyncWrite,
                        QuicChannelFactoryTest,
                        ::testing::Values(false));
INSTANTIATE_TEST_CASE_P(AsyncWrite,
                        QuicChannelFactoryTest,
                        ::testing::Values(true));

TEST_P(QuicChannelFactoryTest, OneChannel) {
  Initialize();

  scoped_ptr<P2PStreamSocket> host_channel;
  scoped_ptr<P2PStreamSocket> client_channel;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_channel, &client_channel));

  StreamConnectionTester tester(host_channel.get(), client_channel.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

TEST_P(QuicChannelFactoryTest, TwoChannels) {
  Initialize();

  scoped_ptr<P2PStreamSocket> host_channel1_;
  scoped_ptr<P2PStreamSocket> client_channel1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_channel1_, &client_channel1_));

  scoped_ptr<P2PStreamSocket> host_channel2_;
  scoped_ptr<P2PStreamSocket> client_channel2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_channel2_, &client_channel2_));

  StreamConnectionTester tester1(host_channel1_.get(), client_channel1_.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester2(host_channel2_.get(), client_channel2_.get(),
                                 kMessageSize, kMessages);
  tester1.Start();
  tester2.Start();
  while (!tester1.done() || !tester2.done()) {
    message_loop_.Run();
  }
  tester1.CheckResults();
  tester2.CheckResults();
}

TEST_P(QuicChannelFactoryTest, SendFail) {
  Initialize();

  scoped_ptr<P2PStreamSocket> host_channel1_;
  scoped_ptr<P2PStreamSocket> client_channel1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_channel1_, &client_channel1_));

  scoped_ptr<P2PStreamSocket> host_channel2_;
  scoped_ptr<P2PStreamSocket> client_channel2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_channel2_, &client_channel2_));

  host_base_channel_factory_.GetFakeChannel(kQuicChannelName)
      ->set_next_send_error(net::ERR_FAILED);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);


  // Try writing to a channel. This should result in all stream being closed due
  // to an error.
  {
    net::TestCompletionCallback write_cb_1;
    host_channel1_->Write(buf.get(), buf->size(), write_cb_1.callback());
    base::RunLoop().RunUntilIdle();
  }

  // Repeated attempt to write should result in an error.
  {
    net::TestCompletionCallback write_cb_1;
    net::TestCompletionCallback write_cb_2;
    EXPECT_NE(net::OK, host_channel1_->Write(buf.get(), buf->size(),
                                             write_cb_1.callback()));
    EXPECT_FALSE(write_cb_1.have_result());
    EXPECT_NE(net::OK, host_channel1_->Write(buf.get(), buf->size(),
                                             write_cb_2.callback()));
    EXPECT_FALSE(write_cb_2.have_result());
  }
}

TEST_P(QuicChannelFactoryTest, DeleteWhenFailed) {
  Initialize();

  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_channel1_, &client_channel1_));
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_channel2_, &client_channel2_));

  host_base_channel_factory_.GetFakeChannel(kQuicChannelName)
      ->set_next_send_error(net::ERR_FAILED);

  scoped_refptr<net::IOBufferWithSize> read_buf =
      new net::IOBufferWithSize(100);

  EXPECT_EQ(net::ERR_IO_PENDING,
            host_channel1_->Read(
                read_buf.get(), read_buf->size(),
                base::Bind(&QuicChannelFactoryTest::FailedReadDeleteAll,
                           base::Unretained(this))));

  // Try writing to a channel. This should result it DeleteAll() called and the
  // connection torn down.
  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);
  net::TestCompletionCallback write_cb_1;
  host_channel1_->Write(buf.get(), buf->size(), write_cb_1.callback());

  base::RunLoop().RunUntilIdle();

  // Check that the connection was torn down.
  EXPECT_FALSE(host_quic_);
}

TEST_P(QuicChannelFactoryTest, SessionFail) {
  host_base_channel_factory_.set_fail_create(true);
  Initialize();

  host_quic_->CreateChannel(
      kTestChannelName,
      base::Bind(&QuicChannelFactoryTest::OnChannelConnectedExpectFail,
                 base::Unretained(this)));

  // host_quic_ may be destroyed at this point in sync mode.
  if (host_quic_) {
    host_quic_->CreateChannel(
        kTestChannelName2,
        base::Bind(&QuicChannelFactoryTest::OnChannelConnectedNotReached,
                   base::Unretained(this)));
  }

  base::RunLoop().RunUntilIdle();

  // Check that DeleteAll() was called and the connection was torn down.
  EXPECT_FALSE(host_quic_);
}

// Verify that the host just ignores incoming stream with unexpected name.
TEST_P(QuicChannelFactoryTest, UnknownName) {
  Initialize();

  // Create a new channel from the client side.
  client_quic_->CreateChannel(
      kTestChannelName, base::Bind(&QuicChannelFactoryTest::OnChannelConnected,
                                   base::Unretained(this), &client_channel1_,
                                   nullptr, nullptr));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0U, host_quic_->GetP2PSessionForTests()->GetNumOpenStreams());
}

// Verify that incoming streams that have received only partial name are
// destroyed correctly.
TEST_P(QuicChannelFactoryTest, SendPartialName) {
  Initialize();

  base::RunLoop().RunUntilIdle();

  net::QuicP2PSession* session = client_quic_->GetP2PSessionForTests();
  net::QuicP2PStream* stream = session->CreateOutgoingDynamicStream();

  std::string name = kTestChannelName;
  // Send only half of the name to the host.
  stream->WriteHeader(std::string(1, static_cast<char>(name.size())) +
                      name.substr(0, name.size() / 2));

  base::RunLoop().RunUntilIdle();

  // Host should have received the new stream and is still waiting for the name.
  EXPECT_EQ(1U, host_quic_->GetP2PSessionForTests()->GetNumOpenStreams());

  session->CloseStream(stream->id());
  base::RunLoop().RunUntilIdle();

  // Verify that the stream was closed on the host side.
  EXPECT_EQ(0U, host_quic_->GetP2PSessionForTests()->GetNumOpenStreams());

  // Create another stream with only partial name and tear down connection while
  // it's still pending.
  stream = session->CreateOutgoingDynamicStream();
  stream->WriteHeader(std::string(1, static_cast<char>(name.size())) +
                      name.substr(0, name.size() / 2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, host_quic_->GetP2PSessionForTests()->GetNumOpenStreams());
}

// Verify that correct HKDF input suffix is used to generate encryption keys.
TEST_P(QuicChannelFactoryTest, HkdfInputSuffix) {
  Initialize();
  base::RunLoop().RunUntilIdle();

  net::QuicCryptoStream* crypto_stream =
      reinterpret_cast<net::QuicCryptoStream*>(
          host_quic_->GetP2PSessionForTests()->GetStream(net::kCryptoStreamId));
  const std::string& suffix =
      crypto_stream->crypto_negotiated_params().hkdf_input_suffix;
  EXPECT_EQ(strlen(kTestSessionId) + 1 + strlen(kQuicChannelName) + 1 +
                session_initiate_message_.size() +
                session_accept_message_.size(),
            suffix.size());
  EXPECT_EQ(std::string(kTestSessionId) + '\0' + kQuicChannelName + '\0' +
                session_initiate_message_ + session_accept_message_,
            suffix);
}

}  // namespace protocol
}  // namespace remoting

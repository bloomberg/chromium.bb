// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_session.h"

#include "build/build_config.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_clock.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string_utils.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_test_mem_slice_vector.h"
#include "net/third_party/quic/quartc/counting_packet_filter.h"
#include "net/third_party/quic/quartc/quartc_factory.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"
#include "net/third_party/quic/quartc/simulated_packet_transport.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/simulator/packet_filter.h"
#include "net/third_party/quic/test_tools/simulator/simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests flaky on iOS.
// TODO(vasilvv): figure out what's wrong and re-enable if possible.
#if !defined(OS_IOS)
namespace quic {

namespace {

static QuicByteCount kDefaultMaxPacketSize = 1200;

class FakeQuartcSessionDelegate : public QuartcSession::Delegate {
 public:
  explicit FakeQuartcSessionDelegate(QuartcStream::Delegate* stream_delegate,
                                     const QuicClock* clock)
      : stream_delegate_(stream_delegate), clock_(clock) {}

  void OnConnectionWritable() override {
    LOG(INFO) << "Connection writable!";
    if (!writable_time_.IsInitialized()) {
      writable_time_ = clock_->Now();
    }
  }

  // Called when peers have established forward-secure encryption
  void OnCryptoHandshakeComplete() override {
    LOG(INFO) << "Crypto handshake complete!";
    crypto_handshake_time_ = clock_->Now();
  }

  // Called when connection closes locally, or remotely by peer.
  void OnConnectionClosed(QuicErrorCode error_code,
                          const QuicString& error_details,
                          ConnectionCloseSource source) override {
    connected_ = false;
  }

  // Called when an incoming QUIC stream is created.
  void OnIncomingStream(QuartcStream* quartc_stream) override {
    last_incoming_stream_ = quartc_stream;
    last_incoming_stream_->SetDelegate(stream_delegate_);
  }

  void OnMessageReceived(QuicStringPiece message) override {
    incoming_messages_.emplace_back(message);
  }

  void OnCongestionControlChange(QuicBandwidth bandwidth_estimate,
                                 QuicBandwidth pacing_rate,
                                 QuicTime::Delta latest_rtt) override {}

  QuartcStream* last_incoming_stream() { return last_incoming_stream_; }

  // Returns all received messages.
  const std::vector<QuicString>& incoming_messages() {
    return incoming_messages_;
  }

  bool connected() { return connected_; }
  QuicTime writable_time() const { return writable_time_; }
  QuicTime crypto_handshake_time() const { return crypto_handshake_time_; }

 private:
  QuartcStream* last_incoming_stream_;
  std::vector<QuicString> incoming_messages_;
  bool connected_ = true;
  QuartcStream::Delegate* stream_delegate_;
  QuicTime writable_time_ = QuicTime::Zero();
  QuicTime crypto_handshake_time_ = QuicTime::Zero();
  const QuicClock* clock_;
};

class FakeQuartcStreamDelegate : public QuartcStream::Delegate {
 public:
  size_t OnReceived(QuartcStream* stream,
                    iovec* iov,
                    size_t iov_length,
                    bool fin) override {
    size_t bytes_consumed = 0;
    for (size_t i = 0; i < iov_length; ++i) {
      received_data_[stream->id()] +=
          QuicString(static_cast<const char*>(iov[i].iov_base), iov[i].iov_len);
      bytes_consumed += iov[i].iov_len;
    }
    return bytes_consumed;
  }

  void OnClose(QuartcStream* stream) override {
    errors_[stream->id()] = stream->stream_error();
  }

  void OnBufferChanged(QuartcStream* stream) override {}

  bool has_data() { return !received_data_.empty(); }
  std::map<QuicStreamId, QuicString> data() { return received_data_; }

  QuicRstStreamErrorCode stream_error(QuicStreamId id) { return errors_[id]; }

 private:
  std::map<QuicStreamId, QuicString> received_data_;
  std::map<QuicStreamId, QuicRstStreamErrorCode> errors_;
};

class QuartcSessionTest : public QuicTest {
 public:
  ~QuartcSessionTest() override {}

  void Init() {
    client_transport_ =
        QuicMakeUnique<simulator::SimulatedQuartcPacketTransport>(
            &simulator_, "client_transport", "server_transport",
            10 * kDefaultMaxPacketSize);
    server_transport_ =
        QuicMakeUnique<simulator::SimulatedQuartcPacketTransport>(
            &simulator_, "server_transport", "client_transport",
            10 * kDefaultMaxPacketSize);

    client_filter_ = QuicMakeUnique<simulator::CountingPacketFilter>(
        &simulator_, "client_filter", client_transport_.get());

    client_server_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        client_filter_.get(), server_transport_.get(),
        QuicBandwidth::FromKBitsPerSecond(10 * 1000),
        QuicTime::Delta::FromMilliseconds(10));

    client_stream_delegate_ = QuicMakeUnique<FakeQuartcStreamDelegate>();
    client_session_delegate_ = QuicMakeUnique<FakeQuartcSessionDelegate>(
        client_stream_delegate_.get(), simulator_.GetClock());

    server_stream_delegate_ = QuicMakeUnique<FakeQuartcStreamDelegate>();
    server_session_delegate_ = QuicMakeUnique<FakeQuartcSessionDelegate>(
        server_stream_delegate_.get(), simulator_.GetClock());

    QuartcFactoryConfig factory_config;
    factory_config.alarm_factory = simulator_.GetAlarmFactory();
    factory_config.clock = simulator_.GetClock();
    quartc_factory_ = QuicMakeUnique<QuartcFactory>(factory_config);
  }

  // Note that input session config will apply to both server and client.
  // Perspective and packet_transport will be overwritten.
  void CreateClientAndServerSessions(
      const QuartcSessionConfig& session_config) {
    Init();

    QuartcSessionConfig client_session_config = session_config;
    client_session_config.perspective = Perspective::IS_CLIENT;
    client_session_config.packet_transport = client_transport_.get();
    client_peer_ = quartc_factory_->CreateQuartcSession(client_session_config);
    client_peer_->SetDelegate(client_session_delegate_.get());

    QuartcSessionConfig server_session_config = session_config;
    server_session_config.perspective = Perspective::IS_SERVER;
    server_session_config.packet_transport = server_transport_.get();
    server_peer_ = quartc_factory_->CreateQuartcSession(server_session_config);
    server_peer_->SetDelegate(server_session_delegate_.get());
  }

  // Runs all tasks scheduled in the next 200 ms.
  void RunTasks() { simulator_.RunFor(QuicTime::Delta::FromMilliseconds(200)); }

  void StartHandshake() {
    server_peer_->StartCryptoHandshake();
    client_peer_->StartCryptoHandshake();
    RunTasks();
  }

  // Test handshake establishment and sending/receiving of data for two
  // directions.
  void TestSendReceiveStreams() {
    ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());
    ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
    ASSERT_TRUE(server_peer_->IsEncryptionEstablished());
    ASSERT_TRUE(client_peer_->IsEncryptionEstablished());

    // Now we can establish encrypted outgoing stream.
    QuartcStream* outgoing_stream =
        server_peer_->CreateOutgoingBidirectionalStream();
    QuicStreamId stream_id = outgoing_stream->id();
    ASSERT_NE(nullptr, outgoing_stream);
    EXPECT_TRUE(server_peer_->HasOpenDynamicStreams());

    outgoing_stream->SetDelegate(server_stream_delegate_.get());

    // Send a test message from peer 1 to peer 2.
    char kTestMessage[] = "Hello";
    test::QuicTestMemSliceVector data(
        {std::make_pair(kTestMessage, strlen(kTestMessage))});
    outgoing_stream->WriteMemSlices(data.span(), /*fin=*/false);
    RunTasks();

    // Wait for peer 2 to receive messages.
    ASSERT_TRUE(client_stream_delegate_->has_data());

    QuartcStream* incoming = client_session_delegate_->last_incoming_stream();
    ASSERT_TRUE(incoming);
    EXPECT_EQ(incoming->id(), stream_id);
    EXPECT_TRUE(client_peer_->HasOpenDynamicStreams());

    EXPECT_EQ(client_stream_delegate_->data()[stream_id], kTestMessage);
    // Send a test message from peer 2 to peer 1.
    char kTestResponse[] = "Response";
    test::QuicTestMemSliceVector response(
        {std::make_pair(kTestResponse, strlen(kTestResponse))});
    incoming->WriteMemSlices(response.span(), /*fin=*/false);
    RunTasks();
    // Wait for peer 1 to receive messages.
    ASSERT_TRUE(server_stream_delegate_->has_data());

    EXPECT_EQ(server_stream_delegate_->data()[stream_id], kTestResponse);
  }

  // Test sending/receiving of messages for two directions.
  void TestSendReceiveMessage() {
    ASSERT_TRUE(server_peer_->CanSendMessage());
    ASSERT_TRUE(client_peer_->CanSendMessage());

    // Send message from peer 1 to peer 2.
    ASSERT_TRUE(server_peer_->SendOrQueueMessage("Message from server"));

    // First message in each direction should not be queued.
    EXPECT_EQ(server_peer_->send_message_queue_size(), 0u);

    // Wait for peer 2 to receive message.
    RunTasks();

    EXPECT_THAT(client_session_delegate_->incoming_messages(),
                testing::ElementsAre("Message from server"));

    // Send message from peer 2 to peer 1.
    ASSERT_TRUE(client_peer_->SendOrQueueMessage("Message from client"));

    // First message in each direction should not be queued.
    EXPECT_EQ(client_peer_->send_message_queue_size(), 0u);

    // Wait for peer 1 to receive message.
    RunTasks();

    EXPECT_THAT(server_session_delegate_->incoming_messages(),
                testing::ElementsAre("Message from client"));
  }

  // Test for sending multiple messages that also result in queueing.
  // This is one-way test, which is run in given direction.
  void TestSendReceiveQueuedMessages(bool direction_from_server) {
    // Send until queue_size number of messages are queued.
    constexpr size_t queue_size = 10;

    ASSERT_TRUE(server_peer_->CanSendMessage());
    ASSERT_TRUE(client_peer_->CanSendMessage());

    QuartcSession* const peer_sending =
        direction_from_server ? server_peer_.get() : client_peer_.get();

    FakeQuartcSessionDelegate* const delegate_receiving =
        direction_from_server ? client_session_delegate_.get()
                              : server_session_delegate_.get();

    // There should be no messages in the queue before we start sending.
    EXPECT_EQ(peer_sending->send_message_queue_size(), 0u);

    // Send messages from peer 1 to peer 2 until required number of messages
    // are queued in unsent message queue.
    std::vector<QuicString> sent_messages;
    while (peer_sending->send_message_queue_size() < queue_size) {
      sent_messages.push_back(
          QuicStrCat("Sending message, index=", sent_messages.size()));
      ASSERT_TRUE(peer_sending->SendOrQueueMessage(sent_messages.back()));
    }

    // Wait for peer 2 to receive all messages.
    RunTasks();

    EXPECT_EQ(delegate_receiving->incoming_messages(), sent_messages);
  }

  // Test sending long messages:
  // - message of maximum allowed length should succeed
  // - message of > maximum allowed length should fail.
  void TestSendLongMessage() {
    ASSERT_TRUE(server_peer_->CanSendMessage());
    ASSERT_TRUE(client_peer_->CanSendMessage());

    // Send message of maximum allowed length.
    QuicString message_max_long =
        QuicString(server_peer_->GetLargestMessagePayload(), 'A');
    ASSERT_TRUE(server_peer_->SendOrQueueMessage(message_max_long));

    // Send long message which should fail.
    QuicString message_too_long =
        QuicString(server_peer_->GetLargestMessagePayload() + 1, 'B');
    ASSERT_FALSE(server_peer_->SendOrQueueMessage(message_too_long));

    // Wait for peer 2 to receive message.
    RunTasks();

    // Client should only receive one message of allowed length.
    EXPECT_THAT(client_session_delegate_->incoming_messages(),
                testing::ElementsAre(message_max_long));
  }

  // Test that client and server are not connected after handshake failure.
  void TestDisconnectAfterFailedHandshake() {
    EXPECT_TRUE(!client_session_delegate_->connected());
    EXPECT_TRUE(!server_session_delegate_->connected());

    EXPECT_FALSE(client_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(client_peer_->IsCryptoHandshakeConfirmed());

    EXPECT_FALSE(server_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(server_peer_->IsCryptoHandshakeConfirmed());
  }

 protected:
  simulator::Simulator simulator_;

  std::unique_ptr<QuartcFactory> quartc_factory_;

  std::unique_ptr<simulator::SimulatedQuartcPacketTransport> client_transport_;
  std::unique_ptr<simulator::SimulatedQuartcPacketTransport> server_transport_;
  std::unique_ptr<simulator::CountingPacketFilter> client_filter_;
  std::unique_ptr<simulator::SymmetricLink> client_server_link_;

  std::unique_ptr<QuartcSession> client_peer_;
  std::unique_ptr<QuartcSession> server_peer_;

  std::unique_ptr<FakeQuartcStreamDelegate> client_stream_delegate_;
  std::unique_ptr<FakeQuartcSessionDelegate> client_session_delegate_;
  std::unique_ptr<FakeQuartcStreamDelegate> server_stream_delegate_;
  std::unique_ptr<FakeQuartcSessionDelegate> server_session_delegate_;
};

TEST_F(QuartcSessionTest, SendReceiveStreams) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  TestSendReceiveStreams();
}

TEST_F(QuartcSessionTest, SendReceiveMessages) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  TestSendReceiveMessage();
}

TEST_F(QuartcSessionTest, SendReceiveQueuedMessages) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  TestSendReceiveQueuedMessages(/*direction_from_server=*/true);
  TestSendReceiveQueuedMessages(/*direction_from_server=*/false);
}

TEST_F(QuartcSessionTest, SendMessageFails) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  TestSendLongMessage();
}

TEST_F(QuartcSessionTest, TestCryptoHandshakeCanWriteTriggers) {
  CreateClientAndServerSessions(QuartcSessionConfig());

  StartHandshake();

  RunTasks();

  ASSERT_TRUE(client_session_delegate_->writable_time().IsInitialized());
  ASSERT_TRUE(
      client_session_delegate_->crypto_handshake_time().IsInitialized());
  // On client, we are writable 1-rtt before crypto handshake is complete.
  ASSERT_LT(client_session_delegate_->writable_time(),
            client_session_delegate_->crypto_handshake_time());

  ASSERT_TRUE(server_session_delegate_->writable_time().IsInitialized());
  ASSERT_TRUE(
      server_session_delegate_->crypto_handshake_time().IsInitialized());
  // On server, the writable time and crypto handshake are the same. (when SHLO
  // is sent).
  ASSERT_EQ(server_session_delegate_->writable_time(),
            server_session_delegate_->crypto_handshake_time());
}

TEST_F(QuartcSessionTest, PreSharedKeyHandshake) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  client_peer_->SetPreSharedKey("foo");
  server_peer_->SetPreSharedKey("foo");
  StartHandshake();
  TestSendReceiveStreams();
  TestSendReceiveMessage();
}

// Test that data streams are not created before handshake.
TEST_F(QuartcSessionTest, CannotCreateDataStreamBeforeHandshake) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  EXPECT_EQ(nullptr, server_peer_->CreateOutgoingBidirectionalStream());
  EXPECT_EQ(nullptr, client_peer_->CreateOutgoingBidirectionalStream());
}

TEST_F(QuartcSessionTest, CancelQuartcStream) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  ASSERT_NE(nullptr, stream);

  uint32_t id = stream->id();
  EXPECT_FALSE(client_peer_->IsClosedStream(id));
  stream->SetDelegate(client_stream_delegate_.get());
  client_peer_->CancelStream(id);
  EXPECT_EQ(stream->stream_error(),
            QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
  EXPECT_TRUE(client_peer_->IsClosedStream(id));
}

// TODO(b/112561077):  This is the wrong layer for this test.  We should write a
// test specifically for QuartcPacketWriter with a stubbed-out
// QuartcPacketTransport and remove
// SimulatedQuartcPacketTransport::last_packet_number().
TEST_F(QuartcSessionTest, WriterGivesPacketNumberToTransport) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  stream->SetDelegate(client_stream_delegate_.get());

  char kClientMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kClientMessage, strlen(kClientMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  RunTasks();

  // The transport should see the latest packet number sent by QUIC.
  EXPECT_EQ(
      client_transport_->last_packet_number(),
      client_peer_->connection()->sent_packet_manager().GetLargestSentPacket());
}

TEST_F(QuartcSessionTest, CloseConnection) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  client_peer_->CloseConnection("Connection closed by client");
  EXPECT_FALSE(client_session_delegate_->connected());
  RunTasks();
  EXPECT_FALSE(server_session_delegate_->connected());
}

TEST_F(QuartcSessionTest, StreamRetransmissionEnabled) {
  CreateClientAndServerSessions(QuartcSessionConfig());
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  QuicStreamId stream_id = stream->id();
  stream->SetDelegate(client_stream_delegate_.get());
  stream->set_cancel_on_loss(false);

  client_filter_->set_packets_to_drop(1);

  char kClientMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kClientMessage, strlen(kClientMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  RunTasks();

  // Stream data should make it despite packet loss.
  ASSERT_TRUE(server_stream_delegate_->has_data());
  EXPECT_EQ(server_stream_delegate_->data()[stream_id], kClientMessage);
}

TEST_F(QuartcSessionTest, StreamRetransmissionDisabled) {
  // Disable tail loss probe, otherwise test maybe flaky because dropped
  // message will be retransmitted to detect tail loss.
  QuartcSessionConfig session_config;
  session_config.enable_tail_loss_probe = false;
  CreateClientAndServerSessions(session_config);

  // Disable probing retransmissions, otherwise test maybe flaky because dropped
  // message will be retransmitted to to probe for more bandwidth.
  client_peer_->connection()->set_fill_up_link_during_probing(false);

  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  QuicStreamId stream_id = stream->id();
  stream->SetDelegate(client_stream_delegate_.get());
  stream->set_cancel_on_loss(true);

  client_filter_->set_packets_to_drop(1);

  char kMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kMessage, strlen(kMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  simulator_.RunFor(QuicTime::Delta::FromMilliseconds(1));

  // Send another packet to trigger loss detection.
  QuartcStream* stream_1 = client_peer_->CreateOutgoingBidirectionalStream();
  stream_1->SetDelegate(client_stream_delegate_.get());

  char kMessage1[] = "Second message";
  test::QuicTestMemSliceVector stream_data_1(
      {std::make_pair(kMessage1, strlen(kMessage1))});
  stream_1->WriteMemSlices(stream_data_1.span(), /*fin=*/false);
  RunTasks();

  // QUIC should try to retransmit the first stream by loss detection.  Instead,
  // it will cancel itself.
  EXPECT_THAT(server_stream_delegate_->data()[stream_id], testing::IsEmpty());

  EXPECT_TRUE(client_peer_->IsClosedStream(stream_id));
  EXPECT_TRUE(server_peer_->IsClosedStream(stream_id));
  EXPECT_EQ(client_stream_delegate_->stream_error(stream_id),
            QUIC_STREAM_CANCELLED);
  EXPECT_EQ(server_stream_delegate_->stream_error(stream_id),
            QUIC_STREAM_CANCELLED);
}

}  // namespace

}  // namespace quic
#endif  // !defined(OS_IOS)

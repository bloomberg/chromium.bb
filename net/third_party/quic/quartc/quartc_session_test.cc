// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_session.h"

#include "build/build_config.h"
#include "net/third_party/quic/core/crypto/crypto_server_config_protobuf.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_test_mem_slice_vector.h"
#include "net/third_party/quic/quartc/quartc_factory.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests flaky on iOS.
// TODO(vasilvv): figure out what's wrong and re-enable if possible.
#if !defined(OS_IOS)
namespace quic {

namespace {

static QuicByteCount kDefaultMaxPacketSize = 1200;

// Single-threaded alarm implementation based on a MockClock.
//
// Simulates asynchronous execution on a single thread by holding alarms
// until Run() is called. Performs no synchronization, assumes that
// CreateAlarm(), Set(), Cancel(), and Run() are called on the same thread.
class FakeAlarmFactory : public QuicAlarmFactory {
 public:
  class FakeAlarm : public QuicAlarm {
   public:
    FakeAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
              FakeAlarmFactory* parent)
        : QuicAlarm(std::move(delegate)), parent_(parent) {}

    ~FakeAlarm() override { parent_->RemoveAlarm(this); }

    void SetImpl() override { parent_->AddAlarm(this); }

    void CancelImpl() override { parent_->RemoveAlarm(this); }

    void Run() { Fire(); }

   private:
    FakeAlarmFactory* parent_;
  };

  explicit FakeAlarmFactory(MockClock* clock) : clock_(clock) {}
  FakeAlarmFactory(const FakeAlarmFactory&) = delete;
  FakeAlarmFactory& operator=(const FakeAlarmFactory&) = delete;

  QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override {
    return new FakeAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate),
                         this);
  }

  QuicArenaScopedPtr<QuicAlarm> CreateAlarm(
      QuicArenaScopedPtr<QuicAlarm::Delegate> delegate,
      QuicConnectionArena* arena) override {
    return arena->New<FakeAlarm>(std::move(delegate), this);
  }

  // Runs all alarms scheduled in the next total_ms milliseconds.  Advances the
  // clock by total_ms.  Runs tasks in time order.  Executes tasks scheduled at
  // the same in an arbitrary order.
  void Run(uint32_t total_ms) {
    for (uint32_t i = 0; i < total_ms; ++i) {
      while (!alarms_.empty() && alarms_.top()->deadline() <= clock_->Now()) {
        alarms_.top()->Run();
        alarms_.pop();
      }
      clock_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
    }
  }

 private:
  void RemoveAlarm(FakeAlarm* alarm) {
    std::vector<FakeAlarm*> leftovers;
    while (!alarms_.empty()) {
      FakeAlarm* top = alarms_.top();
      alarms_.pop();
      if (top == alarm) {
        break;
      }
      leftovers.push_back(top);
    }
    for (FakeAlarm* leftover : leftovers) {
      alarms_.push(leftover);
    }
  }

  void AddAlarm(FakeAlarm* alarm) { alarms_.push(alarm); }

  MockClock* clock_;

  using AlarmCompare = std::function<bool(const FakeAlarm*, const FakeAlarm*)>;
  const AlarmCompare alarm_later_ = [](const FakeAlarm* l, const FakeAlarm* r) {
    // Sort alarms so that the earliest deadline appears first.
    return l->deadline() > r->deadline();
  };
  std::priority_queue<FakeAlarm*, std::vector<FakeAlarm*>, AlarmCompare>
      alarms_{alarm_later_};
};

// Fake QuartcPacketTransport.  Assumes all methods run on the main test thread.
class FakePacketTransport : public QuartcPacketTransport {
 public:
  explicit FakePacketTransport(QuicAlarmFactory* alarm_factory,
                               MockClock* clock)
      : alarm_(alarm_factory->CreateAlarm(new AlarmDelegate(this))),
        clock_(clock) {}

  void SetDestination(FakePacketTransport* dest) {
    if (!dest_) {
      dest_ = dest;
      dest_->SetDestination(this);
    }
    if (delegate_) {
      delegate_->OnTransportCanWrite();
    }
  }

  int Write(const char* data,
            size_t len,
            const QuartcPacketTransport::PacketInfo& info) {
    // If the destination is not set.
    if (!dest_) {
      return -1;
    }

    // Advance the time 10us to ensure the RTT is never 0ms.
    clock_->AdvanceTime(QuicTime::Delta::FromMicroseconds(10));

    if (packets_to_lose_ > 0) {
      --packets_to_lose_;
      return len;
    }
    last_packet_number_ = info.packet_number;

    if (async_) {
      packet_queue_.emplace_back(data, len);
      alarm_->Cancel();
      alarm_->Set(clock_->Now());
    } else {
      Send(QuicString(data, len));
    }
    return static_cast<int>(len);
  }

  QuartcPacketTransport::Delegate* delegate() { return delegate_; }

  void SetDelegate(QuartcPacketTransport::Delegate* delegate) override {
    delegate_ = delegate;
    if (dest_ && delegate_) {
      delegate_->OnTransportCanWrite();
    }
  }

  void SetAsync(bool async) { async_ = async; }

  QuicPacketNumber last_packet_number() { return last_packet_number_; }

  void set_packets_to_lose(QuicPacketCount count) { packets_to_lose_ = count; }

 private:
  class AlarmDelegate : public QuicAlarm::Delegate {
   public:
    explicit AlarmDelegate(FakePacketTransport* transport)
        : transport_(transport) {}

    void OnAlarm() override { transport_->OnAlarm(); }

   private:
    FakePacketTransport* transport_;
  };

  void Send(const QuicString& data) {
    DCHECK(dest_);
    DCHECK(dest_->delegate());
    dest_->delegate()->OnTransportReceived(data.data(), data.size());
  }

  void OnAlarm() {
    QUIC_LOG(WARNING) << "Sending packet: " << packet_queue_.front();
    Send(packet_queue_.front());
    packet_queue_.pop_front();

    if (!packet_queue_.empty()) {
      alarm_->Cancel();
      alarm_->Set(clock_->Now());
    }
  }

  // The writing destination of this channel.
  FakePacketTransport* dest_ = nullptr;
  // Packet transport delegate.  Called when data is received.
  QuartcPacketTransport::Delegate* delegate_ = nullptr;
  // If async, will send packets by running asynchronous tasks.
  bool async_ = false;
  // If async, packets are queued here to send.
  QuicDeque<QuicString> packet_queue_;
  // Alarm used to send data asynchronously.
  QuicArenaScopedPtr<QuicAlarm> alarm_;
  // The test clock.  Used to ensure the RTT is not 0.
  MockClock* clock_;

  QuicPacketNumber last_packet_number_;
  QuicPacketCount packets_to_lose_ = 0;
};

class FakeQuartcSessionDelegate : public QuartcSession::Delegate {
 public:
  explicit FakeQuartcSessionDelegate(QuartcStream::Delegate* stream_delegate)
      : stream_delegate_(stream_delegate) {}
  // Called when peers have established forward-secure encryption
  void OnCryptoHandshakeComplete() override {
    LOG(INFO) << "Crypto handshake complete!";
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

  void OnCongestionControlChange(QuicBandwidth bandwidth_estimate,
                                 QuicBandwidth pacing_rate,
                                 QuicTime::Delta latest_rtt) override {}

  QuartcStream* incoming_stream() { return last_incoming_stream_; }

  bool connected() { return connected_; }

 private:
  QuartcStream* last_incoming_stream_;
  bool connected_ = true;
  QuartcStream::Delegate* stream_delegate_;
};

class FakeQuartcStreamDelegate : public QuartcStream::Delegate {
 public:
  void OnReceived(QuartcStream* stream,
                  const char* data,
                  size_t size) override {
    received_data_[stream->id()] += QuicString(data, size);
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

class QuartcSessionTest : public QuicTest,
                          public QuicConnectionHelperInterface {
 public:
  ~QuartcSessionTest() override {}

  void Init() {
    // Quic crashes if packets are sent at time 0, and the clock defaults to 0.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));
    client_transport_ =
        QuicMakeUnique<FakePacketTransport>(&alarm_factory_, &clock_);
    server_transport_ =
        QuicMakeUnique<FakePacketTransport>(&alarm_factory_, &clock_);
    // Make the channel asynchronous so that two peer will not keep calling each
    // other when they exchange information.
    client_transport_->SetAsync(true);
    client_transport_->SetDestination(server_transport_.get());

    client_writer_ = QuicMakeUnique<QuartcPacketWriter>(client_transport_.get(),
                                                        kDefaultMaxPacketSize);
    server_writer_ = QuicMakeUnique<QuartcPacketWriter>(server_transport_.get(),
                                                        kDefaultMaxPacketSize);

    client_stream_delegate_ = QuicMakeUnique<FakeQuartcStreamDelegate>();
    client_session_delegate_ = QuicMakeUnique<FakeQuartcSessionDelegate>(
        client_stream_delegate_.get());

    server_stream_delegate_ = QuicMakeUnique<FakeQuartcStreamDelegate>();
    server_session_delegate_ = QuicMakeUnique<FakeQuartcSessionDelegate>(
        server_stream_delegate_.get());
  }

  // The parameters are used to control whether the handshake will success or
  // not.
  void CreateClientAndServerSessions() {
    Init();
    client_peer_ =
        CreateSession(Perspective::IS_CLIENT, std::move(client_writer_));
    client_peer_->SetDelegate(client_session_delegate_.get());
    server_peer_ =
        CreateSession(Perspective::IS_SERVER, std::move(server_writer_));
    server_peer_->SetDelegate(server_session_delegate_.get());
  }

  std::unique_ptr<QuartcSession> CreateSession(
      Perspective perspective,
      std::unique_ptr<QuartcPacketWriter> writer) {
    std::unique_ptr<QuicConnection> quic_connection =
        CreateConnection(perspective, writer.get());
    QuicString remote_fingerprint_value = "value";
    QuicConfig config;
    return QuicMakeUnique<QuartcSession>(std::move(quic_connection), config,
                                         remote_fingerprint_value, perspective,
                                         this, &clock_, std::move(writer));
  }

  std::unique_ptr<QuicConnection> CreateConnection(Perspective perspective,
                                                   QuartcPacketWriter* writer) {
    QuicIpAddress ip;
    ip.FromString("0.0.0.0");
    return QuicMakeUnique<QuicConnection>(
        0, QuicSocketAddress(ip, 0), this /*QuicConnectionHelperInterface*/,
        &alarm_factory_, writer, /*owns_writer=*/false, perspective,
        CurrentSupportedVersions());
  }

  // Runs all tasks scheduled in the next 200 ms.
  void RunTasks() { alarm_factory_.Run(200); }

  void StartHandshake() {
    server_peer_->StartCryptoHandshake();
    client_peer_->StartCryptoHandshake();
    RunTasks();
  }

  // Test handshake establishment and sending/receiving of data for two
  // directions.
  void TestStreamConnection() {
    ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());
    ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
    ASSERT_TRUE(server_peer_->IsEncryptionEstablished());
    ASSERT_TRUE(client_peer_->IsEncryptionEstablished());

    // Now we can establish encrypted outgoing stream.
    QuartcStream* outgoing_stream = server_peer_->CreateOutgoingDynamicStream();
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

    QuartcStream* incoming = client_session_delegate_->incoming_stream();
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

  // Test that client and server are not connected after handshake failure.
  void TestDisconnectAfterFailedHandshake() {
    EXPECT_TRUE(!client_session_delegate_->connected());
    EXPECT_TRUE(!server_session_delegate_->connected());

    EXPECT_FALSE(client_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(client_peer_->IsCryptoHandshakeConfirmed());

    EXPECT_FALSE(server_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(server_peer_->IsCryptoHandshakeConfirmed());
  }

  const QuicClock* GetClock() const override { return &clock_; }

  QuicRandom* GetRandomGenerator() override {
    return QuicRandom::GetInstance();
  }

  QuicBufferAllocator* GetStreamSendBufferAllocator() override {
    return &buffer_allocator_;
  }

 protected:
  MockClock clock_;
  FakeAlarmFactory alarm_factory_{&clock_};
  SimpleBufferAllocator buffer_allocator_;

  std::unique_ptr<FakePacketTransport> client_transport_;
  std::unique_ptr<FakePacketTransport> server_transport_;
  std::unique_ptr<QuartcPacketWriter> client_writer_;
  std::unique_ptr<QuartcPacketWriter> server_writer_;
  std::unique_ptr<QuartcSession> client_peer_;
  std::unique_ptr<QuartcSession> server_peer_;

  std::unique_ptr<FakeQuartcStreamDelegate> client_stream_delegate_;
  std::unique_ptr<FakeQuartcSessionDelegate> client_session_delegate_;
  std::unique_ptr<FakeQuartcStreamDelegate> server_stream_delegate_;
  std::unique_ptr<FakeQuartcSessionDelegate> server_session_delegate_;
};

TEST_F(QuartcSessionTest, StreamConnection) {
  CreateClientAndServerSessions();
  StartHandshake();
  TestStreamConnection();
}

TEST_F(QuartcSessionTest, PreSharedKeyHandshake) {
  CreateClientAndServerSessions();
  client_peer_->SetPreSharedKey("foo");
  server_peer_->SetPreSharedKey("foo");
  StartHandshake();
  TestStreamConnection();
}

// Test that data streams are not created before handshake.
TEST_F(QuartcSessionTest, CannotCreateDataStreamBeforeHandshake) {
  CreateClientAndServerSessions();
  EXPECT_EQ(nullptr, server_peer_->CreateOutgoingDynamicStream());
  EXPECT_EQ(nullptr, client_peer_->CreateOutgoingDynamicStream());
}

TEST_F(QuartcSessionTest, CancelQuartcStream) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingDynamicStream();
  ASSERT_NE(nullptr, stream);

  uint32_t id = stream->id();
  EXPECT_FALSE(client_peer_->IsClosedStream(id));
  stream->SetDelegate(client_stream_delegate_.get());
  client_peer_->CancelStream(id);
  EXPECT_EQ(stream->stream_error(),
            QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
  EXPECT_TRUE(client_peer_->IsClosedStream(id));
}

TEST_F(QuartcSessionTest, WriterGivesPacketNumberToTransport) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingDynamicStream();
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
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  client_peer_->CloseConnection("Connection closed by client");
  EXPECT_FALSE(client_session_delegate_->connected());
  RunTasks();
  EXPECT_FALSE(server_session_delegate_->connected());
}

TEST_F(QuartcSessionTest, StreamRetransmissionEnabled) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingDynamicStream();
  QuicStreamId stream_id = stream->id();
  stream->SetDelegate(client_stream_delegate_.get());
  stream->set_cancel_on_loss(false);

  client_transport_->set_packets_to_lose(1);

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
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingDynamicStream();
  QuicStreamId stream_id = stream->id();
  stream->SetDelegate(client_stream_delegate_.get());
  stream->set_cancel_on_loss(true);

  client_transport_->set_packets_to_lose(1);

  char kMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kMessage, strlen(kMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  alarm_factory_.Run(1);

  // Send another packet to trigger loss detection.
  QuartcStream* stream_1 = client_peer_->CreateOutgoingDynamicStream();
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_dispatcher.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "net/third_party/quiche/src/quic/core/chlo_extractor.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer_wrapper.h"
#include "net/third_party/quiche/src/quic/core/quic_time_wait_list_manager.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/fake_proof_source.h"
#include "net/third_party/quiche/src/quic/test_tools/first_flight.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_quic_time_wait_list_manager.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_buffered_packet_store_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_crypto_server_config_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_dispatcher_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_time_wait_list_manager_peer.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_crypto_server_stream_helper.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_arraysize.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_str_cat.h"
#include "net/third_party/quiche/src/common/test_tools/quiche_test_utils.h"

using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;
using testing::WithoutArgs;

static const size_t kDefaultMaxConnectionsInStore = 100;
static const size_t kMaxConnectionsWithoutCHLO =
    kDefaultMaxConnectionsInStore / 2;
static const int16_t kMaxNumSessionsToCreate = 16;

namespace quic {
namespace test {
namespace {

class TestQuicSpdyServerSession : public QuicServerSessionBase {
 public:
  TestQuicSpdyServerSession(const QuicConfig& config,
                            QuicConnection* connection,
                            const QuicCryptoServerConfig* crypto_config,
                            QuicCompressedCertsCache* compressed_certs_cache)
      : QuicServerSessionBase(config,
                              CurrentSupportedVersions(),
                              connection,
                              nullptr,
                              nullptr,
                              crypto_config,
                              compressed_certs_cache) {
    Initialize();
  }
  TestQuicSpdyServerSession(const TestQuicSpdyServerSession&) = delete;
  TestQuicSpdyServerSession& operator=(const TestQuicSpdyServerSession&) =
      delete;

  ~TestQuicSpdyServerSession() override { DeleteConnection(); }

  MOCK_METHOD(void,
              OnConnectionClosed,
              (const QuicConnectionCloseFrame& frame,
               ConnectionCloseSource source),
              (override));
  MOCK_METHOD(QuicSpdyStream*,
              CreateIncomingStream,
              (QuicStreamId id),
              (override));
  MOCK_METHOD(QuicSpdyStream*,
              CreateIncomingStream,
              (PendingStream*),
              (override));
  MOCK_METHOD(QuicSpdyStream*,
              CreateOutgoingBidirectionalStream,
              (),
              (override));
  MOCK_METHOD(QuicSpdyStream*,
              CreateOutgoingUnidirectionalStream,
              (),
              (override));

  std::unique_ptr<QuicCryptoServerStreamBase> CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override {
    return CreateCryptoServerStream(crypto_config, compressed_certs_cache, this,
                                    stream_helper());
  }

  QuicCryptoServerStreamBase::Helper* stream_helper() {
    return QuicServerSessionBase::stream_helper();
  }
};

class TestDispatcher : public QuicDispatcher {
 public:
  TestDispatcher(const QuicConfig* config,
                 const QuicCryptoServerConfig* crypto_config,
                 QuicVersionManager* version_manager,
                 QuicRandom* random)
      : QuicDispatcher(config,
                       crypto_config,
                       version_manager,
                       std::make_unique<MockQuicConnectionHelper>(),
                       std::unique_ptr<QuicCryptoServerStreamBase::Helper>(
                           new QuicSimpleCryptoServerStreamHelper()),
                       std::make_unique<MockAlarmFactory>(),
                       kQuicDefaultConnectionIdLength),
        random_(random) {}

  MOCK_METHOD(std::unique_ptr<QuicSession>,
              CreateQuicSession,
              (QuicConnectionId connection_id,
               const QuicSocketAddress& peer_address,
               quiche::QuicheStringPiece alpn,
               const quic::ParsedQuicVersion& version),
              (override));

  MOCK_METHOD(bool,
              ShouldCreateOrBufferPacketForConnection,
              (const ReceivedPacketInfo& packet_info),
              (override));

  struct TestQuicPerPacketContext : public QuicPerPacketContext {
    std::string custom_packet_context;
  };

  std::unique_ptr<QuicPerPacketContext> GetPerPacketContext() const override {
    auto test_context = std::make_unique<TestQuicPerPacketContext>();
    test_context->custom_packet_context = custom_packet_context_;
    return std::move(test_context);
  }

  void RestorePerPacketContext(
      std::unique_ptr<QuicPerPacketContext> context) override {
    TestQuicPerPacketContext* test_context =
        static_cast<TestQuicPerPacketContext*>(context.get());
    custom_packet_context_ = test_context->custom_packet_context;
  }

  std::string custom_packet_context_;

  using QuicDispatcher::SetAllowShortInitialServerConnectionIds;
  using QuicDispatcher::writer;

  QuicRandom* random_;
};

// A Connection class which unregisters the session from the dispatcher when
// sending connection close.
// It'd be slightly more realistic to do this from the Session but it would
// involve a lot more mocking.
class MockServerConnection : public MockQuicConnection {
 public:
  MockServerConnection(QuicConnectionId connection_id,
                       MockQuicConnectionHelper* helper,
                       MockAlarmFactory* alarm_factory,
                       QuicDispatcher* dispatcher)
      : MockQuicConnection(connection_id,
                           helper,
                           alarm_factory,
                           Perspective::IS_SERVER),
        dispatcher_(dispatcher) {}

  void UnregisterOnConnectionClosed() {
    QUIC_LOG(ERROR) << "Unregistering " << connection_id();
    dispatcher_->OnConnectionClosed(connection_id(), QUIC_NO_ERROR,
                                    "Unregistering.",
                                    ConnectionCloseSource::FROM_SELF);
  }

 private:
  QuicDispatcher* dispatcher_;
};

class QuicDispatcherTestBase : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicDispatcherTestBase()
      : QuicDispatcherTestBase(crypto_test_utils::ProofSourceForTesting()) {}

  explicit QuicDispatcherTestBase(std::unique_ptr<ProofSource> proof_source)
      : version_(GetParam()),
        version_manager_(AllSupportedVersions()),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(),
                       std::move(proof_source),
                       KeyExchangeSource::Default()),
        server_address_(QuicIpAddress::Any4(), 5),
        dispatcher_(
            new NiceMock<TestDispatcher>(&config_,
                                         &crypto_config_,
                                         &version_manager_,
                                         mock_helper_.GetRandomGenerator())),
        time_wait_list_manager_(nullptr),
        session1_(nullptr),
        session2_(nullptr),
        store_(nullptr),
        connection_id_(1) {}

  void SetUp() override {
    dispatcher_->InitializeWithWriter(new NiceMock<MockPacketWriter>());
    // Set the counter to some value to start with.
    QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(
        dispatcher_.get(), kMaxNumSessionsToCreate);
    ON_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(_))
        .WillByDefault(Return(true));
  }

  MockQuicConnection* connection1() {
    if (session1_ == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<MockQuicConnection*>(session1_->connection());
  }

  MockQuicConnection* connection2() {
    if (session2_ == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<MockQuicConnection*>(session2_->connection());
  }

  // Process a packet with an 8 byte connection id,
  // 6 byte packet number, default path id, and packet number 1,
  // using the version under test.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag,
                     const std::string& data) {
    ProcessPacket(peer_address, server_connection_id, has_version_flag, data,
                  CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER);
  }

  // Process a packet with a default path id, and packet number 1,
  // using the version under test.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag,
                     const std::string& data,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicPacketNumberLength packet_number_length) {
    ProcessPacket(peer_address, server_connection_id, has_version_flag, data,
                  server_connection_id_included, packet_number_length, 1);
  }

  // Process a packet using the version under test.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag,
                     const std::string& data,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicPacketNumberLength packet_number_length,
                     uint64_t packet_number) {
    ProcessPacket(peer_address, server_connection_id, has_version_flag,
                  version_, data, true, server_connection_id_included,
                  packet_number_length, packet_number);
  }

  // Processes a packet.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag,
                     ParsedQuicVersion version,
                     const std::string& data,
                     bool full_padding,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicPacketNumberLength packet_number_length,
                     uint64_t packet_number) {
    ProcessPacket(peer_address, server_connection_id, EmptyQuicConnectionId(),
                  has_version_flag, version, data, full_padding,
                  server_connection_id_included, CONNECTION_ID_ABSENT,
                  packet_number_length, packet_number);
  }

  // Processes a packet.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     QuicConnectionId client_connection_id,
                     bool has_version_flag,
                     ParsedQuicVersion version,
                     const std::string& data,
                     bool full_padding,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicConnectionIdIncluded client_connection_id_included,
                     QuicPacketNumberLength packet_number_length,
                     uint64_t packet_number) {
    ParsedQuicVersionVector versions(SupportedVersions(version));
    std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
        server_connection_id, client_connection_id, has_version_flag, false,
        packet_number, data, full_padding, server_connection_id_included,
        client_connection_id_included, packet_number_length, &versions));
    std::unique_ptr<QuicReceivedPacket> received_packet(
        ConstructReceivedPacket(*packet, mock_helper_.GetClock()->Now()));
    ProcessReceivedPacket(std::move(received_packet), peer_address, version,
                          server_connection_id);
  }

  void ProcessReceivedPacket(
      std::unique_ptr<QuicReceivedPacket> received_packet,
      const QuicSocketAddress& peer_address,
      const ParsedQuicVersion& version,
      const QuicConnectionId& server_connection_id) {
    if (version.UsesQuicCrypto() &&
        ChloExtractor::Extract(*received_packet, version, {}, nullptr,
                               server_connection_id.length())) {
      // Add CHLO packet to the beginning to be verified first, because it is
      // also processed first by new session.
      data_connection_map_[server_connection_id].push_front(
          std::string(received_packet->data(), received_packet->length()));
    } else {
      // For non-CHLO, always append to last.
      data_connection_map_[server_connection_id].push_back(
          std::string(received_packet->data(), received_packet->length()));
    }
    dispatcher_->ProcessPacket(server_address_, peer_address, *received_packet);
  }

  void ValidatePacket(QuicConnectionId conn_id,
                      const QuicEncryptedPacket& packet) {
    EXPECT_EQ(data_connection_map_[conn_id].front().length(),
              packet.AsStringPiece().length());
    EXPECT_EQ(data_connection_map_[conn_id].front(), packet.AsStringPiece());
    data_connection_map_[conn_id].pop_front();
  }

  std::unique_ptr<QuicSession> CreateSession(
      TestDispatcher* dispatcher,
      const QuicConfig& config,
      QuicConnectionId connection_id,
      const QuicSocketAddress& /*peer_address*/,
      MockQuicConnectionHelper* helper,
      MockAlarmFactory* alarm_factory,
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      TestQuicSpdyServerSession** session_ptr) {
    MockServerConnection* connection = new MockServerConnection(
        connection_id, helper, alarm_factory, dispatcher);
    connection->SetQuicPacketWriter(dispatcher->writer(),
                                    /*owns_writer=*/false);
    auto session = std::make_unique<TestQuicSpdyServerSession>(
        config, connection, crypto_config, compressed_certs_cache);
    *session_ptr = session.get();
    connection->set_visitor(session.get());
    ON_CALL(*connection, CloseConnection(_, _, _))
        .WillByDefault(WithoutArgs(Invoke(
            connection, &MockServerConnection::UnregisterOnConnectionClosed)));
    return session;
  }

  void CreateTimeWaitListManager() {
    time_wait_list_manager_ = new MockTimeWaitListManager(
        QuicDispatcherPeer::GetWriter(dispatcher_.get()), dispatcher_.get(),
        mock_helper_.GetClock(), &mock_alarm_factory_);
    // dispatcher_ takes the ownership of time_wait_list_manager_.
    QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                               time_wait_list_manager_);
  }

  std::string SerializeCHLO() {
    CryptoHandshakeMessage client_hello;
    client_hello.set_tag(kCHLO);
    client_hello.SetStringPiece(kALPN, ExpectedAlpn());
    return std::string(client_hello.GetSerialized().AsStringPiece());
  }

  void ProcessUndecryptableEarlyPacket(
      const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    ProcessUndecryptableEarlyPacket(version_, peer_address,
                                    server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const ParsedQuicVersion& version,
      const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    std::unique_ptr<QuicEncryptedPacket> encrypted_packet =
        GetUndecryptableEarlyPacket(version, server_connection_id);
    std::unique_ptr<QuicReceivedPacket> received_packet(ConstructReceivedPacket(
        *encrypted_packet, mock_helper_.GetClock()->Now()));
    ProcessReceivedPacket(std::move(received_packet), peer_address, version,
                          server_connection_id);
  }

  void ProcessFirstFlight(const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version_, peer_address, server_connection_id);
  }

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version, peer_address, server_connection_id,
                       EmptyQuicConnectionId());
  }

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id,
                          const QuicConnectionId& client_connection_id) {
    std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
        GetFirstFlightOfPackets(version, server_connection_id,
                                client_connection_id);
    for (auto&& packet : packets) {
      ProcessReceivedPacket(std::move(packet), peer_address, version,
                            server_connection_id);
    }
  }

  std::string ExpectedAlpnForVersion(ParsedQuicVersion version) {
    return AlpnForVersion(version);
  }

  std::string ExpectedAlpn() { return ExpectedAlpnForVersion(version_); }

  void MarkSession1Deleted() { session1_ = nullptr; }

  void VerifyVersionSupported(ParsedQuicVersion version) {
    QuicConnectionId connection_id = TestConnectionId(++connection_id_);
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(connection_id, client_address,
                                  Eq(ExpectedAlpnForVersion(version)), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, connection_id, client_address,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(connection_id, packet);
            })));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(
                    ReceivedPacketInfoConnectionIdEquals(connection_id)));
    ProcessFirstFlight(version, client_address, connection_id);
  }

  void VerifyVersionNotSupported(ParsedQuicVersion version) {
    QuicConnectionId connection_id = TestConnectionId(++connection_id_);
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(connection_id, client_address, _, _))
        .Times(0);
    ProcessFirstFlight(version, client_address, connection_id);
  }

  void TestTlsMultiPacketClientHello(bool add_reordering);

  ParsedQuicVersion version_;
  MockQuicConnectionHelper mock_helper_;
  MockAlarmFactory mock_alarm_factory_;
  QuicConfig config_;
  QuicVersionManager version_manager_;
  QuicCryptoServerConfig crypto_config_;
  QuicSocketAddress server_address_;
  std::unique_ptr<NiceMock<TestDispatcher>> dispatcher_;
  MockTimeWaitListManager* time_wait_list_manager_;
  TestQuicSpdyServerSession* session1_;
  TestQuicSpdyServerSession* session2_;
  std::map<QuicConnectionId, std::list<std::string>> data_connection_map_;
  QuicBufferedPacketStore* store_;
  uint64_t connection_id_;
};

class QuicDispatcherTestAllVersions : public QuicDispatcherTestBase {};
class QuicDispatcherTestOneVersion : public QuicDispatcherTestBase {};

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsAllVersions,
                         QuicDispatcherTestAllVersions,
                         ::testing::ValuesIn(CurrentSupportedVersions()),
                         ::testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsOneVersion,
                         QuicDispatcherTestOneVersion,
                         ::testing::Values(CurrentSupportedVersions().front()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicDispatcherTestAllVersions, TlsClientHelloCreatesSession) {
  if (version_.UsesQuicCrypto()) {
    return;
  }
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));

  ProcessFirstFlight(client_address, TestConnectionId(1));
}

void QuicDispatcherTestBase::TestTlsMultiPacketClientHello(
    bool add_reordering) {
  if (!version_.UsesTls()) {
    return;
  }
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId server_connection_id = TestConnectionId();
  QuicConfig client_config = DefaultQuicConfig();
  // Add a 2000-byte custom parameter to increase the length of the CHLO.
  constexpr auto kCustomParameterId =
      static_cast<TransportParameters::TransportParameterId>(0xff33);
  std::string kCustomParameterValue(2000, '-');
  client_config.custom_transport_parameters_to_send()[kCustomParameterId] =
      kCustomParameterValue;
  std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
      GetFirstFlightOfPackets(version_, client_config, server_connection_id);
  ASSERT_EQ(packets.size(), 2u);
  if (add_reordering) {
    std::swap(packets[0], packets[1]);
  }

  // Processing the first packet should not create a new session.
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(server_connection_id)));
  ProcessReceivedPacket(std::move(packets[0]), client_address, version_,
                        server_connection_id);

  EXPECT_EQ(dispatcher_->session_map().size(), 0u)
      << "No session should be created before the rest of the CHLO arrives.";

  // Processing the second packet should create the new session.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(server_connection_id, client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, server_connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2);

  ProcessReceivedPacket(std::move(packets[1]), client_address, version_,
                        server_connection_id);
  EXPECT_EQ(dispatcher_->session_map().size(), 1u);
}

TEST_P(QuicDispatcherTestAllVersions, TlsMultiPacketClientHello) {
  TestTlsMultiPacketClientHello(/*add_reordering=*/false);
}

TEST_P(QuicDispatcherTestAllVersions, TlsMultiPacketClientHelloWithReordering) {
  TestTlsMultiPacketClientHello(/*add_reordering=*/true);
}

TEST_P(QuicDispatcherTestAllVersions, ProcessPackets) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(2), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(2), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(2), packet);
      })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(2))));
  ProcessFirstFlight(client_address, TestConnectionId(2));

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessPacket(client_address, TestConnectionId(1), false, "data");
}

// Regression test of b/93325907.
TEST_P(QuicDispatcherTestAllVersions, DispatcherDoesNotRejectPacketNumberZero) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  // Verify both packets 1 and 2 are processed by connection 1.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2)
      .WillRepeatedly(
          WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
            ValidatePacket(TestConnectionId(1), packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));
  ProcessFirstFlight(client_address, TestConnectionId(1));
  // Packet number 256 with packet number length 1 would be considered as 0 in
  // dispatcher.
  ProcessPacket(client_address, TestConnectionId(1), false, version_, "", true,
                CONNECTION_ID_PRESENT, PACKET_1BYTE_PACKET_NUMBER, 256);
}

TEST_P(QuicDispatcherTestOneVersion, StatelessVersionNegotiation) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(TestConnectionId(1), _, _, _, _, _, _, _))
      .Times(1);
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     TestConnectionId(1));
}

TEST_P(QuicDispatcherTestOneVersion,
       StatelessVersionNegotiationWithVeryLongConnectionId) {
  QuicConnectionId connection_id = QuicUtils::CreateRandomConnectionId(33);
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(connection_id, _, _, _, _, _, _, _))
      .Times(1);
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     connection_id);
}

TEST_P(QuicDispatcherTestOneVersion,
       StatelessVersionNegotiationWithClientConnectionId) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(
                  TestConnectionId(1), TestConnectionId(2), _, _, _, _, _, _))
      .Times(1);
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     TestConnectionId(1), TestConnectionId(2));
}

TEST_P(QuicDispatcherTestOneVersion, NoVersionNegotiationWithSmallPacket) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
      .Times(0);
  std::string chlo = SerializeCHLO() + std::string(1200, 'a');
  // Truncate to 1100 bytes of payload which results in a packet just
  // under 1200 bytes after framing, packet, and encryption overhead.
  DCHECK_LE(1200u, chlo.length());
  std::string truncated_chlo = chlo.substr(0, 1100);
  DCHECK_EQ(1100u, truncated_chlo.length());
  ProcessPacket(client_address, TestConnectionId(1), true,
                QuicVersionReservedForNegotiation(), truncated_chlo, false,
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER, 1);
}

// Disabling CHLO size validation allows the dispatcher to send version
// negotiation packets in response to a CHLO that is otherwise too small.
TEST_P(QuicDispatcherTestOneVersion,
       VersionNegotiationWithoutChloSizeValidation) {
  crypto_config_.set_validate_chlo_size(false);

  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
      .Times(1);
  std::string chlo = SerializeCHLO() + std::string(1200, 'a');
  // Truncate to 1100 bytes of payload which results in a packet just
  // under 1200 bytes after framing, packet, and encryption overhead.
  DCHECK_LE(1200u, chlo.length());
  std::string truncated_chlo = chlo.substr(0, 1100);
  DCHECK_EQ(1100u, truncated_chlo.length());
  ProcessPacket(client_address, TestConnectionId(1), true,
                QuicVersionReservedForNegotiation(), truncated_chlo, true,
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER, 1);
}

TEST_P(QuicDispatcherTestAllVersions, Shutdown) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(_, client_address, Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));

  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              CloseConnection(QUIC_PEER_GOING_AWAY, _, _));

  dispatcher_->Shutdown();
}

TEST_P(QuicDispatcherTestAllVersions, TimeWaitListManager) {
  CreateTimeWaitListManager();

  // Create a new session.
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));

  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));
  ProcessFirstFlight(client_address, connection_id);

  // Now close the connection, which should add it to the time wait list.
  session1_->connection()->CloseConnection(
      QUIC_INVALID_VERSION,
      "Server: Packet 2 without version flag before version negotiated.",
      ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));

  // Dispatcher forwards subsequent packets for this connection_id to the time
  // wait list manager.
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _))
      .Times(1);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);
  ProcessPacket(client_address, connection_id, true, "data");
}

TEST_P(QuicDispatcherTestAllVersions, NoVersionPacketToTimeWaitListManager) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);
  // Dispatcher forwards all packets for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _))
      .Times(1);
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data");
}

TEST_P(QuicDispatcherTestAllVersions,
       DonotTimeWaitPacketsWithUnknownConnectionIdAndNoVersion) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();

  char short_packet[22] = {0x70, 0xa7, 0x02, 0x6b};
  QuicReceivedPacket packet(short_packet, 22, QuicTime::Zero());
  char valid_size_packet[23] = {0x70, 0xa7, 0x02, 0x6c};
  QuicReceivedPacket packet2(valid_size_packet, 23, QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);
  // Verify small packet is silently dropped.
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _))
      .Times(0);
  dispatcher_->ProcessPacket(server_address_, client_address, packet);
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, packet2);
}

// Makes sure nine-byte connection IDs are replaced by 8-byte ones.
TEST_P(QuicDispatcherTestAllVersions, LongConnectionIdLengthReplaced) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    // When variable length connection IDs are not supported, the connection
    // fails. See StrayPacketTruncatedConnectionId.
    return;
  }
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  QuicConnectionId bad_connection_id = TestConnectionIdNineBytesLong(2);
  QuicConnectionId fixed_connection_id =
      QuicUtils::CreateReplacementConnectionId(bad_connection_id);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(fixed_connection_id, client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, fixed_connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, bad_connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(bad_connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(bad_connection_id)));
  ProcessFirstFlight(client_address, bad_connection_id);
}

// Makes sure zero-byte connection IDs are replaced by 8-byte ones.
TEST_P(QuicDispatcherTestAllVersions, InvalidShortConnectionIdLengthReplaced) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    // When variable length connection IDs are not supported, the connection
    // fails. See StrayPacketTruncatedConnectionId.
    return;
  }
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  QuicConnectionId bad_connection_id = EmptyQuicConnectionId();
  QuicConnectionId fixed_connection_id =
      QuicUtils::CreateReplacementConnectionId(bad_connection_id);

  // Disable validation of invalid short connection IDs.
  dispatcher_->SetAllowShortInitialServerConnectionIds(true);
  // Note that StrayPacketTruncatedConnectionId covers the case where the
  // validation is still enabled.

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(fixed_connection_id, client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, fixed_connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, bad_connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(bad_connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(bad_connection_id)));
  ProcessFirstFlight(client_address, bad_connection_id);
}

// Makes sure TestConnectionId(1) creates a new connection and
// TestConnectionIdNineBytesLong(2) gets replaced.
TEST_P(QuicDispatcherTestAllVersions, MixGoodAndBadConnectionIdLengthPackets) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    return;
  }

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId bad_connection_id = TestConnectionIdNineBytesLong(2);
  QuicConnectionId fixed_connection_id =
      QuicUtils::CreateReplacementConnectionId(bad_connection_id);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(fixed_connection_id, client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, fixed_connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, bad_connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(bad_connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(bad_connection_id)));
  ProcessFirstFlight(client_address, bad_connection_id);

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessPacket(client_address, TestConnectionId(1), false, "data");
}

TEST_P(QuicDispatcherTestAllVersions, ProcessPacketWithZeroPort) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 0);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);
  ProcessPacket(client_address, TestConnectionId(1), /*has_version_flag=*/true,
                "data");
}

TEST_P(QuicDispatcherTestAllVersions,
       ProcessPacketWithInvalidShortInitialConnectionId) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    return;
  }
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, client_address, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);
  ProcessFirstFlight(client_address, EmptyQuicConnectionId());
}

TEST_P(QuicDispatcherTestAllVersions, OKSeqNoPacketProcessed) {
  if (version_.UsesTls()) {
    // QUIC+TLS allows clients to start with any packet number.
    return;
  }
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));

  // A packet whose packet number is the largest that is allowed to start a
  // connection.
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(
                  ReceivedPacketInfoConnectionIdEquals(connection_id)));
  ProcessPacket(client_address, connection_id, true, SerializeCHLO(),
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER,
                QuicDispatcher::kMaxReasonableInitialPacketNumber);
}

TEST_P(QuicDispatcherTestOneVersion, VersionsChangeInFlight) {
  VerifyVersionNotSupported(QuicVersionReservedForNegotiation());
  for (ParsedQuicVersion version : CurrentSupportedVersions()) {
    VerifyVersionSupported(version);
  }

  // Turn off version Q050.
  SetQuicReloadableFlag(quic_disable_version_q050, true);
  VerifyVersionNotSupported(
      ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_50));

  // Turn on version Q050.
  SetQuicReloadableFlag(quic_disable_version_q050, false);
  VerifyVersionSupported(
      ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_50));
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionsWithVersionNegotiation) {
  static_assert(quic::SupportedVersions().size() == 8u,
                "Please add deprecated versions to this test");
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();

  {
    char packet47[kMinPacketSizeForVersionNegotiation] = {
        0xC0, 'Q', '0', '4', '7', /*connection ID length byte*/ 0x50};
    QuicReceivedPacket received_packet47(
        packet47, kMinPacketSizeForVersionNegotiation, QuicTime::Zero());
    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
    EXPECT_CALL(*time_wait_list_manager_,
                SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
        .Times(1);
    dispatcher_->ProcessPacket(server_address_, client_address,
                               received_packet47);
  }

  {
    char packet45[kMinPacketSizeForVersionNegotiation] = {
        0xC0, 'Q', '0', '4', '5', /*connection ID length byte*/ 0x50};
    QuicReceivedPacket received_packet45(
        packet45, kMinPacketSizeForVersionNegotiation, QuicTime::Zero());
    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
    EXPECT_CALL(*time_wait_list_manager_,
                SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
        .Times(1);
    dispatcher_->ProcessPacket(server_address_, client_address,
                               received_packet45);
  }

  {
    char packet44[kMinPacketSizeForVersionNegotiation] = {
        0xFF, 'Q', '0', '4', '4', /*connection ID length byte*/ 0x50};
    QuicReceivedPacket received_packet44(
        packet44, kMinPacketSizeForVersionNegotiation, QuicTime::Zero());
    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
    EXPECT_CALL(*time_wait_list_manager_,
                SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
        .Times(1);
    dispatcher_->ProcessPacket(server_address_, client_address,
                               received_packet44);
  }
}

TEST_P(QuicDispatcherTestOneVersion, VersionNegotiationProbeOld) {
  SetQuicFlag(FLAGS_quic_prober_uses_length_prefixed_connection_ids, false);
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  char packet[1200];
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  QuicEncryptedPacket encrypted(packet, sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  QuicConnectionId client_connection_id = EmptyQuicConnectionId();
  QuicConnectionId server_connection_id(
      destination_connection_id_bytes, sizeof(destination_connection_id_bytes));
  bool ietf_quic = true;
  bool use_length_prefix =
      GetQuicFlag(FLAGS_quic_prober_uses_length_prefixed_connection_ids);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(server_connection_id, client_connection_id,
                                   ietf_quic, use_length_prefix, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);

  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
}

TEST_P(QuicDispatcherTestOneVersion, VersionNegotiationProbe) {
  SetQuicFlag(FLAGS_quic_prober_uses_length_prefixed_connection_ids, true);
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  char packet[1200];
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  QuicEncryptedPacket encrypted(packet, sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  QuicConnectionId client_connection_id = EmptyQuicConnectionId();
  QuicConnectionId server_connection_id(
      destination_connection_id_bytes, sizeof(destination_connection_id_bytes));
  bool ietf_quic = true;
  bool use_length_prefix =
      GetQuicFlag(FLAGS_quic_prober_uses_length_prefixed_connection_ids);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(server_connection_id, client_connection_id,
                                   ietf_quic, use_length_prefix, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);

  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
}

// Testing packet writer that saves all packets instead of sending them.
// Useful for tests that need access to sent packets.
class SavingWriter : public QuicPacketWriterWrapper {
 public:
  bool IsWriteBlocked() const override { return false; }

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& /*self_client_address*/,
                          const QuicSocketAddress& /*peer_client_address*/,
                          PerPacketOptions* /*options*/) override {
    packets_.push_back(
        QuicEncryptedPacket(buffer, buf_len, /*owns_buffer=*/false).Clone());
    return WriteResult(WRITE_STATUS_OK, buf_len);
  }

  std::vector<std::unique_ptr<QuicEncryptedPacket>>* packets() {
    return &packets_;
  }

 private:
  std::vector<std::unique_ptr<QuicEncryptedPacket>> packets_;
};

TEST_P(QuicDispatcherTestOneVersion, VersionNegotiationProbeEndToEndOld) {
  SetQuicFlag(FLAGS_quic_prober_uses_length_prefixed_connection_ids, false);

  SavingWriter* saving_writer = new SavingWriter();
  // dispatcher_ takes ownership of saving_writer.
  QuicDispatcherPeer::UseWriter(dispatcher_.get(), saving_writer);

  QuicTimeWaitListManager* time_wait_list_manager = new QuicTimeWaitListManager(
      saving_writer, dispatcher_.get(), mock_helper_.GetClock(),
      &mock_alarm_factory_);
  // dispatcher_ takes ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                             time_wait_list_manager);
  char packet[1200] = {};
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  QuicEncryptedPacket encrypted(packet, sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
  ASSERT_EQ(1u, saving_writer->packets()->size());

  char source_connection_id_bytes[255] = {};
  uint8_t source_connection_id_length = 0;
  std::string detailed_error = "foobar";
  EXPECT_TRUE(QuicFramer::ParseServerVersionNegotiationProbeResponse(
      (*(saving_writer->packets()))[0]->data(),
      (*(saving_writer->packets()))[0]->length(), source_connection_id_bytes,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ("", detailed_error);

  // The source connection ID of the probe response should match the
  // destination connection ID of the probe request.
  quiche::test::CompareCharArraysWithHexError(
      "parsed probe", source_connection_id_bytes, source_connection_id_length,
      destination_connection_id_bytes, sizeof(destination_connection_id_bytes));
}

TEST_P(QuicDispatcherTestOneVersion, VersionNegotiationProbeEndToEnd) {
  SetQuicFlag(FLAGS_quic_prober_uses_length_prefixed_connection_ids, true);

  SavingWriter* saving_writer = new SavingWriter();
  // dispatcher_ takes ownership of saving_writer.
  QuicDispatcherPeer::UseWriter(dispatcher_.get(), saving_writer);

  QuicTimeWaitListManager* time_wait_list_manager = new QuicTimeWaitListManager(
      saving_writer, dispatcher_.get(), mock_helper_.GetClock(),
      &mock_alarm_factory_);
  // dispatcher_ takes ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                             time_wait_list_manager);
  char packet[1200] = {};
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  QuicEncryptedPacket encrypted(packet, sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
  ASSERT_EQ(1u, saving_writer->packets()->size());

  char source_connection_id_bytes[255] = {};
  uint8_t source_connection_id_length = 0;
  std::string detailed_error = "foobar";
  EXPECT_TRUE(QuicFramer::ParseServerVersionNegotiationProbeResponse(
      (*(saving_writer->packets()))[0]->data(),
      (*(saving_writer->packets()))[0]->length(), source_connection_id_bytes,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ("", detailed_error);

  // The source connection ID of the probe response should match the
  // destination connection ID of the probe request.
  quiche::test::CompareCharArraysWithHexError(
      "parsed probe", source_connection_id_bytes, source_connection_id_length,
      destination_connection_id_bytes, sizeof(destination_connection_id_bytes));
}

TEST_P(QuicDispatcherTestOneVersion, AndroidConformanceTestOld) {
  if (GetQuicReloadableFlag(quic_remove_android_conformance_test_workaround)) {
    // TODO(b/139691956) Remove this test once the flag is deprecated.
    return;
  }
  SavingWriter* saving_writer = new SavingWriter();
  // dispatcher_ takes ownership of saving_writer.
  QuicDispatcherPeer::UseWriter(dispatcher_.get(), saving_writer);

  QuicTimeWaitListManager* time_wait_list_manager = new QuicTimeWaitListManager(
      saving_writer, dispatcher_.get(), mock_helper_.GetClock(),
      &mock_alarm_factory_);
  // dispatcher_ takes ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                             time_wait_list_manager);
  // clang-format off
  static const unsigned char packet[] = {
    // Android UDP network conformance test packet as it was before this change:
    // https://android-review.googlesource.com/c/platform/cts/+/1104285
    0x0c,  // public flags: 8-byte connection ID, 1-byte packet number
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  // 8-byte connection ID
    0x01,  // 1-byte packet number
    0x00,  // private flags
    0x07,  // PING frame
  };
  // clang-format on

  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
  ASSERT_EQ(1u, saving_writer->packets()->size());

  // The Android UDP network conformance test directly checks that bytes 1-9
  // of the response match the connection ID that was sent.
  static const char connection_id_bytes[] = {0x71, 0x72, 0x73, 0x74,
                                             0x75, 0x76, 0x77, 0x78};
  ASSERT_GE((*(saving_writer->packets()))[0]->length(),
            1u + sizeof(connection_id_bytes));
  quiche::test::CompareCharArraysWithHexError(
      "response connection ID", &(*(saving_writer->packets()))[0]->data()[1],
      sizeof(connection_id_bytes), connection_id_bytes,
      sizeof(connection_id_bytes));
}

TEST_P(QuicDispatcherTestOneVersion, AndroidConformanceTest) {
  // WARNING: do not remove or modify this test without making sure that we
  // still have adequate coverage for the Android conformance test.
  SavingWriter* saving_writer = new SavingWriter();
  // dispatcher_ takes ownership of saving_writer.
  QuicDispatcherPeer::UseWriter(dispatcher_.get(), saving_writer);

  QuicTimeWaitListManager* time_wait_list_manager = new QuicTimeWaitListManager(
      saving_writer, dispatcher_.get(), mock_helper_.GetClock(),
      &mock_alarm_factory_);
  // dispatcher_ takes ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                             time_wait_list_manager);
  // clang-format off
  static const unsigned char packet[1200] = {
    // Android UDP network conformance test packet as it was after this change:
    // https://android-review.googlesource.com/c/platform/cts/+/1104285
    0x0d,  // public flags: version, 8-byte connection ID, 1-byte packet number
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  // 8-byte connection ID
    0xaa, 0xda, 0xca, 0xaa,  // reserved-space version number
    0x01,  // 1-byte packet number
    0x00,  // private flags
    0x07,  // PING frame
  };
  // clang-format on

  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
  ASSERT_EQ(1u, saving_writer->packets()->size());

  // The Android UDP network conformance test directly checks that bytes 1-9
  // of the response match the connection ID that was sent.
  static const char connection_id_bytes[] = {0x71, 0x72, 0x73, 0x74,
                                             0x75, 0x76, 0x77, 0x78};
  ASSERT_GE((*(saving_writer->packets()))[0]->length(),
            1u + sizeof(connection_id_bytes));
  quiche::test::CompareCharArraysWithHexError(
      "response connection ID", &(*(saving_writer->packets()))[0]->data()[1],
      sizeof(connection_id_bytes), connection_id_bytes,
      sizeof(connection_id_bytes));
}

TEST_P(QuicDispatcherTestAllVersions, DoNotProcessSmallPacket) {
  if (!version_.HasIetfInvariantHeader()) {
    // We only drop small packets when using IETF_QUIC_LONG_HEADER_PACKET.
    return;
  }
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, SendPacket(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);
  ProcessPacket(client_address, TestConnectionId(1), /*has_version_flag=*/true,
                version_, SerializeCHLO(), /*full_padding=*/false,
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER, 1);
}

TEST_P(QuicDispatcherTestAllVersions, ProcessSmallCoalescedPacket) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*time_wait_list_manager_, SendPacket(_, _, _)).Times(0);

  // clang-format off
  char coalesced_packet[1200] = {
    // first coalesced packet
      // public flags (long header with packet type INITIAL and
      // 4-byte packet number)
      0xC3,
      // version
      'Q', '0', '9', '9',
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x05,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // Padding
      0x00,
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xC3,
      // version
      'Q', '0', '9', '9',
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
  };
  // clang-format on
  QuicReceivedPacket packet(coalesced_packet, 1200, QuicTime::Zero());
  dispatcher_->ProcessPacket(server_address_, client_address, packet);
}

TEST_P(QuicDispatcherTestAllVersions, StopAcceptingNewConnections) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  dispatcher_->StopAcceptingNewConnections();
  EXPECT_FALSE(dispatcher_->accept_new_connections());

  // No more new connections afterwards.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(2), client_address,
                                Eq(ExpectedAlpn()), _))
      .Times(0u);
  ProcessFirstFlight(client_address, TestConnectionId(2));

  // Existing connections should be able to continue.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1u)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessPacket(client_address, TestConnectionId(1), false, "data");
}

TEST_P(QuicDispatcherTestAllVersions, StartAcceptingNewConnections) {
  dispatcher_->StopAcceptingNewConnections();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  // No more new connections afterwards.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(2), client_address,
                                Eq(ExpectedAlpn()), _))
      .Times(0u);
  ProcessFirstFlight(client_address, TestConnectionId(2));

  dispatcher_->StartAcceptingNewConnections();
  EXPECT_TRUE(dispatcher_->accept_new_connections());

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), client_address,
                                Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(1));
}

TEST_P(QuicDispatcherTestOneVersion, SelectAlpn) {
  EXPECT_EQ(QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {}), "");
  EXPECT_EQ(QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {""}), "");
  EXPECT_EQ(QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {"hq"}), "hq");
  // Q033 is no longer supported but Q050 is.
  QuicEnableVersion(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_50));
  EXPECT_EQ(
      QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {"h3-Q033", "h3-Q050"}),
      "h3-Q050");
}

// Verify the stopgap test: Packets with truncated connection IDs should be
// dropped.
class QuicDispatcherTestStrayPacketConnectionId
    : public QuicDispatcherTestBase {};

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsStrayPacketConnectionId,
                         QuicDispatcherTestStrayPacketConnectionId,
                         ::testing::ValuesIn(CurrentSupportedVersions()),
                         ::testing::PrintToStringParamName());

// Packets with truncated connection IDs should be dropped.
TEST_P(QuicDispatcherTestStrayPacketConnectionId,
       StrayPacketTruncatedConnectionId) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              AddConnectionIdToTimeWait(_, _, _, _, _))
      .Times(0);

  ProcessPacket(client_address, connection_id, true, "data",
                CONNECTION_ID_ABSENT, PACKET_4BYTE_PACKET_NUMBER);
}

class BlockingWriter : public QuicPacketWriterWrapper {
 public:
  BlockingWriter() : write_blocked_(false) {}

  bool IsWriteBlocked() const override { return write_blocked_; }
  void SetWritable() override { write_blocked_ = false; }

  WriteResult WritePacket(const char* /*buffer*/,
                          size_t /*buf_len*/,
                          const QuicIpAddress& /*self_client_address*/,
                          const QuicSocketAddress& /*peer_client_address*/,
                          PerPacketOptions* /*options*/) override {
    // It would be quite possible to actually implement this method here with
    // the fake blocked status, but it would be significantly more work in
    // Chromium, and since it's not called anyway, don't bother.
    QUIC_LOG(DFATAL) << "Not supported";
    return WriteResult();
  }

  bool write_blocked_;
};

class QuicDispatcherWriteBlockedListTest : public QuicDispatcherTestBase {
 public:
  void SetUp() override {
    QuicDispatcherTestBase::SetUp();
    writer_ = new BlockingWriter;
    QuicDispatcherPeer::UseWriter(dispatcher_.get(), writer_);

    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(_, client_address, Eq(ExpectedAlpn()), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(1), client_address,
            &helper_, &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(TestConnectionId(1), packet);
        })));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(
                    ReceivedPacketInfoConnectionIdEquals(TestConnectionId(1))));
    ProcessFirstFlight(client_address, TestConnectionId(1));

    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(_, client_address, Eq(ExpectedAlpn()), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(2), client_address,
            &helper_, &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(TestConnectionId(2), packet);
        })));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(
                    ReceivedPacketInfoConnectionIdEquals(TestConnectionId(2))));
    ProcessFirstFlight(client_address, TestConnectionId(2));

    blocked_list_ = QuicDispatcherPeer::GetWriteBlockedList(dispatcher_.get());
  }

  void TearDown() override {
    if (connection1() != nullptr) {
      EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
    }

    if (connection2() != nullptr) {
      EXPECT_CALL(*connection2(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
    }
    dispatcher_->Shutdown();
  }

  // Set the dispatcher's writer to be blocked. By default, all connections use
  // the same writer as the dispatcher in this test.
  void SetBlocked() {
    QUIC_LOG(INFO) << "set writer " << writer_ << " to blocked";
    writer_->write_blocked_ = true;
  }

  // Simulate what happens when connection1 gets blocked when writing.
  void BlockConnection1() {
    Connection1Writer()->write_blocked_ = true;
    dispatcher_->OnWriteBlocked(connection1());
  }

  BlockingWriter* Connection1Writer() {
    return static_cast<BlockingWriter*>(connection1()->writer());
  }

  // Simulate what happens when connection2 gets blocked when writing.
  void BlockConnection2() {
    Connection2Writer()->write_blocked_ = true;
    dispatcher_->OnWriteBlocked(connection2());
  }

  BlockingWriter* Connection2Writer() {
    return static_cast<BlockingWriter*>(connection2()->writer());
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  BlockingWriter* writer_;
  QuicDispatcher::WriteBlockedList* blocked_list_;
};

INSTANTIATE_TEST_SUITE_P(QuicDispatcherWriteBlockedListTests,
                         QuicDispatcherWriteBlockedListTest,
                         ::testing::Values(CurrentSupportedVersions().front()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicDispatcherWriteBlockedListTest, BasicOnCanWrite) {
  // No OnCanWrite calls because no connections are blocked.
  dispatcher_->OnCanWrite();

  // Register connection 1 for events, and make sure it's notified.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // It should get only one notification.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteOrder) {
  // Make sure we handle events in order.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // Check the other ordering.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection2());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_->OnCanWrite();
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteRemove) {
  // Add and remove one connction.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // Add and remove one connction and make sure it doesn't affect others.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // Add it, remove it, and add it back and make sure things are OK.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_->OnCanWrite();
}

TEST_P(QuicDispatcherWriteBlockedListTest, DoubleAdd) {
  // Make sure a double add does not necessitate a double remove.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // Make sure a double add does not result in two OnCanWrite calls.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_->OnCanWrite();
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlockConnection1) {
  // If the 1st blocked writer gets blocked in OnCanWrite, it will be added back
  // into the write blocked list.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection1));
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // connection1 should be still in the write blocked list.
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, connection1 should get its second chance.
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlockConnection2) {
  // If the 2nd blocked writer gets blocked in OnCanWrite, it will be added back
  // into the write blocked list.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_->OnCanWrite();

  // connection2 should be still in the write blocked list.
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, connection2 should get its second chance.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest,
       OnCanWriteHandleBlockBothConnections) {
  // Both connections get blocked in OnCanWrite, and added back into the write
  // blocked list.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection1));
  EXPECT_CALL(*connection2(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_->OnCanWrite();

  // Both connections should be still in the write blocked list.
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, both connections should get its second chance.
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest, PerConnectionWriterBlocked) {
  // By default, all connections share the same packet writer with the
  // dispatcher.
  EXPECT_EQ(dispatcher_->writer(), connection1()->writer());
  EXPECT_EQ(dispatcher_->writer(), connection2()->writer());

  // Test the case where connection1 shares the same packet writer as the
  // dispatcher, whereas connection2 owns it's packet writer.
  // Change connection2's writer.
  connection2()->SetQuicPacketWriter(new BlockingWriter, /*owns_writer=*/true);
  EXPECT_NE(dispatcher_->writer(), connection2()->writer());

  BlockConnection2();
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest,
       RemoveConnectionFromWriteBlockedListWhenDeletingSessions) {
  dispatcher_->OnConnectionClosed(connection1()->connection_id(),
                                  QUIC_PACKET_WRITE_ERROR, "Closed by test.",
                                  ConnectionCloseSource::FROM_SELF);

  SetBlocked();

  ASSERT_FALSE(dispatcher_->HasPendingWrites());
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  ASSERT_TRUE(dispatcher_->HasPendingWrites());

  EXPECT_QUIC_BUG(dispatcher_->DeleteSessions(),
                  "QuicConnection was in WriteBlockedList before destruction");
  MarkSession1Deleted();
}

class BufferedPacketStoreTest : public QuicDispatcherTestBase {
 public:
  BufferedPacketStoreTest()
      : QuicDispatcherTestBase(),
        client_addr_(QuicIpAddress::Loopback4(), 1234) {}

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    QuicDispatcherTestBase::ProcessFirstFlight(version, peer_address,
                                               server_connection_id);
  }

  void ProcessFirstFlight(const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version_, peer_address, server_connection_id);
  }

  void ProcessFirstFlight(const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(client_addr_, server_connection_id);
  }

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version, client_addr_, server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const ParsedQuicVersion& version,
      const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    QuicDispatcherTestBase::ProcessUndecryptableEarlyPacket(
        version, peer_address, server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    ProcessUndecryptableEarlyPacket(version_, peer_address,
                                    server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const QuicConnectionId& server_connection_id) {
    ProcessUndecryptableEarlyPacket(version_, client_addr_,
                                    server_connection_id);
  }

 protected:
  QuicSocketAddress client_addr_;
};

INSTANTIATE_TEST_SUITE_P(BufferedPacketStoreTests,
                         BufferedPacketStoreTest,
                         ::testing::ValuesIn(CurrentSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(BufferedPacketStoreTest, ProcessNonChloPacketBeforeChlo) {
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  // Non-CHLO should be buffered upon arrival, and should trigger
  // ShouldCreateOrBufferPacketForConnection().
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(
                                ReceivedPacketInfoConnectionIdEquals(conn_id)));
  // Process non-CHLO packet.
  ProcessUndecryptableEarlyPacket(conn_id);
  EXPECT_EQ(0u, dispatcher_->session_map().size())
      << "No session should be created before CHLO arrives.";

  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_addr_, Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2)  // non-CHLO + CHLO.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(conn_id, packet);
            }
          })));
  ProcessFirstFlight(conn_id);
}

TEST_P(BufferedPacketStoreTest, ProcessNonChloPacketsUptoLimitAndProcessChlo) {
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  // A bunch of non-CHLO should be buffered upon arrival, and the first one
  // should trigger ShouldCreateOrBufferPacketForConnection().
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(
                                ReceivedPacketInfoConnectionIdEquals(conn_id)));
  for (size_t i = 1; i <= kDefaultMaxUndecryptablePackets + 1; ++i) {
    ProcessUndecryptableEarlyPacket(conn_id);
  }
  EXPECT_EQ(0u, dispatcher_->session_map().size())
      << "No session should be created before CHLO arrives.";

  // Pop out the last packet as it is also be dropped by the store.
  data_connection_map_[conn_id].pop_back();
  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_addr_, Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));

  // Only |kDefaultMaxUndecryptablePackets| packets were buffered, and they
  // should be delivered in arrival order.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(kDefaultMaxUndecryptablePackets + 1)  // + 1 for CHLO.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(conn_id, packet);
            }
          })));
  ProcessFirstFlight(conn_id);
}

TEST_P(BufferedPacketStoreTest,
       ProcessNonChloPacketsForDifferentConnectionsUptoLimit) {
  InSequence s;
  // A bunch of non-CHLO should be buffered upon arrival.
  size_t kNumConnections = kMaxConnectionsWithoutCHLO + 1;
  for (size_t i = 1; i <= kNumConnections; ++i) {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), i);
    QuicConnectionId conn_id = TestConnectionId(i);
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(
                    ReceivedPacketInfoConnectionIdEquals(conn_id)));
    ProcessUndecryptableEarlyPacket(client_address, conn_id);
  }

  // Pop out the packet on last connection as it shouldn't be enqueued in store
  // as well.
  data_connection_map_[TestConnectionId(kNumConnections)].pop_front();

  // Reset session creation counter to ensure processing CHLO can always
  // create session.
  QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(dispatcher_.get(),
                                                              kNumConnections);
  // Process CHLOs to create session for these connections.
  for (size_t i = 1; i <= kNumConnections; ++i) {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), i);
    QuicConnectionId conn_id = TestConnectionId(i);
    if (i == kNumConnections) {
      EXPECT_CALL(*dispatcher_,
                  ShouldCreateOrBufferPacketForConnection(
                      ReceivedPacketInfoConnectionIdEquals(conn_id)));
    }
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, client_address,
                                                Eq(ExpectedAlpn()), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, conn_id, client_address, &mock_helper_,
            &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    // First |kNumConnections| - 1 connections should have buffered
    // a packet in store. The rest should have been dropped.
    size_t num_packet_to_process = i <= kMaxConnectionsWithoutCHLO ? 2u : 1u;
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, client_address, _))
        .Times(num_packet_to_process)
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(conn_id, packet);
              }
            })));

    ProcessFirstFlight(client_address, conn_id);
  }
}

// Tests that store delivers empty packet list if CHLO arrives firstly.
TEST_P(BufferedPacketStoreTest, DeliverEmptyPackets) {
  QuicConnectionId conn_id = TestConnectionId(1);
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(
                                ReceivedPacketInfoConnectionIdEquals(conn_id)));
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_addr_, Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, client_addr_, _));
  ProcessFirstFlight(conn_id);
}

// Tests that a retransmitted CHLO arrives after a connection for the
// CHLO has been created.
TEST_P(BufferedPacketStoreTest, ReceiveRetransmittedCHLO) {
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  ProcessUndecryptableEarlyPacket(conn_id);

  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_addr_, Eq(ExpectedAlpn()), _))
      .Times(1)  // Only triggered by 1st CHLO.
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(3)  // Triggered by 1 data packet and 2 CHLOs.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(conn_id, packet);
            }
          })));

  std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
      GetFirstFlightOfPackets(version_, conn_id);
  ASSERT_EQ(packets.size(), 1u);
  // Receive the CHLO once.
  ProcessReceivedPacket(packets[0]->Clone(), client_addr_, version_, conn_id);
  // Receive the CHLO a second time to simulate retransmission.
  ProcessReceivedPacket(std::move(packets[0]), client_addr_, version_, conn_id);
}

// Tests that expiration of a connection add connection id to time wait list.
TEST_P(BufferedPacketStoreTest, ReceiveCHLOAfterExpiration) {
  InSequence s;
  CreateTimeWaitListManager();
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  QuicBufferedPacketStorePeer::set_clock(store, mock_helper_.GetClock());

  QuicConnectionId conn_id = TestConnectionId(1);
  ProcessPacket(client_addr_, conn_id, true,
                quiche::QuicheStrCat("data packet ", 2), CONNECTION_ID_PRESENT,
                PACKET_4BYTE_PACKET_NUMBER,
                /*packet_number=*/2);

  mock_helper_.AdvanceTime(
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs));
  QuicAlarm* alarm = QuicBufferedPacketStorePeer::expiration_alarm(store);
  // Cancel alarm as if it had been fired.
  alarm->Cancel();
  store->OnExpirationTimeout();
  // New arrived CHLO will be dropped because this connection is in time wait
  // list.
  ASSERT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, conn_id, _, _));
  ProcessFirstFlight(conn_id);
}

TEST_P(BufferedPacketStoreTest, ProcessCHLOsUptoLimitAndBufferTheRest) {
  // Process more than (|kMaxNumSessionsToCreate| +
  // |kDefaultMaxConnectionsInStore|) CHLOs,
  // the first |kMaxNumSessionsToCreate| should create connections immediately,
  // the next |kDefaultMaxConnectionsInStore| should be buffered,
  // the rest should be dropped.
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  const size_t kNumCHLOs =
      kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore + 1;
  for (uint64_t conn_id = 1; conn_id <= kNumCHLOs; ++conn_id) {
    EXPECT_CALL(
        *dispatcher_,
        ShouldCreateOrBufferPacketForConnection(
            ReceivedPacketInfoConnectionIdEquals(TestConnectionId(conn_id))));
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                                    Eq(ExpectedAlpn()), _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
    if (conn_id <= kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore &&
        conn_id > kMaxNumSessionsToCreate) {
      EXPECT_TRUE(store->HasChloForConnection(TestConnectionId(conn_id)));
    } else {
      // First |kMaxNumSessionsToCreate| CHLOs should be passed to new
      // connections immediately, and the last CHLO should be dropped as the
      // store is full.
      EXPECT_FALSE(store->HasChloForConnection(TestConnectionId(conn_id)));
    }
  }

  // Graduately consume buffered CHLOs. The buffered connections should be
  // created but the dropped one shouldn't.
  for (uint64_t conn_id = kMaxNumSessionsToCreate + 1;
       conn_id <= kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore;
       ++conn_id) {
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                                  Eq(ExpectedAlpn()), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(conn_id), client_addr_,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(TestConnectionId(conn_id), packet);
              }
            })));
  }
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(kNumCHLOs), client_addr_,
                                Eq(ExpectedAlpn()), _))
      .Times(0);

  while (store->HasChlosBuffered()) {
    dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
  }

  EXPECT_EQ(TestConnectionId(static_cast<size_t>(kMaxNumSessionsToCreate) +
                             kDefaultMaxConnectionsInStore),
            session1_->connection_id());
}

// Duplicated CHLO shouldn't be buffered.
TEST_P(BufferedPacketStoreTest, BufferDuplicatedCHLO) {
  for (uint64_t conn_id = 1; conn_id <= kMaxNumSessionsToCreate + 1;
       ++conn_id) {
    // Last CHLO will be buffered. Others will create connection right away.
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                                    Eq(ExpectedAlpn()), _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
  }
  // Retransmit CHLO on last connection should be dropped.
  QuicConnectionId last_connection =
      TestConnectionId(kMaxNumSessionsToCreate + 1);
  ProcessFirstFlight(last_connection);

  size_t packets_buffered = 2;

  // Reset counter and process buffered CHLO.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(last_connection, client_addr_,
                                              Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, last_connection, client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  // Only one packet(CHLO) should be process.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(packets_buffered)
      .WillRepeatedly(WithArg<2>(
          Invoke([this, last_connection](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(last_connection, packet);
            }
          })));
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

TEST_P(BufferedPacketStoreTest, BufferNonChloPacketsUptoLimitWithChloBuffered) {
  uint64_t last_conn_id = kMaxNumSessionsToCreate + 1;
  QuicConnectionId last_connection_id = TestConnectionId(last_conn_id);
  for (uint64_t conn_id = 1; conn_id <= last_conn_id; ++conn_id) {
    // Last CHLO will be buffered. Others will create connection right away.
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                                    Eq(ExpectedAlpn()), _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillRepeatedly(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
  }

  // Process another |kDefaultMaxUndecryptablePackets| + 1 data packets. The
  // last one should be dropped.
  for (uint64_t packet_number = 2;
       packet_number <= kDefaultMaxUndecryptablePackets + 2; ++packet_number) {
    ProcessPacket(client_addr_, last_connection_id, true, "data packet");
  }

  // Reset counter and process buffered CHLO.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(last_connection_id, client_addr_,
                                              Eq(ExpectedAlpn()), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, last_connection_id, client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  // Only CHLO and following |kDefaultMaxUndecryptablePackets| data packets
  // should be process.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(kDefaultMaxUndecryptablePackets + 1)
      .WillRepeatedly(WithArg<2>(
          Invoke([this, last_connection_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(last_connection_id, packet);
            }
          })));
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

// Tests that when dispatcher's packet buffer is full, a CHLO on connection
// which doesn't have buffered CHLO should be buffered.
TEST_P(BufferedPacketStoreTest, ReceiveCHLOForBufferedConnection) {
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());

  uint64_t conn_id = 1;
  ProcessUndecryptableEarlyPacket(TestConnectionId(conn_id));
  // Fill packet buffer to full with CHLOs on other connections. Need to feed
  // extra CHLOs because the first |kMaxNumSessionsToCreate| are going to create
  // session directly.
  for (conn_id = 2;
       conn_id <= kDefaultMaxConnectionsInStore + kMaxNumSessionsToCreate;
       ++conn_id) {
    if (conn_id <= kMaxNumSessionsToCreate + 1) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                                    Eq(ExpectedAlpn()), _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
  }
  EXPECT_FALSE(store->HasChloForConnection(
      /*connection_id=*/TestConnectionId(1)));

  // CHLO on connection 1 should still be buffered.
  ProcessFirstFlight(TestConnectionId(1));
  EXPECT_TRUE(store->HasChloForConnection(
      /*connection_id=*/TestConnectionId(1)));
}

// Regression test for b/117874922.
TEST_P(BufferedPacketStoreTest, ProcessBufferedChloWithDifferentVersion) {
  // Ensure the preferred version is not supported by the server.
  SetQuicReloadableFlag(quic_enable_version_draft_27, false);
  uint64_t last_connection_id = kMaxNumSessionsToCreate + 5;
  ParsedQuicVersionVector supported_versions = CurrentSupportedVersions();
  for (uint64_t conn_id = 1; conn_id <= last_connection_id; ++conn_id) {
    // Last 5 CHLOs will be buffered. Others will create connection right away.
    ParsedQuicVersion version =
        supported_versions[(conn_id - 1) % supported_versions.size()];
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(
          *dispatcher_,
          CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                            Eq(ExpectedAlpnForVersion(version)), version))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillRepeatedly(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(version, TestConnectionId(conn_id));
  }

  // Process buffered CHLOs. Verify the version is correct.
  for (uint64_t conn_id = kMaxNumSessionsToCreate + 1;
       conn_id <= last_connection_id; ++conn_id) {
    ParsedQuicVersion version =
        supported_versions[(conn_id - 1) % supported_versions.size()];
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(TestConnectionId(conn_id), client_addr_,
                                  Eq(ExpectedAlpnForVersion(version)), version))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(conn_id), client_addr_,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(TestConnectionId(conn_id), packet);
              }
            })));
  }
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

}  // namespace
}  // namespace test
}  // namespace quic

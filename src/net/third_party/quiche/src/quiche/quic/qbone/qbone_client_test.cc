// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sets up a dispatcher and sends requests via the QboneClient.

#include "quiche/quic/qbone/qbone_client.h"

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_default_packet_writer.h"
#include "quiche/quic/core/quic_dispatcher.h"
#include "quiche/quic/core/quic_epoll_alarm_factory.h"
#include "quiche/quic/core/quic_epoll_connection_helper.h"
#include "quiche/quic/core/quic_packet_reader.h"
#include "quiche/quic/platform/api/quic_mutex.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/qbone/qbone_constants.h"
#include "quiche/quic/qbone/qbone_packet_processor_test_tools.h"
#include "quiche/quic/qbone/qbone_server_session.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_dispatcher_peer.h"
#include "quiche/quic/test_tools/quic_server_peer.h"
#include "quiche/quic/test_tools/server_thread.h"
#include "quiche/quic/tools/quic_memory_cache_backend.h"
#include "quiche/quic/tools/quic_server.h"

namespace quic {
namespace test {
namespace {

using ::testing::ElementsAre;

ParsedQuicVersionVector GetTestParams() {
  ParsedQuicVersionVector test_versions;

  // TODO(b/113130636): Make QBONE work with TLS.
  for (const auto& version : CurrentSupportedVersionsWithQuicCrypto()) {
    // QBONE requires MESSAGE frames
    if (!version.SupportsMessageFrames()) {
      continue;
    }
    test_versions.push_back(version);
  }

  return test_versions;
}

std::string TestPacketIn(const std::string& body) {
  return PrependIPv6HeaderForTest(body, 5);
}

std::string TestPacketOut(const std::string& body) {
  return PrependIPv6HeaderForTest(body, 4);
}

class DataSavingQbonePacketWriter : public QbonePacketWriter {
 public:
  void WritePacketToNetwork(const char* packet, size_t size) override {
    QuicWriterMutexLock lock(&mu_);
    data_.push_back(std::string(packet, size));
  }

  std::vector<std::string> data() {
    QuicWriterMutexLock lock(&mu_);
    return data_;
  }

 private:
  QuicMutex mu_;
  std::vector<std::string> data_;
};

// A subclass of a QBONE session that will own the connection passed in.
class ConnectionOwningQboneServerSession : public QboneServerSession {
 public:
  ConnectionOwningQboneServerSession(
      const ParsedQuicVersionVector& supported_versions,
      QuicConnection* connection, Visitor* owner, const QuicConfig& config,
      const QuicCryptoServerConfig* quic_crypto_server_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      QbonePacketWriter* writer)
      : QboneServerSession(supported_versions, connection, owner, config,
                           quic_crypto_server_config, compressed_certs_cache,
                           writer, TestLoopback6(), TestLoopback6(), 64,
                           nullptr),
        connection_(connection) {}

 private:
  // Note that we don't expect the QboneServerSession or any of its parent
  // classes to do anything with the connection_ in their destructors.
  std::unique_ptr<QuicConnection> connection_;
};

class QuicQboneDispatcher : public QuicDispatcher {
 public:
  QuicQboneDispatcher(
      const QuicConfig* config, const QuicCryptoServerConfig* crypto_config,
      QuicVersionManager* version_manager,
      std::unique_ptr<QuicConnectionHelperInterface> helper,
      std::unique_ptr<QuicCryptoServerStreamBase::Helper> session_helper,
      std::unique_ptr<QuicAlarmFactory> alarm_factory,
      QbonePacketWriter* writer)
      : QuicDispatcher(config, crypto_config, version_manager,
                       std::move(helper), std::move(session_helper),
                       std::move(alarm_factory),
                       kQuicDefaultConnectionIdLength),
        writer_(writer) {}

  std::unique_ptr<QuicSession> CreateQuicSession(
      QuicConnectionId id, const QuicSocketAddress& self_address,
      const QuicSocketAddress& peer_address, absl::string_view alpn,
      const ParsedQuicVersion& version,
      const ParsedClientHello& /*parsed_chlo*/) override {
    QUICHE_CHECK_EQ(alpn, "qbone");
    QuicConnection* connection = new QuicConnection(
        id, self_address, peer_address, helper(), alarm_factory(), writer(),
        /* owns_writer= */ false, Perspective::IS_SERVER,
        ParsedQuicVersionVector{version});
    // The connection owning wrapper owns the connection created.
    auto session = std::make_unique<ConnectionOwningQboneServerSession>(
        GetSupportedVersions(), connection, this, config(), crypto_config(),
        compressed_certs_cache(), writer_);
    session->Initialize();
    return session;
  }

 private:
  QbonePacketWriter* writer_;
};

class QboneTestServer : public QuicServer {
 public:
  explicit QboneTestServer(std::unique_ptr<ProofSource> proof_source)
      : QuicServer(std::move(proof_source), &response_cache_) {}
  QuicDispatcher* CreateQuicDispatcher() override {
    QuicEpollAlarmFactory alarm_factory(epoll_server());
    return new QuicQboneDispatcher(
        &config(), &crypto_config(), version_manager(),
        std::unique_ptr<QuicEpollConnectionHelper>(
            new QuicEpollConnectionHelper(epoll_server(),
                                          QuicAllocator::BUFFER_POOL)),
        std::unique_ptr<QuicCryptoServerStreamBase::Helper>(
            new QboneCryptoServerStreamHelper()),
        std::unique_ptr<QuicEpollAlarmFactory>(
            new QuicEpollAlarmFactory(epoll_server())),
        &writer_);
  }

  std::vector<std::string> data() { return writer_.data(); }

 private:
  quic::QuicMemoryCacheBackend response_cache_;
  DataSavingQbonePacketWriter writer_;
};

class QboneTestClient : public QboneClient {
 public:
  QboneTestClient(QuicSocketAddress server_address,
                  const QuicServerId& server_id,
                  const ParsedQuicVersionVector& supported_versions,
                  QuicEpollServer* epoll_server,
                  std::unique_ptr<ProofVerifier> proof_verifier)
      : QboneClient(server_address, server_id, supported_versions,
                    /*session_owner=*/nullptr, QuicConfig(), epoll_server,
                    std::move(proof_verifier), &qbone_writer_, nullptr) {}

  ~QboneTestClient() override {}

  void SendData(const std::string& data) {
    qbone_session()->ProcessPacketFromNetwork(data);
  }

  void WaitForWriteToFlush() {
    while (connected() && session()->HasDataToWrite()) {
      WaitForEvents();
    }
  }

  // Returns true when the data size is reached or false on timeouts.
  bool WaitForDataSize(int n, QuicTime::Delta timeout) {
    const QuicClock* clock =
        quic::test::QuicConnectionPeer::GetHelper(session()->connection())
            ->GetClock();
    const QuicTime deadline = clock->Now() + timeout;
    while (data().size() < n) {
      if (clock->Now() > deadline) {
        return false;
      }
      WaitForEvents();
    }
    return true;
  }

  std::vector<std::string> data() { return qbone_writer_.data(); }

 private:
  DataSavingQbonePacketWriter qbone_writer_;
};

class QboneClientTest : public QuicTestWithParam<ParsedQuicVersion> {};

INSTANTIATE_TEST_SUITE_P(Tests, QboneClientTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QboneClientTest, SendDataFromClient) {
  auto server = std::make_unique<QboneTestServer>(
      crypto_test_utils::ProofSourceForTesting());
  QboneTestServer* server_ptr = server.get();
  QuicSocketAddress server_address(TestLoopback(), 0);
  ServerThread server_thread(std::move(server), server_address);
  server_thread.Initialize();
  server_address =
      QuicSocketAddress(server_address.host(), server_thread.GetPort());
  server_thread.Start();

  QuicEpollServer epoll_server;
  QboneTestClient client(
      server_address,
      QuicServerId("test.example.com", server_address.port(), false),
      ParsedQuicVersionVector{GetParam()}, &epoll_server,
      crypto_test_utils::ProofVerifierForTesting());
  ASSERT_TRUE(client.Initialize());
  ASSERT_TRUE(client.Connect());
  ASSERT_TRUE(client.WaitForOneRttKeysAvailable());
  client.SendData(TestPacketIn("hello"));
  client.SendData(TestPacketIn("world"));
  client.WaitForWriteToFlush();

  // Wait until the server has received at least two packets, timeout after 5s.
  ASSERT_TRUE(
      server_thread.WaitUntil([&] { return server_ptr->data().size() >= 2; },
                              QuicTime::Delta::FromSeconds(5)));

  // Pretend the server gets data.
  std::string long_data(1000, 'A');
  server_thread.Schedule([server_ptr, &long_data]() {
    EXPECT_THAT(server_ptr->data(),
                ElementsAre(TestPacketOut("hello"), TestPacketOut("world")));
    auto server_session = static_cast<QboneServerSession*>(
        QuicDispatcherPeer::GetFirstSessionIfAny(
            QuicServerPeer::GetDispatcher(server_ptr)));
    server_session->ProcessPacketFromNetwork(
        TestPacketIn("Somethingsomething"));
    server_session->ProcessPacketFromNetwork(TestPacketIn(long_data));
    server_session->ProcessPacketFromNetwork(TestPacketIn(long_data));
  });

  EXPECT_TRUE(client.WaitForDataSize(3, QuicTime::Delta::FromSeconds(5)));
  EXPECT_THAT(client.data(),
              ElementsAre(TestPacketOut("Somethingsomething"),
                          TestPacketOut(long_data), TestPacketOut(long_data)));

  client.Disconnect();
  server_thread.Quit();
  server_thread.Join();
}

}  // namespace
}  // namespace test
}  // namespace quic

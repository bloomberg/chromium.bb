// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <memory>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/properties_based_quic_server_info.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_stream_factory_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quiche/src/quic/core/quic_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/spdy/core/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;

namespace net {

namespace test {

namespace {

enum DestinationType {
  // In pooling tests with two requests for different origins to the same
  // destination, the destination should be
  SAME_AS_FIRST,   // the same as the first origin,
  SAME_AS_SECOND,  // the same as the second origin, or
  DIFFERENT,       // different from both.
};

const char kDefaultServerHostName[] = "www.example.org";
const char kServer2HostName[] = "mail.example.org";
const char kDifferentHostname[] = "different.example.com";
const int kDefaultServerPort = 443;
const char kDefaultUrl[] = "https://www.example.org/";
const char kServer2Url[] = "https://mail.example.org/";
const char kServer3Url[] = "https://docs.example.org/";
const char kServer4Url[] = "https://images.example.org/";
const int kDefaultRTTMilliSecs = 300;
const size_t kMinRetryTimeForDefaultNetworkSecs = 1;
const size_t kWaitTimeForNewNetworkSecs = 10;
const IPAddress kCachedIPAddress = IPAddress(192, 168, 0, 2);
const char kNonCachedIPAddress[] = "192.168.0.1";

// Run QuicStreamFactoryTest instances with all value combinations of version
// and enable_connection_racting.
struct TestParams {
  quic::ParsedQuicVersion version;
  bool client_headers_include_h2_stream_dependency;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return quiche::QuicheStrCat(
      ParsedQuicVersionToString(p.version), "_",
      (p.client_headers_include_h2_stream_dependency ? "" : "No"),
      "Dependency");
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      quic::AllSupportedVersions();
  for (const auto& version : all_supported_versions) {
      params.push_back(TestParams{version, false});
      params.push_back(TestParams{version, true});
  }
  return params;
}

// Run QuicStreamFactoryWithDestinationTest instances with all value
// combinations of version, enable_connection_racting, and destination_type.
struct PoolingTestParams {
  quic::ParsedQuicVersion version;
  DestinationType destination_type;
  bool client_headers_include_h2_stream_dependency;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const PoolingTestParams& p) {
  const char* destination_string = "";
  switch (p.destination_type) {
    case SAME_AS_FIRST:
      destination_string = "SAME_AS_FIRST";
      break;
    case SAME_AS_SECOND:
      destination_string = "SAME_AS_SECOND";
      break;
    case DIFFERENT:
      destination_string = "DIFFERENT";
      break;
  }
  return quiche::QuicheStrCat(
      ParsedQuicVersionToString(p.version), "_", destination_string, "_",
      (p.client_headers_include_h2_stream_dependency ? "" : "No"),
      "Dependency");
}

std::vector<PoolingTestParams> GetPoolingTestParams() {
  std::vector<PoolingTestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      quic::AllSupportedVersions();
  for (const quic::ParsedQuicVersion& version : all_supported_versions) {
    params.push_back(PoolingTestParams{version, SAME_AS_FIRST, false});
    params.push_back(PoolingTestParams{version, SAME_AS_FIRST, true});
    params.push_back(PoolingTestParams{version, SAME_AS_SECOND, false});
    params.push_back(PoolingTestParams{version, SAME_AS_SECOND, true});
    params.push_back(PoolingTestParams{version, DIFFERENT, false});
    params.push_back(PoolingTestParams{version, DIFFERENT, true});
  }
  return params;
}

}  // namespace

class QuicHttpStreamPeer {
 public:
  static QuicChromiumClientSession::Handle* GetSessionHandle(
      HttpStream* stream) {
    return static_cast<QuicHttpStream*>(stream)->quic_session();
  }
};

// TestMigrationSocketFactory will vend sockets with incremental port number.
class TestMigrationSocketFactory : public MockClientSocketFactory {
 public:
  TestMigrationSocketFactory() : next_source_port_num_(1u) {}
  ~TestMigrationSocketFactory() override {}

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    SocketDataProvider* data_provider = mock_data().GetNext();
    std::unique_ptr<MockUDPClientSocket> socket(
        new MockUDPClientSocket(data_provider, net_log));
    socket->set_source_port(next_source_port_num_++);
    return std::move(socket);
  }

 private:
  uint16_t next_source_port_num_;

  DISALLOW_COPY_AND_ASSIGN(TestMigrationSocketFactory);
};

class QuicStreamFactoryTestBase : public WithTaskEnvironment {
 protected:
  QuicStreamFactoryTestBase(quic::ParsedQuicVersion version,
                            bool client_headers_include_h2_stream_dependency)
      : host_resolver_(new MockHostResolver),
        ssl_config_service_(new SSLConfigServiceDefaults),
        socket_factory_(new MockClientSocketFactory),
        runner_(new TestTaskRunner(context_.mock_clock())),
        version_(version),
        client_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(
                          context_.random_generator()),
                      context_.clock(),
                      kDefaultServerHostName,
                      quic::Perspective::IS_CLIENT,
                      client_headers_include_h2_stream_dependency),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(
                          context_.random_generator()),
                      context_.clock(),
                      kDefaultServerHostName,
                      quic::Perspective::IS_SERVER,
                      false),
        http_server_properties_(std::make_unique<HttpServerProperties>()),
        cert_verifier_(std::make_unique<MockCertVerifier>()),
        cert_transparency_verifier_(std::make_unique<DoNothingCTVerifier>()),
        scoped_mock_network_change_notifier_(nullptr),
        factory_(nullptr),
        host_port_pair_(kDefaultServerHostName, kDefaultServerPort),
        url_(kDefaultUrl),
        url2_(kServer2Url),
        url3_(kServer3Url),
        url4_(kServer4Url),
        privacy_mode_(PRIVACY_MODE_DISABLED),
        failed_on_default_network_callback_(base::BindRepeating(
            &QuicStreamFactoryTestBase::OnFailedOnDefaultNetwork,
            base::Unretained(this))),
        failed_on_default_network_(false),
        quic_params_(context_.params()) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic_params_->headers_include_h2_stream_dependency =
        client_headers_include_h2_stream_dependency;
    context_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void Initialize() {
    DCHECK(!factory_);
    factory_ = std::make_unique<QuicStreamFactory>(
        net_log_.net_log(), host_resolver_.get(), ssl_config_service_.get(),
        socket_factory_.get(), http_server_properties_.get(),
        cert_verifier_.get(), &ct_policy_enforcer_, &transport_security_state_,
        cert_transparency_verifier_.get(),
        /*SocketPerformanceWatcherFactory*/ nullptr,
        &crypto_client_stream_factory_, &context_);
  }

  void InitializeConnectionMigrationV2Test(
      NetworkChangeNotifier::NetworkList connected_networks) {
    scoped_mock_network_change_notifier_.reset(
        new ScopedMockNetworkChangeNotifier());
    MockNetworkChangeNotifier* mock_ncn =
        scoped_mock_network_change_notifier_->mock_network_change_notifier();
    mock_ncn->ForceNetworkHandlesSupported();
    mock_ncn->SetConnectedNetworksList(connected_networks);
    quic_params_->migrate_sessions_on_network_change_v2 = true;
    quic_params_->migrate_sessions_early_v2 = true;
    quic_params_->allow_port_migration = false;
    socket_factory_.reset(new TestMigrationSocketFactory);
    Initialize();
  }

  std::unique_ptr<HttpStream> CreateStream(QuicStreamRequest* request) {
    std::unique_ptr<QuicChromiumClientSession::Handle> session =
        request->ReleaseSessionHandle();
    if (!session || !session->IsConnected())
      return nullptr;

    return std::make_unique<QuicHttpStream>(std::move(session));
  }

  bool HasActiveSession(const HostPortPair& host_port_pair,
                        const NetworkIsolationKey& network_isolation_key =
                            NetworkIsolationKey()) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::HasActiveSession(factory_.get(), server_id,
                                                   network_isolation_key);
  }

  bool HasLiveSession(const HostPortPair& host_port_pair) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::HasLiveSession(factory_.get(), host_port_pair,
                                                 server_id);
  }

  bool HasActiveJob(const HostPortPair& host_port_pair,
                    const PrivacyMode privacy_mode) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 privacy_mode == PRIVACY_MODE_ENABLED);
    return QuicStreamFactoryPeer::HasActiveJob(factory_.get(), server_id);
  }

  // Get the pending, not activated session, if there is only one session alive.
  QuicChromiumClientSession* GetPendingSession(
      const HostPortPair& host_port_pair) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::GetPendingSession(factory_.get(), server_id,
                                                    host_port_pair);
  }

  QuicChromiumClientSession* GetActiveSession(
      const HostPortPair& host_port_pair,
      const NetworkIsolationKey& network_isolation_key =
          NetworkIsolationKey()) {
    quic::QuicServerId server_id(host_port_pair.host(), host_port_pair.port(),
                                 false);
    return QuicStreamFactoryPeer::GetActiveSession(factory_.get(), server_id,
                                                   network_isolation_key);
  }

  int GetSourcePortForNewSession(const HostPortPair& destination) {
    return GetSourcePortForNewSessionInner(destination, false);
  }

  int GetSourcePortForNewSessionAndGoAway(const HostPortPair& destination) {
    return GetSourcePortForNewSessionInner(destination, true);
  }

  int GetSourcePortForNewSessionInner(const HostPortPair& destination,
                                      bool goaway_received) {
    // Should only be called if there is no active session for this destination.
    EXPECT_FALSE(HasActiveSession(destination));
    size_t socket_count = socket_factory_->udp_client_socket_ports().size();

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (VersionUsesHttp3(version_.transport_version))
      socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    GURL url("https://" + destination.host() + "/");
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
            NetworkIsolationKey(), false /* disable_secure_dns */,
            /*cert_verify_flags=*/0, url, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsOk());
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());
    stream.reset();

    QuicChromiumClientSession* session = GetActiveSession(destination);

    if (socket_count + 1 != socket_factory_->udp_client_socket_ports().size()) {
      ADD_FAILURE();
      return 0;
    }

    if (goaway_received) {
      quic::QuicGoAwayFrame goaway(quic::kInvalidControlFrameId,
                                   quic::QUIC_NO_ERROR, 1, "");
      session->connection()->OnGoAwayFrame(goaway);
    }

    factory_->OnSessionClosed(session);
    EXPECT_FALSE(HasActiveSession(destination));
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
    return socket_factory_->udp_client_socket_ports()[socket_count];
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientConnectionClosePacket(uint64_t num) {
    return client_maker_.MakeConnectionClosePacket(
        num, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!");
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code) {
    quic::QuicStreamId stream_id =
        GetNthClientInitiatedBidirectionalStreamId(0);
    return client_maker_.MakeRstPacket(packet_number, true, stream_id,
                                       error_code);
  }

  static ProofVerifyDetailsChromium DefaultProofVerifyDetails() {
    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());
    ProofVerifyDetailsChromium verify_details;
    verify_details.cert_verify_result.verified_cert = test_cert;
    verify_details.cert_verify_result.is_issued_by_known_root = true;
    return verify_details;
  }

  void NotifyIPAddressChanged() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Spin the message loop so the notification is delivered.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructGetRequestPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin) {
    spdy::SpdyHeaderBlock headers =
        client_maker_.GetRequestHeaders("GET", "https", "/");
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, stream_id, should_include_version, fin, priority,
        std::move(headers), 0, &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructGetRequestPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      quic::QuicStreamId parent_stream_id,
      bool should_include_version,
      bool fin) {
    spdy::SpdyHeaderBlock headers =
        client_maker_.GetRequestHeaders("GET", "https", "/");
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    return client_maker_.MakeRequestHeadersPacket(
        packet_number, stream_id, should_include_version, fin, priority,
        std::move(headers), parent_stream_id, &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructOkResponsePacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin) {
    spdy::SpdyHeaderBlock headers = server_maker_.GetResponseHeaders("200 OK");
    size_t spdy_headers_frame_len;
    return server_maker_.MakeResponseHeadersPacket(
        packet_number, stream_id, should_include_version, fin,
        std::move(headers), &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket() {
    return client_maker_.MakeInitialSettingsPacket(1);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number);
  }

  // Helper method for server migration tests.
  void VerifyServerMigration(const quic::QuicConfig& config,
                             IPEndPoint expected_address) {
    quic_params_->allow_server_migration = true;
    Initialize();

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.SetConfig(config);

    // Set up first socket data provider.
    MockQuicData socket_data1(version_);
    socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data1.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after
    // migration.
    MockQuicData socket_data2(version_);
    socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    int packet_num = 1;
    if (VersionUsesHttp3(version_.transport_version)) {
      socket_data2.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    }
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
    if (VersionUsesHttp3(version_.transport_version)) {
      socket_data2.AddWrite(
          SYNCHRONOUS, client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), true, false,
                           StreamCancellationQpackDecoderInstruction(0)));
    }
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, true, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
    socket_data2.AddSocketDataToFactory(socket_factory_.get());

    // Create request and QuicHttpStream.
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));
    EXPECT_EQ(OK, callback_.WaitForResult());

    // Run QuicChromiumClientSession::WriteToNewSocket()
    // posted by QuicChromiumClientSession::MigrateToSocket().
    base::RunLoop().RunUntilIdle();

    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());

    // Cause QUIC stream to be created.
    HttpRequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL("https://www.example.org/");
    request_info.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_EQ(OK,
              stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                       net_log_, CompletionOnceCallback()));
    // Ensure that session is alive and active.
    QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(host_port_pair_));

    IPEndPoint actual_address;
    session->GetDefaultSocket()->GetPeerAddress(&actual_address);
    EXPECT_EQ(actual_address, expected_address);
    DVLOG(1) << "Socket connected to: " << actual_address.address().ToString()
             << " " << actual_address.port();
    DVLOG(1) << "Expected address: " << expected_address.address().ToString()
             << " " << expected_address.port();

    stream.reset();
    EXPECT_TRUE(socket_data1.AllReadDataConsumed());
    EXPECT_TRUE(socket_data2.AllReadDataConsumed());
    EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  }

  // Verifies that the QUIC stream factory is initialized correctly.
  // If |vary_network_isolation_key| is true, stores data for two different
  // NetworkIsolationKeys, but the same server. If false, stores data for two
  // different servers, using the same NetworkIsolationKey.
  void VerifyInitialization(bool vary_network_isolation_key) {
    const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
    const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));

    NetworkIsolationKey network_isolation_key1(kOrigin1, kOrigin1);
    quic::QuicServerId quic_server_id1(
        kDefaultServerHostName, kDefaultServerPort, PRIVACY_MODE_DISABLED);

    NetworkIsolationKey network_isolation_key2;
    quic::QuicServerId quic_server_id2;

    if (vary_network_isolation_key) {
      network_isolation_key2 = NetworkIsolationKey(kOrigin2, kOrigin2);
      quic_server_id2 = quic_server_id1;
    } else {
      network_isolation_key2 = network_isolation_key1;
      quic_server_id2 = quic::QuicServerId(kServer2HostName, kDefaultServerPort,
                                           PRIVACY_MODE_DISABLED);
    }

    quic_params_->max_server_configs_stored_in_properties = 1;
    quic_params_->idle_connection_timeout = base::TimeDelta::FromSeconds(500);
    Initialize();
    factory_->set_is_quic_known_to_work_on_current_network(true);
    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::ZERO_RTT);
    const quic::QuicConfig* config =
        QuicStreamFactoryPeer::GetConfig(factory_.get());
    EXPECT_EQ(500, config->IdleNetworkTimeout().ToSeconds());

    QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

    const AlternativeService alternative_service1(
        kProtoQUIC, host_port_pair_.host(), host_port_pair_.port());
    AlternativeServiceInfoVector alternative_service_info_vector;
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    alternative_service_info_vector.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service1, expiration, {version_}));
    http_server_properties_->SetAlternativeServices(
        url::SchemeHostPort("https", quic_server_id1.host(),
                            quic_server_id1.port()),
        network_isolation_key1, alternative_service_info_vector);

    const AlternativeService alternative_service2(
        kProtoQUIC, quic_server_id2.host(), quic_server_id2.port());
    AlternativeServiceInfoVector alternative_service_info_vector2;
    alternative_service_info_vector2.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service2, expiration, {version_}));

    http_server_properties_->SetAlternativeServices(
        url::SchemeHostPort("https", quic_server_id2.host(),
                            quic_server_id2.port()),
        network_isolation_key2, alternative_service_info_vector2);
    // Verify that the properties of both QUIC servers are stored in the
    // HTTP properties map.
    EXPECT_EQ(2U,
              http_server_properties_->server_info_map_for_testing().size());

    http_server_properties_->SetMaxServerConfigsStoredInProperties(
        kDefaultMaxQuicServerEntries);

    std::unique_ptr<QuicServerInfo> quic_server_info =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            quic_server_id1, network_isolation_key1,
            http_server_properties_.get());

    // Update quic_server_info's server_config and persist it.
    QuicServerInfo::State* state = quic_server_info->mutable_state();
    // Minimum SCFG that passes config validation checks.
    const char scfg[] = {// SCFG
                         0x53, 0x43, 0x46, 0x47,
                         // num entries
                         0x01, 0x00,
                         // padding
                         0x00, 0x00,
                         // EXPY
                         0x45, 0x58, 0x50, 0x59,
                         // EXPY end offset
                         0x08, 0x00, 0x00, 0x00,
                         // Value
                         '1', '2', '3', '4', '5', '6', '7', '8'};

    // Create temporary strings because Persist() clears string data in |state|.
    string server_config(reinterpret_cast<const char*>(&scfg), sizeof(scfg));
    string source_address_token("test_source_address_token");
    string cert_sct("test_cert_sct");
    string chlo_hash("test_chlo_hash");
    string signature("test_signature");
    string test_cert("test_cert");
    std::vector<string> certs;
    certs.push_back(test_cert);
    state->server_config = server_config;
    state->source_address_token = source_address_token;
    state->cert_sct = cert_sct;
    state->chlo_hash = chlo_hash;
    state->server_config_sig = signature;
    state->certs = certs;

    quic_server_info->Persist();

    std::unique_ptr<QuicServerInfo> quic_server_info2 =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            quic_server_id2, network_isolation_key2,
            http_server_properties_.get());
    // Update quic_server_info2's server_config and persist it.
    QuicServerInfo::State* state2 = quic_server_info2->mutable_state();

    // Minimum SCFG that passes config validation checks.
    const char scfg2[] = {// SCFG
                          0x53, 0x43, 0x46, 0x47,
                          // num entries
                          0x01, 0x00,
                          // padding
                          0x00, 0x00,
                          // EXPY
                          0x45, 0x58, 0x50, 0x59,
                          // EXPY end offset
                          0x08, 0x00, 0x00, 0x00,
                          // Value
                          '8', '7', '3', '4', '5', '6', '2', '1'};

    // Create temporary strings because Persist() clears string data in
    // |state2|.
    string server_config2(reinterpret_cast<const char*>(&scfg2), sizeof(scfg2));
    string source_address_token2("test_source_address_token2");
    string cert_sct2("test_cert_sct2");
    string chlo_hash2("test_chlo_hash2");
    string signature2("test_signature2");
    string test_cert2("test_cert2");
    std::vector<string> certs2;
    certs2.push_back(test_cert2);
    state2->server_config = server_config2;
    state2->source_address_token = source_address_token2;
    state2->cert_sct = cert_sct2;
    state2->chlo_hash = chlo_hash2;
    state2->server_config_sig = signature2;
    state2->certs = certs2;

    quic_server_info2->Persist();

    // Verify the MRU order is maintained.
    const HttpServerProperties::QuicServerInfoMap& quic_server_info_map =
        http_server_properties_->quic_server_info_map();
    EXPECT_EQ(2u, quic_server_info_map.size());
    auto quic_server_info_map_it = quic_server_info_map.begin();
    EXPECT_EQ(quic_server_info_map_it->first.server_id, quic_server_id2);
    ++quic_server_info_map_it;
    EXPECT_EQ(quic_server_info_map_it->first.server_id, quic_server_id1);

    host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                              "192.168.0.1", "");

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
    if (VersionUsesHttp3(version_.transport_version))
      socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  HostPortPair(quic_server_id1.host(), quic_server_id1.port()),
                  version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                  network_isolation_key1, false /* disable_secure_dns */,
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());

    EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
        factory_.get(), quic_server_id1, network_isolation_key1));

    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1 =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_isolation_key1);
    quic::QuicCryptoClientConfig::CachedState* cached =
        crypto_config_handle1->GetConfig()->LookupOrCreate(quic_server_id1);
    EXPECT_FALSE(cached->server_config().empty());
    EXPECT_TRUE(cached->GetServerConfig());
    EXPECT_EQ(server_config, cached->server_config());
    EXPECT_EQ(source_address_token, cached->source_address_token());
    EXPECT_EQ(cert_sct, cached->cert_sct());
    EXPECT_EQ(chlo_hash, cached->chlo_hash());
    EXPECT_EQ(signature, cached->signature());
    ASSERT_EQ(1U, cached->certs().size());
    EXPECT_EQ(test_cert, cached->certs()[0]);

    EXPECT_TRUE(socket_data.AllWriteDataConsumed());

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data2(version_);
    socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    client_maker_.Reset();
    if (VersionUsesHttp3(version_.transport_version))
      socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data2.AddSocketDataToFactory(socket_factory_.get());

    host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                              "192.168.0.2", "");

    QuicStreamRequest request2(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request2.Request(
            HostPortPair(quic_server_id2.host(), quic_server_id2.port()),
            version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
            network_isolation_key2, false /* disable_secure_dns */,
            /*cert_verify_flags=*/0,
            vary_network_isolation_key ? url_
                                       : GURL("https://mail.example.org/"),
            net_log_, &net_error_details_, failed_on_default_network_callback_,
            callback_.callback()));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());

    EXPECT_FALSE(QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
        factory_.get(), quic_server_id2, network_isolation_key2));
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2 =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_isolation_key2);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config_handle2->GetConfig()->LookupOrCreate(quic_server_id2);
    EXPECT_FALSE(cached2->server_config().empty());
    EXPECT_TRUE(cached2->GetServerConfig());
    EXPECT_EQ(server_config2, cached2->server_config());
    EXPECT_EQ(source_address_token2, cached2->source_address_token());
    EXPECT_EQ(cert_sct2, cached2->cert_sct());
    EXPECT_EQ(chlo_hash2, cached2->chlo_hash());
    EXPECT_EQ(signature2, cached2->signature());
    ASSERT_EQ(1U, cached->certs().size());
    EXPECT_EQ(test_cert2, cached2->certs()[0]);
  }

  void RunTestLoopUntilIdle() {
    while (!runner_->GetPostedTasks().empty())
      runner_->RunNextTask();
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) const {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetQpackDecoderStreamId() const {
    return quic::test::GetNthClientInitiatedUnidirectionalStreamId(
        version_.transport_version, 1);
  }

  std::string StreamCancellationQpackDecoderInstruction(int n) const {
    return StreamCancellationQpackDecoderInstruction(n, true);
  }

  std::string StreamCancellationQpackDecoderInstruction(
      int n,
      bool create_stream) const {
    const quic::QuicStreamId cancelled_stream_id =
        GetNthClientInitiatedBidirectionalStreamId(n);
    EXPECT_LT(cancelled_stream_id, 63u);

    const unsigned char opcode = 0x40;
    if (create_stream) {
      return {0x03, opcode | static_cast<unsigned char>(cancelled_stream_id)};
    } else {
      return {opcode | static_cast<unsigned char>(cancelled_stream_id)};
    }
  }

  std::string ConstructDataHeader(size_t body_len) {
    if (!version_.HasIetfQuicFrames()) {
      return "";
    }
    std::unique_ptr<char[]> buffer;
    auto header_length =
        quic::HttpEncoder::SerializeDataFrameHeader(body_len, &buffer);
    return std::string(buffer.get(), header_length);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      quiche::QuicheStringPiece data) {
    return server_maker_.MakeDataPacket(packet_number, stream_id,
                                        should_include_version, fin, data);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        version_.transport_version, n);
  }

  void OnFailedOnDefaultNetwork(int rv) { failed_on_default_network_ = true; }

  // Helper methods for tests of connection migration on write error.
  void TestMigrationOnWriteErrorNonMigratableStream(IoMode write_error_mode,
                                                    bool migrate_idle_sessions);
  // Migratable stream triggers write error.
  void TestMigrationOnWriteErrorMixedStreams(IoMode write_error_mode);
  // Non-migratable stream triggers write error.
  void TestMigrationOnWriteErrorMixedStreams2(IoMode write_error_mode);
  void TestMigrationOnWriteErrorMigrationDisabled(IoMode write_error_mode);
  void TestMigrationOnWriteError(IoMode write_error_mode);
  void TestMigrationOnWriteErrorWithMultipleRequests(IoMode write_error_mode);
  void TestMigrationOnWriteErrorNoNewNetwork(IoMode write_error_mode);
  void TestMigrationOnMultipleWriteErrors(
      IoMode write_error_mode_on_old_network,
      IoMode write_error_mode_on_new_network);
  void TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
      bool disconnected);
  void TestMigrationOnWriteErrorWithNotificationQueuedLater(bool disconnected);
  void TestMigrationOnNetworkDisconnected(bool async_write_before);
  void TestMigrationOnNetworkMadeDefault(IoMode write_mode);
  void TestMigrationOnPathDegrading(bool async_write_before);
  void TestMigrateSessionWithDrainingStream(
      IoMode write_mode_for_queued_packet);
  void TestMigrationOnWriteErrorPauseBeforeConnected(IoMode write_error_mode);
  void TestMigrationOnWriteErrorWithMultipleNotifications(
      IoMode write_error_mode,
      bool disconnect_before_connect);
  void TestNoAlternateNetworkBeforeHandshake(quic::QuicErrorCode error);
  void TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QuicErrorCode error);
  void TestOnNetworkMadeDefaultNonMigratableStream(bool migrate_idle_sessions);
  void TestMigrateSessionEarlyNonMigratableStream(bool migrate_idle_sessions);
  void TestOnNetworkDisconnectedNoOpenStreams(bool migrate_idle_sessions);
  void TestOnNetworkMadeDefaultNoOpenStreams(bool migrate_idle_sessions);
  void TestOnNetworkDisconnectedNonMigratableStream(bool migrate_idle_sessions);

  // Port migrations.
  void TestSimplePortMigrationOnPathDegrading();

  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  std::unique_ptr<MockHostResolverBase> host_resolver_;
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  std::unique_ptr<MockClientSocketFactory> socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockQuicContext context_;
  scoped_refptr<TestTaskRunner> runner_;
  const quic::ParsedQuicVersion version_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<CTVerifier> cert_transparency_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  std::unique_ptr<ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier_;
  std::unique_ptr<QuicStreamFactory> factory_;
  HostPortPair host_port_pair_;
  GURL url_;
  GURL url2_;
  GURL url3_;
  GURL url4_;

  PrivacyMode privacy_mode_;
  NetLogWithSource net_log_;
  TestCompletionCallback callback_;
  const CompletionRepeatingCallback failed_on_default_network_callback_;
  bool failed_on_default_network_;
  NetErrorDetails net_error_details_;

  QuicParams* quic_params_;
};

class QuicStreamFactoryTest : public QuicStreamFactoryTestBase,
                              public ::testing::TestWithParam<TestParams> {
 protected:
  QuicStreamFactoryTest()
      : QuicStreamFactoryTestBase(
            GetParam().version,
            GetParam().client_headers_include_h2_stream_dependency) {}
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicStreamFactoryTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicStreamFactoryTest, Create) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  // Will reset stream 3.
  stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(
      OK,
      request3.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  stream = CreateStream(&request3);  // Will reset stream 5.
  stream.reset();                    // Will reset stream 7.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CreateZeroRtt) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, DefaultInitialRtt) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(session->require_confirmation());
  EXPECT_EQ(100000u, session->connection()->GetStats().srtt_us);
  ASSERT_FALSE(session->config()->HasInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, FactoryDestroyedWhenJobPending) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  auto request = std::make_unique<QuicStreamRequest>(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request->Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  request.reset();
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  // Tearing down a QuicStreamFactory with a pending Job should not cause any
  // crash. crbug.com/768343.
  factory_.reset();
}

TEST_P(QuicStreamFactoryTest, RequireConfirmation) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(session->require_confirmation());
}

TEST_P(QuicStreamFactoryTest, DontRequireConfirmationFromSameIP) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(false);
  http_server_properties_->SetLastLocalAddressWhenQuicWorked(
      IPAddress(192, 0, 2, 33));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_THAT(
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()),
      IsOk());

  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_FALSE(session->require_confirmation());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());
}

TEST_P(QuicStreamFactoryTest, CachedInitialRtt) {
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMilliseconds(10);
  http_server_properties_->SetServerNetworkStats(url::SchemeHostPort(url_),
                                                 NetworkIsolationKey(), stats);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(10000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(10000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

// Test that QUIC sessions use the cached RTT from HttpServerProperties for the
// correct NetworkIsolationKey.
TEST_P(QuicStreamFactoryTest, CachedInitialRttWithNetworkIsolationKey) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkIsolationKey for
       // QuicSessionAliasKey to include NetworkIsolationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMilliseconds(10);
  http_server_properties_->SetServerNetworkStats(url::SchemeHostPort(url_),
                                                 kNetworkIsolationKey1, stats);
  quic_params_->estimate_initial_rtt = true;
  Initialize();

  for (const auto& network_isolation_key :
       {kNetworkIsolationKey1, kNetworkIsolationKey2, NetworkIsolationKey()}) {
    SCOPED_TRACE(network_isolation_key.ToString());

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    QuicTestPacketMaker packet_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        quic_params_->headers_include_h2_stream_dependency);

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (VersionUsesHttp3(version_.transport_version)) {
      socket_data.AddWrite(SYNCHRONOUS,
                           packet_maker.MakeInitialSettingsPacket(1));
    }
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), network_isolation_key, false /* disable_secure_dns */,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsOk());
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());

    QuicChromiumClientSession* session =
        GetActiveSession(host_port_pair_, network_isolation_key);
    if (network_isolation_key == kNetworkIsolationKey1) {
      EXPECT_EQ(10000, session->connection()->GetStats().srtt_us);
      ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
      EXPECT_EQ(10000u, session->config()->GetInitialRoundTripTimeUsToSend());
    } else {
      EXPECT_EQ(quic::kInitialRttMs * 1000,
                session->connection()->GetStats().srtt_us);
      EXPECT_FALSE(session->config()->HasInitialRoundTripTimeUsToSend());
    }
  }
}

TEST_P(QuicStreamFactoryTest, 2gInitialRtt) {
  ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_2G);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(1200000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(1200000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, 3gInitialRtt) {
  ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_3G);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(400000u, session->connection()->GetStats().srtt_us);
  ASSERT_TRUE(session->config()->HasInitialRoundTripTimeUsToSend());
  EXPECT_EQ(400000u, session->config()->GetInitialRoundTripTimeUsToSend());
}

TEST_P(QuicStreamFactoryTest, GoAway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  session->OnGoAway(quic::QuicGoAwayFrame());

  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, GoAwayForConnectionMigrationWithPortOnly) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  session->OnGoAway(quic::QuicGoAwayFrame(
      quic::kInvalidControlFrameId, quic::QUIC_ERROR_MIGRATING_PORT, 0,
      "peer connection migration due to port change only"));
  NetErrorDetails details;
  EXPECT_FALSE(details.quic_port_migration_detected);
  session->PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_port_migration_detected);
  details.quic_port_migration_detected = false;
  stream->PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_port_migration_detected);

  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Makes sure that setting and clearing ServerNetworkStats respects the
// NetworkIsolationKey.
TEST_P(QuicStreamFactoryTest, ServerNetworkStatsWithNetworkIsolationKey) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  const NetworkIsolationKey kNetworkIsolationKeys[] = {
      kNetworkIsolationKey1, kNetworkIsolationKey2, NetworkIsolationKey()};

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkIsolationKey for
       // QuicSessionAliasKey to include NetworkIsolationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();
  Initialize();

  // For each server, set up and tear down a QUIC session cleanly, and check
  // that stats have been added to HttpServerProperties using the correct
  // NetworkIsolationKey.
  for (size_t i = 0; i < base::size(kNetworkIsolationKeys); ++i) {
    SCOPED_TRACE(i);

    ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    QuicTestPacketMaker packet_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        quic_params_->headers_include_h2_stream_dependency);

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (VersionUsesHttp3(version_.transport_version)) {
      socket_data.AddWrite(SYNCHRONOUS,
                           packet_maker.MakeInitialSettingsPacket(1));
    }
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                  SocketTag(), kNetworkIsolationKeys[i],
                  false /* disable_secure_dns */,
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsOk());
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream.get());

    QuicChromiumClientSession* session =
        GetActiveSession(host_port_pair_, kNetworkIsolationKeys[i]);

    session->OnGoAway(quic::QuicGoAwayFrame());

    EXPECT_FALSE(HasActiveSession(host_port_pair_, kNetworkIsolationKeys[i]));

    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());

    for (size_t j = 0; j < base::size(kNetworkIsolationKeys); ++j) {
      // Stats up to kNetworkIsolationKeys[j] should have been populated, all
      // others should remain empty.
      if (j <= i) {
        EXPECT_TRUE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkIsolationKeys[j]));
      } else {
        EXPECT_FALSE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkIsolationKeys[j]));
      }
    }
  }

  // Use unmocked crypto stream to do crypto connect, since crypto errors result
  // in deleting network stats..
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // For each server, simulate an error during session creation, and check that
  // stats have been deleted from HttpServerProperties using the correct
  // NetworkIsolationKey.
  for (size_t i = 0; i < base::size(kNetworkIsolationKeys); ++i) {
    SCOPED_TRACE(i);

    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
    socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
    socket_data.AddSocketDataToFactory(socket_factory_.get());

    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(ERR_IO_PENDING,
              request.Request(
                  host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
                  SocketTag(), kNetworkIsolationKeys[i],
                  false /* disable_secure_dns */,
                  /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                  failed_on_default_network_callback_, callback_.callback()));

    EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_QUIC_HANDSHAKE_FAILED));

    EXPECT_FALSE(HasActiveSession(host_port_pair_, kNetworkIsolationKeys[i]));

    for (size_t j = 0; j < base::size(kNetworkIsolationKeys); ++j) {
      // Stats up to kNetworkIsolationKeys[j] should have been deleted, all
      // others should still be populated.
      if (j <= i) {
        EXPECT_FALSE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkIsolationKeys[j]));
      } else {
        EXPECT_TRUE(http_server_properties_->GetServerNetworkStats(
            url::SchemeHostPort(url_), kNetworkIsolationKeys[j]));
      }
    }
  }
}

TEST_P(QuicStreamFactoryTest, Pooling) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server2(kServer2HostName, kDefaultServerPort);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(host_port_pair_), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, PoolingWithServerMigration) {
  // Set up session to migrate.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 443);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));

  VerifyServerMigration(config, alt_address);

  // Close server-migrated session.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  session->CloseSessionOnError(0u, quic::QUIC_NO_ERROR,
                               quic::ConnectionCloseBehavior::SILENT_CLOSE);

  client_maker_.Reset();
  // Set up server IP, socket, proof, and config for new session.
  HostPortPair server2(kServer2HostName, kDefaultServerPort);
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  quic::QuicConfig config2;
  crypto_client_stream_factory_.SetConfig(config2);

  // Create new request to cause new session creation.
  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(ERR_IO_PENDING,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  EXPECT_EQ(OK, callback.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  // EXPECT_EQ(GetActiveSession(host_port_pair_), GetActiveSession(server2));
}

TEST_P(QuicStreamFactoryTest, NoPoolingAfterGoAway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server2(kServer2HostName, kDefaultServerPort);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  factory_->OnSessionGoingAway(GetActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveSession(server2));

  TestCompletionCallback callback3;
  QuicStreamRequest request3(factory_.get());
  EXPECT_EQ(OK,
            request3.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback3.callback()));
  std::unique_ptr<HttpStream> stream3 = CreateStream(&request3);
  EXPECT_TRUE(stream3.get());

  EXPECT_TRUE(HasActiveSession(server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HttpsPooling) {
  Initialize();

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HttpsPoolingWithMatchingPins) {
  Initialize();
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);
  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  verify_details.cert_verify_result.public_key_hashes.push_back(primary_pin);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_EQ(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NoHttpsPoolingWithDifferentPins) {
  Initialize();

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);
  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  ProofVerifyDetailsChromium verify_details1 = DefaultProofVerifyDetails();
  uint8_t bad_pin = 3;
  verify_details1.cert_verify_result.public_key_hashes.push_back(
      test::GetTestHashValue(bad_pin));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  ProofVerifyDetailsChromium verify_details2 = DefaultProofVerifyDetails();
  verify_details2.cert_verify_result.public_key_hashes.push_back(primary_pin);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(OK,
            request.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  TestCompletionCallback callback;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_NE(GetActiveSession(server1), GetActiveSession(server2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, Goaway) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Mark the session as going away.  Ensure that while it is still alive
  // that it is no longer active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  factory_->OnSessionGoingAway(session);
  EXPECT_EQ(true,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Create a new request for the same destination and verify that a
  // new session is created.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_NE(session, GetActiveSession(host_port_pair_));
  EXPECT_EQ(true,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MaxOpenStream) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  quic::QuicStreamId stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  MockQuicData socket_data(version_);
  if (version_.HasIetfQuicFrames()) {
    int packet_num = 1;
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
    socket_data.AddWrite(SYNCHRONOUS, client_maker_.MakeStreamsBlockedPacket(
                                          packet_num++, true, 50,
                                          /*unidirectional=*/false));
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(packet_num++, true, stream_id,
                                                 quic::QUIC_STREAM_CANCELLED));
    socket_data.AddRead(
        ASYNC, server_maker_.MakeRstPacket(1, false, stream_id,
                                           quic::QUIC_STREAM_CANCELLED));
    socket_data.AddRead(
        ASYNC, server_maker_.MakeMaxStreamsPacket(4, true, 52,
                                                  /*unidirectional=*/false));
  } else {
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstPacket(1, true, stream_id,
                                                 quic::QUIC_STREAM_CANCELLED));
    socket_data.AddRead(
        ASYNC, server_maker_.MakeRstPacket(1, false, stream_id,
                                           quic::QUIC_STREAM_CANCELLED));
  }
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  std::vector<std::unique_ptr<HttpStream>> streams;
  // The MockCryptoClientStream sets max_open_streams to be
  // quic::kDefaultMaxStreamsPerConnection / 2.
  for (size_t i = 0; i < quic::kDefaultMaxStreamsPerConnection / 2; i++) {
    QuicStreamRequest request(factory_.get());
    int rv = request.Request(
        host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
        NetworkIsolationKey(), false /* disable_secure_dns */,
        /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
        failed_on_default_network_callback_, callback_.callback());
    if (i == 0) {
      EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
      EXPECT_THAT(callback_.WaitForResult(), IsOk());
    } else {
      EXPECT_THAT(rv, IsOk());
    }
    std::unique_ptr<HttpStream> stream = CreateStream(&request);
    EXPECT_TRUE(stream);
    EXPECT_EQ(OK,
              stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                       net_log_, CompletionOnceCallback()));
    streams.push_back(std::move(stream));
  }

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, CompletionOnceCallback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  EXPECT_EQ(ERR_IO_PENDING,
            stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                     net_log_, callback_.callback()));

  // Close the first stream.
  streams.front()->Close(false);
  // Trigger exchange of RSTs that in turn allow progress for the last
  // stream.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  // Force close of the connection to suppress the generation of RST
  // packets when streams are torn down, which wouldn't be relevant to
  // this test anyway.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  session->connection()->CloseConnection(
      quic::QUIC_PUBLIC_RESET, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
}

TEST_P(QuicStreamFactoryTest, ResolutionErrorInCreate) {
  Initialize();
  MockQuicData socket_data(version_);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  host_resolver_->rules()->AddSimulatedFailure(kDefaultServerHostName);

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ConnectErrorInCreate) {
  Initialize();

  MockQuicData socket_data(version_);
  socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CancelCreate) {
  Initialize();
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  {
    QuicStreamRequest request(factory_.get());
    EXPECT_EQ(
        ERR_IO_PENDING,
        request.Request(
            host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
            SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
            /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
            failed_on_default_network_callback_, callback_.callback()));
  }

  base::RunLoop().RunUntilIdle();

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);

  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CloseAllSessions) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++, quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeConnectionClosePacket(
          packet_num++, true, quic::QUIC_PEER_GOING_AWAY, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Close the session and verify that stream saw the error.
  factory_->CloseAllSessions(ERR_INTERNET_DISCONNECTED,
                             quic::QUIC_PEER_GOING_AWAY);
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  stream = CreateStream(&request2);
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Regression test for crbug.com/700617. Test a write error during the
// crypto handshake will not hang QuicStreamFactory::Job and should
// report QUIC_HANDSHAKE_FAILED to upper layers. Subsequent
// QuicStreamRequest should succeed without hanging.
TEST_P(QuicStreamFactoryTest,
       WriteErrorInCryptoConnectWithAsyncHostResolution) {
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, WriteErrorInCryptoConnectWithSyncHostResolution) {
  Initialize();
  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail immediately.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_QUIC_HANDSHAKE_FAILED,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  // Check no active session, or active jobs left for this server.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Verify new requests can be sent normally without hanging.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Complete handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CloseSessionsOnIPAddressChanged) {
  quic_params_->close_sessions_on_ip_change = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++, quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeConnectionClosePacket(
          packet_num, true, quic::QUIC_IP_ADDRESS_CHANGED, "net error"));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Check an active session exists for the destination.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());
  // Change the IP address and verify that stream saw the error and the active
  // session is closed.
  NotifyIPAddressChanged();
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_FALSE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());
  // Check no active session exists for the destination.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Now attempting to request a stream to the same origin should create
  // a new session.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  stream = CreateStream(&request2);

  // Check a new active session exisits for the destination and the old session
  // is no longer live.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  stream.reset();  // Will reset stream 3.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Test that if goaway_session_on_ip_change is set, old sessions will be marked
// as going away on IP address change instead of being closed. New requests will
// go to a new connection.
TEST_P(QuicStreamFactoryTest, GoAwaySessionsOnIPAddressChanged) {
  quic_params_->goaway_sessions_on_ip_change = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Receive an IP address change notification.
  NotifyIPAddressChanged();

  // The connection should still be alive, but marked as going away.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume the data, response should be read from the original connection.
  quic_data1.Resume();
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  // Second request should be sent on a new connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // Check an active session exisits for the destination.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  stream.reset();
  stream2.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnIPAddressChangedWithConnectionMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num, quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  // Change the IP address and verify that the connection is unaffected.
  NotifyIPAddressChanged();
  EXPECT_TRUE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_TRUE(http_server_properties_->HasLastLocalAddressWhenQuicWorked());

  // Attempting a new request to the same origin uses the same connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  stream = CreateStream(&request2);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateOnNetworkMadeDefaultWithSynchronousWrite) {
  TestMigrationOnNetworkMadeDefault(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateOnNetworkMadeDefaultWithAsyncWrite) {
  TestMigrationOnNetworkMadeDefault(ASYNC);
}

// Sets up a test which attempts connection migration successfully after probing
// when a new network is made as default and the old default is still available.
// |write_mode| specifies the write mode for the last write before
// OnNetworkMadeDefault is delivered to session.
void QuicStreamFactoryTestBase::TestMigrationOnNetworkMadeDefault(
    IoMode write_mode) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data1.AddWrite(
      write_mode,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_num++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeAckAndPingPacket(packet_num++, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 2, 2, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 2, 2, 1));
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // A task will be posted to migrate to the new default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());

  // Execute the posted task to migrate back to the default network.
  task_runner->RunUntilIdle();
  // Another task to try send a new connectivity probe is posted. And a task to
  // retry migrate back to default network is scheduled.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket, declare probing as successful. And a new task to WriteToNewSocket
  // will be posted to complete migration.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s, which
  // is also cancelled due to the success migration on the previous trial.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);
  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Regression test for http://859674.
// This test veries that a writer will not attempt to write packets until being
// unblocked on both socket level and network level. In this test, a probing
// writer is used to send two connectivity probes to the peer: where the first
// one completes successfully, while a connectivity response is received before
// completes sending the second one. The connection migration attempt will
// proceed while the probing writer is blocked at the socket level, which will
// block the writer on the network level. Once connection migration completes
// successfully, the probing writer will be unblocked on the network level, it
// will not attempt to write new packets until the socket level is unblocked.
TEST_P(QuicStreamFactoryTest, MigratedToBlockedSocketAfterProbing) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  // First connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_num++, true));
  quic_data2.AddRead(ASYNC,
                     ERR_IO_PENDING);  // Pause so that we can control time.
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Second connectivity probe which will complete asynchronously.
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeConnectivityProbingPacket(packet_num++, true));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeAckAndPingPacket(packet_num++, false, 1, 1, 1));
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 2, 2, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 2, 2, 1));
  }

  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  // A task will be posted to migrate to the new default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());

  // Execute the posted task to migrate back to the default network.
  task_runner->RunUntilIdle();
  // Another task to resend a new connectivity probe is posted. And a task to
  // retry migrate back to default network is scheduled.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Fast forward to send the second connectivity probe. The write will be
  // asynchronous and complete after the read completes.
  task_runner->FastForwardBy(next_task_delay);

  // Resume quic data and a connectivity probe response will be read on the new
  // socket, declare probing as successful.
  quic_data2.Resume();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network. Second task will retry migrate back to
  // default but cancelled, and the third task will retry send connectivity
  // probe but also cancelled.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Run the message loop to complete the asynchronous write of ack and ping.
  base::RunLoop().RunUntilIdle();

  // Now there are two pending tasks, the nearest one was to retry migrate back
  // to default network and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      expected_delay;
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(expected_delay, next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to retry sending connectivity probe in 0.4s and has
  // also been cancelled due to the successful probing.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  expected_delay =
      base::TimeDelta::FromMilliseconds(3 * 2 * kDefaultRTTMilliSecs) -
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs);
  EXPECT_EQ(expected_delay, next_task_delay);
  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that session times out connection migration attempt
// with signals delivered in the following order (no alternate network is
// available):
// - default network disconnected is delivered: session attempts connection
//   migration but found not alternate network. Session waits for a new network
//   comes up in the next kWaitTimeForNewNetworkSecs seconds.
// - no new network is connected, migration times out. Session is closed.
TEST_P(QuicStreamFactoryTest, MigrationTimeoutWithNoNewNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there are no networks
  // to migrate to, this should cause the session to wait for a new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The migration will not fail until the migration alarm timeout.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(true, session->connection()->writer()->IsWriteBlocked());

  // Migration will be timed out after kWaitTimeForNewNetwokSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromSeconds(kWaitTimeForNewNetworkSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // The connection should now be closed. A request for response
  // headers should fail.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test verifies that connectivity probes will be sent even if there is
// a non-migratable stream. However, when connection migrates to the
// successfully probed path, any non-migratable streams will be reset.
TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNonMigratableStream_MigrateIdleSessions) {
  TestOnNetworkMadeDefaultNonMigratableStream(true);
}

// This test verifies that connectivity probes will be sent even if there is
// a non-migratable stream. However, when connection migrates to the
// successfully probed path, any non-migratable stream will be reset. And if
// the connection becomes idle then, close the connection.
TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNonMigratableStream_DoNotMigrateIdleSessions) {
  TestOnNetworkMadeDefaultNonMigratableStream(false);
}

void QuicStreamFactoryTestBase::TestOnNetworkMadeDefaultNonMigratableStream(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }

  // Set up the second socket data provider that is used for probing.
  MockQuicData quic_data1(version_);
  // Connectivity probe to be sent on the new path.
  quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_num++, true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data1.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));

  if (migrate_idle_sessions) {
    quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    if (VersionUsesHttp3(version_.transport_version)) {
      quic_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeDataAndRstPacket(
                              packet_num++, true, GetQpackDecoderStreamId(),
                              StreamCancellationQpackDecoderInstruction(0),
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
    } else {
      quic_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_num++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
    }
    // Ping packet to send after migration is completed.
    quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeAckAndPingPacket(
                                         packet_num++, false, 1, 1, 1));
  } else {
    if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
      if (VersionUsesHttp3(version_.transport_version)) {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeDataRstAckAndConnectionClosePacket(
                packet_num++, false, GetQpackDecoderStreamId(),
                StreamCancellationQpackDecoderInstruction(0),
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      } else {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeRstAckAndConnectionClosePacket(
                packet_num++, false,
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      }
    } else {
      if (VersionUsesHttp3(version_.transport_version)) {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeDataRstAckAndConnectionClosePacket(
                packet_num++, false, GetQpackDecoderStreamId(),
                StreamCancellationQpackDecoderInstruction(0),
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      } else {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeRstAckAndConnectionClosePacket(
                packet_num++, false,
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      }
    }
  }

  socket_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Session will start to probe the alternative
  // network. Although there is a non-migratable stream, session will still be
  // active until probing is declared as successful.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume data to read a connectivity probing response, which will cause
  // non-migtable streams to be closed.
  quic_data1.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnNetworkMadeDefaultConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, true,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to continue but be marked as going away.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNonMigratableStream_DoNotMigrateIdleSessions) {
  TestOnNetworkDisconnectedNonMigratableStream(false);
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNonMigratableStream_MigrateIdleSessions) {
  TestOnNetworkDisconnectedNonMigratableStream(true);
}

void QuicStreamFactoryTestBase::TestOnNetworkDisconnectedNonMigratableStream(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData failed_socket_data(version_);
  MockQuicData socket_data(version_);
  if (migrate_idle_sessions) {
    failed_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    int packet_num = 1;
    if (VersionUsesHttp3(version_.transport_version)) {
      failed_socket_data.AddWrite(SYNCHRONOUS,
                                  ConstructInitialSettingsPacket(packet_num++));
    }
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    if (VersionUsesHttp3(version_.transport_version)) {
      failed_socket_data.AddWrite(
          SYNCHRONOUS, client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), true, false,
                           StreamCancellationQpackDecoderInstruction(0)));
    }
    failed_socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, true, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
    failed_socket_data.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after migration.
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // Ping packet to send after migration.
    socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  } else {
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    int packet_num = 1;
    if (VersionUsesHttp3(version_.transport_version)) {
      socket_data.AddWrite(SYNCHRONOUS,
                           ConstructInitialSettingsPacket(packet_num++));
      socket_data.AddWrite(
          SYNCHRONOUS, client_maker_.MakeDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), true, false,
                           StreamCancellationQpackDecoderInstruction(0)));
    }
    socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, true, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause a RST_STREAM frame to be emitted with
  // quic::QUIC_STREAM_CANCELLED error code.
  // If migate idle session, the connection will then be migrated to the
  // alternate network. Otherwise, the connection will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_EQ(migrate_idle_sessions,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(host_port_pair_));

  if (migrate_idle_sessions) {
    EXPECT_EQ(0u, session->GetNumActiveStreams());
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(failed_socket_data.AllReadDataConsumed());
    EXPECT_TRUE(failed_socket_data.AllWriteDataConsumed());
  }
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, true,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_RST_ACKNOWLEDGEMENT));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause a RST_STREAM frame to be emitted with
  // quic::QUIC_RST_ACKNOWLEDGEMENT error code, and the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNoOpenStreams_DoNotMigrateIdleSessions) {
  TestOnNetworkMadeDefaultNoOpenStreams(false);
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkMadeDefaultNoOpenStreams_MigrateIdleSessions) {
  TestOnNetworkMadeDefaultNoOpenStreams(true);
}

void QuicStreamFactoryTestBase::TestOnNetworkMadeDefaultNoOpenStreams(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  if (!migrate_idle_sessions) {
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeConnectionClosePacket(
                         packet_num, true,
                         quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                         "net error"));
  }
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data1(version_);
  if (migrate_idle_sessions) {
    // Set up the second socket data provider that is used for probing.
    // Connectivity probe to be sent on the new path.
    quic_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeConnectivityProbingPacket(packet_num++, true));
    quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
    // Connectivity probe to receive from the server.
    quic_data1.AddRead(ASYNC,
                       server_maker_.MakeConnectivityProbingPacket(1, false));
    quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // Ping packet to send after migration is completed.
    quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeAckAndPingPacket(
                                         packet_num, false, 1, 1, 1));
    quic_data1.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());
  EXPECT_EQ(0u, session->GetNumDrainingStreams());

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(host_port_pair_));

  if (migrate_idle_sessions) {
    quic_data1.Resume();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(quic_data1.AllReadDataConsumed());
    EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  }
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNoOpenStreams_DoNotMigateIdleSessions) {
  TestOnNetworkDisconnectedNoOpenStreams(false);
}

TEST_P(QuicStreamFactoryTest,
       OnNetworkDisconnectedNoOpenStreams_MigateIdleSessions) {
  TestOnNetworkDisconnectedNoOpenStreams(true);
}

void QuicStreamFactoryTestBase::TestOnNetworkDisconnectedNoOpenStreams(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData default_socket_data(version_);
  default_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    default_socket_data.AddWrite(SYNCHRONOUS,
                                 ConstructInitialSettingsPacket(packet_num++));
  }
  default_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData alternate_socket_data(version_);
  if (migrate_idle_sessions) {
    // Set up second socket data provider that is used after migration.
    alternate_socket_data.AddRead(SYNCHRONOUS,
                                  ERR_IO_PENDING);  // Hanging read.
    // Ping packet to send after migration.
    alternate_socket_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakePingPacket(packet_num, /*include_version=*/true));
    alternate_socket_data.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is active.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(host_port_pair_));

  EXPECT_TRUE(default_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(default_socket_data.AllWriteDataConsumed());
  if (migrate_idle_sessions) {
    EXPECT_TRUE(alternate_socket_data.AllReadDataConsumed());
    EXPECT_TRUE(alternate_socket_data.AllWriteDataConsumed());
  }
}

// This test verifies session migrates to the alternate network immediately when
// default network disconnects with a synchronous write before migration.
TEST_P(QuicStreamFactoryTest, MigrateOnDefaultNetworkDisconnectedSync) {
  TestMigrationOnNetworkDisconnected(/*async_write_before*/ false);
}

// This test verifies session migrates to the alternate network immediately when
// default network disconnects with an asynchronously write before migration.
TEST_P(QuicStreamFactoryTest, MigrateOnDefaultNetworkDisconnectedAsync) {
  TestMigrationOnNetworkDisconnected(/*async_write_before*/ true);
}

void QuicStreamFactoryTestBase::TestMigrationOnNetworkDisconnected(
    bool async_write_before) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  int packet_number = 1;
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_number++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  if (async_write_before) {
    socket_data.AddWrite(ASYNC, OK);
    packet_number++;
  }
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  if (async_write_before)
    session->SendPing();

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_number++, /*include_version=*/true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_number++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeAckAndRstPacket(
                              packet_number++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Trigger connection migration.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  runner_->RunNextTask();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // There should be posted tasks not executed, which is to migrate back to
  // default network.
  EXPECT_FALSE(runner_->GetPostedTasks().empty());

  // Receive signal to mark new network as default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test receives NCN signals in the following order:
// - default network disconnected
// - after a pause, new network is connected.
// - new network is made default.
TEST_P(QuicStreamFactoryTest, NewNetworkConnectedAfterNoNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger connection migration. Since there are no networks
  // to migrate to, this should cause the session to wait for a new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Add a new network and notify the stream factory of a new connected network.
  // This causes a PING packet to be sent over the new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  runner_->RunNextTask();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // There should posted tasks not executed, which is to migrate back to default
  // network.
  EXPECT_FALSE(runner_->GetPostedTasks().empty());

  // Receive signal to mark new network as default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/872011.
// This test verifies that migrate to the probing socket will not trigger
// new packets being read synchronously and generate ACK frame while
// processing the initial connectivity probe response, which may cause a
// connection being closed with INTERNAL_ERROR as pending ACK frame is not
// allowed when processing a new packet.
TEST_P(QuicStreamFactoryTest, MigrateToProbingSocket) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used for probing on the
  // alternate network.
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // First connectivity probe to receive from the server, which will complete
  // connection migraiton on path degrading.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Read multiple connectivity probes synchronously.
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(2, false));
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(3, false));
  quic_data2.AddRead(SYNCHRONOUS,
                     server_maker_.MakeConnectivityProbingPacket(4, false));
  quic_data2.AddWrite(ASYNC,
                      client_maker_.MakeAckPacket(packet_number++, 1, 4, 1, 1));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          5, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 5, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeRstPacket(
                            packet_number++, false,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            packet_number++, false,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 5, 1, 1));
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that the connection migrates to the alternate network
// early when path degrading is detected with an ASYNCHRONOUS write before
// migration.
TEST_P(QuicStreamFactoryTest, MigrateEarlyOnPathDegradingAsync) {
  TestMigrationOnPathDegrading(/*async_write_before_migration*/ true);
}

// This test verifies that the connection migrates to the alternate network
// early when path degrading is detected with a SYNCHRONOUS write before
// migration.
TEST_P(QuicStreamFactoryTest, MigrateEarlyOnPathDegradingSync) {
  TestMigrationOnPathDegrading(/*async_write_before_migration*/ false);
}

void QuicStreamFactoryTestBase::TestMigrationOnPathDegrading(
    bool async_write_before) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  if (async_write_before) {
    quic_data1.AddWrite(ASYNC, OK);
    packet_number++;
  }
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(ASYNC, client_maker_.MakeAckAndPingPacket(
                                 packet_number++, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 2, 2, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeRstPacket(
                            packet_number++, false,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            packet_number++, false,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 2, 2, 1));
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  if (async_write_before)
    session->SendPing();

  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Verifies that port migration can be attempted and succeed when path degrading
// is detected, even if NetworkHandle is not supported.
TEST_P(QuicStreamFactoryTest, MigratePortOnPathDegrading_WithoutNetworkHandle) {
  quic_params_->allow_port_migration = true;
  socket_factory_.reset(new TestMigrationSocketFactory);
  Initialize();

  TestSimplePortMigrationOnPathDegrading();
}

// Verifies that port migration can be attempted on the default network and
// succeed when path degrading is detected. NetworkHandle is supported.
TEST_P(QuicStreamFactoryTest, MigratePortOnPathDegrading_WithNetworkHandle) {
  scoped_mock_network_change_notifier_.reset(
      new ScopedMockNetworkChangeNotifier());
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  quic_params_->allow_port_migration = true;
  socket_factory_.reset(new TestMigrationSocketFactory);
  Initialize();

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  TestSimplePortMigrationOnPathDegrading();
}

// Verifies that port migration can be attempted on the default network and
// succeed when path degrading is detected.
// NetworkHandle is supported. Migration on network change is also enabled.
// No additional network migration is triggered post port migration.
TEST_P(QuicStreamFactoryTest, MigratePortOnPathDegrading_WithMigration) {
  scoped_mock_network_change_notifier_.reset(
      new ScopedMockNetworkChangeNotifier());
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList({kDefaultNetworkForTests});
  // Enable migration on network change.
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  quic_params_->allow_port_migration = true;
  socket_factory_.reset(new TestMigrationSocketFactory);
  Initialize();

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  TestSimplePortMigrationOnPathDegrading();
}

void QuicStreamFactoryTestBase::TestSimplePortMigrationOnPathDegrading() {
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(ASYNC, client_maker_.MakeAckAndPingPacket(
                                 packet_number++, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 2, 2, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeRstPacket(
                            packet_number++, false,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeAckAndRstPacket(
                            packet_number++, false,
                            GetNthClientInitiatedBidirectionalStreamId(0),
                            quic::QUIC_STREAM_CANCELLED, 2, 2, 1));
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe a different port.
  session->connection()->OnPathDegradingTimeout();

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be pending tasks, the nearest one will complete
  // migration to the new port.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new port.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there is one pending task to send connectivity probe and has been
  // cancelled due to successful migration.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1014092.
TEST_P(QuicStreamFactoryTest, MultiplePortMigrationsExceedsMaxLimit) {
  quic_params_->allow_port_migration = true;
  socket_factory_.reset(new TestMigrationSocketFactory);
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  int server_packet_num = 1;
  base::TimeDelta next_task_delay;
  // Perform 4 round of successful migration, and the 5th round will
  // cancel after successful probing due to hitting the limit.
  for (int i = 0; i <= 4; i++) {
    // Set up a different socket data provider that is used for
    // probing and migration.
    MockQuicData quic_data2(version_);
    // Connectivity probe to be sent on the new path.
    quic_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeConnectivityProbingPacket(
                            packet_number, packet_number == 2));
    packet_number++;
    quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
    // Connectivity probe to receive from the server.
    quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(
                                  server_packet_num++, false));
    // Ping packet to send after migration is completed.
    if (i == 0) {
      // First ack and PING are bundled, and version flag is set.
      quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeAckAndPingPacket(
                                           packet_number++, false, 1, 1, 1));
    } else if (i != 4) {
      // ACK and PING post migration after successful probing.
      quic_data2.AddWrite(
          SYNCHRONOUS, client_maker_.MakeAckPacket(packet_number++, 1 + 2 * i,
                                                   1 + 2 * i, 1));
      quic_data2.AddWrite(SYNCHRONOUS,
                          client_maker_.MakePingPacket(packet_number++, false));
    }
    if (i == 4) {
      // Add one more synchronous read on the last probing reader. The
      // reader should be deleted on the read before this one.
      // The test will verify this read is not consumed.
      quic_data2.AddRead(SYNCHRONOUS,
                         server_maker_.MakeConnectivityProbingPacket(
                             server_packet_num++, false));
    } else {
      quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(
                                    server_packet_num++, false));
    }
    if (i == 3) {
      // On the last allowed port migration, read one more packet so
      // that ACK is sent. The next round of migration (which hists the limit)
      // will not send any proactive ACK when reading the successful probing
      // response.
      quic_data2.AddRead(ASYNC, server_maker_.MakeConnectivityProbingPacket(
                                    server_packet_num++, false));
      quic_data2.AddWrite(
          SYNCHRONOUS, client_maker_.MakeAckPacket(packet_number++, 9, 9, 1));
    }
    quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // EOF.
    quic_data2.AddSocketDataToFactory(socket_factory_.get());

    // Cause the connection to report path degrading to the session.
    // Session will start to probe a different port.
    session->connection()->OnPathDegradingTimeout();

    // Next connectivity probe is scheduled to be sent in 2 *
    // kDefaultRTTMilliSecs.
    EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
    next_task_delay = task_runner->NextPendingTaskDelay();
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
              next_task_delay);

    // The connection should still be alive, and not marked as going away.
    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(host_port_pair_));
    EXPECT_EQ(1u, session->GetNumActiveStreams());

    // Resume quic data and a connectivity probe response will be read on the
    // new socket.
    quic_data2.Resume();
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
    EXPECT_TRUE(HasActiveSession(host_port_pair_));
    EXPECT_EQ(1u, session->GetNumActiveStreams());

    if (i < 4) {
      // There should be pending tasks, the nearest one will complete
      // migration to the new port.
      EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
      next_task_delay = task_runner->NextPendingTaskDelay();
      EXPECT_EQ(base::TimeDelta(), next_task_delay);
    } else {
      // Last attempt to migrate will abort due to hitting the limit of max
      // number of allowed migrations.
      EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
      next_task_delay = task_runner->NextPendingTaskDelay();
      EXPECT_NE(base::TimeDelta(), next_task_delay);
    }
    task_runner->FastForwardBy(next_task_delay);
    EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
    // The last round of migration will abort upon reading the probing response.
    // Future reads in the same socket is ignored.
    EXPECT_EQ(i != 4, quic_data2.AllReadDataConsumed());
  }

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
}

// This test verifies that the session marks itself GOAWAY on path degrading
// and it does not receive any new request
TEST_P(QuicStreamFactoryTest, GoawayOnPathDegrading) {
  quic_params_->go_away_on_path_degrading = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, true));
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Creat request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cerf_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger the connection to report path degrading to the session.
  // Session will mark itself GOAWAY.
  session->connection()->OnPathDegradingTimeout();

  // The connection should still be alive, but marked as going away.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Second request should be sent on a new connection.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // Resume the data, verify old request can read response on the old session
  // successfully.
  quic_data1.Resume();
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(0U, session->GetNumActiveStreams());

  // Check an active session exists for the destination.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));
  EXPECT_NE(session, session2);

  stream.reset();
  stream2.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that the connection will not migrate to a bad socket
// when path degrading is detected.
TEST_P(QuicStreamFactoryTest, DoNotMigrateToBadSocketOnPathDegrading) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ConstructOkResponsePacket(
                               1, GetNthClientInitiatedBidirectionalStreamId(0),
                               false, false));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket that will immediately return disconnected.
  // The stream factory will abort probe the alternate network.
  MockConnect bad_connect = MockConnect(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  SequencedSocketData socket_data(bad_connect, base::span<MockRead>(),
                                  base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data);

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Session will start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume the data, and response header is received over the original network.
  quic_data.Resume();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Verify there is no pending task as probing alternate network is halted.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/847569.
// This test verifies that the connection migrates to the alternate network
// early when there is no active stream but a draining stream.
// The first packet being written after migration is a synchrnous write, which
// will cause a PING packet being sent.
TEST_P(QuicStreamFactoryTest, MigrateSessionWithDrainingStreamSync) {
  TestMigrateSessionWithDrainingStream(SYNCHRONOUS);
}

// Regression test for http://crbug.com/847569.
// This test verifies that the connection migrates to the alternate network
// early when there is no active stream but a draining stream.
// The first packet being written after migration is an asynchronous write, no
// PING packet will be sent.
TEST_P(QuicStreamFactoryTest, MigrateSessionWithDrainingStreamAsync) {
  TestMigrateSessionWithDrainingStream(ASYNC);
}

void QuicStreamFactoryTestBase::TestMigrateSessionWithDrainingStream(
    IoMode write_mode_for_queued_packet) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  int packet_number = 1;
  MockQuicData quic_data1(version_);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  // Read an out of order packet with FIN to drain the stream.
  quic_data1.AddRead(
      ASYNC, ConstructOkResponsePacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                 true));  // keep sending version.
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_number++, false));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(3, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(write_mode_for_queued_packet,
                      client_maker_.MakeAckPacket(packet_number++, 2, 3, 3, 1));
  if (write_mode_for_queued_packet == SYNCHRONOUS) {
    quic_data2.AddWrite(ASYNC,
                        client_maker_.MakePingPacket(packet_number++, false));
  }
  server_maker_.Reset();
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.MakeAckPacket(packet_number++, 1, 3, 1, 1));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run the message loop to receive the out of order packet which contains a
  // FIN and drains the stream.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  // Cause the connection to report path degrading to the session.
  // Session should still start to probe the alternate network.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());
  EXPECT_EQ(1u, session->GetNumDrainingStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  base::RunLoop().RunUntilIdle();

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/835444.
// This test verifies that the connection migrates to the alternate network
// when the alternate network is connected after path has been degrading.
TEST_P(QuicStreamFactoryTest, MigrateOnNewNetworkConnectAfterPathDegrading) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->QueueNetworkMadeDefault(kDefaultNetworkForTests);

  MockQuicData quic_data1(version_);
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging Read.
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data1.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the second socket data provider that is used after migration.
  // The response to the earlier request is read on the new socket.
  MockQuicData quic_data2(version_);
  // Connectivity probe to be sent on the new path.
  quic_data2.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_num++, true));
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data2.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));
  // Ping packet to send after migration is completed.
  quic_data2.AddWrite(
      ASYNC, client_maker_.MakeAckAndPingPacket(packet_num++, false, 1, 1, 1));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 2, 2, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 2, 2, 1));
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Cause the connection to report path degrading to the session.
  // Due to lack of alternate network, session will not mgirate connection.
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  session->connection()->OnPathDegradingTimeout();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Deliver a signal that a alternate network is connected now, this should
  // cause the connection to start early migration on path degrading.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Next connectivity probe is scheduled to be sent in 2 *
  // kDefaultRTTMilliSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // The connection should still be alive, and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume quic data and a connectivity probe response will be read on the new
  // socket.
  quic_data2.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be three pending tasks, the nearest one will complete
  // migration to the new network.
  EXPECT_EQ(3u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta(), next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Now there are two pending tasks, the nearest one was to send connectivity
  // probe and has been cancelled due to successful migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There's one more task to mgirate back to the default network in 0.4s.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs) -
      base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs);
  EXPECT_EQ(expected_delay, next_task_delay);

  // Deliver a signal that the alternate network now becomes default to session,
  // this will cancel mgirate back to default network timer.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  task_runner->FastForwardBy(next_task_delay);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Verify that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  stream.reset();
  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that multiple sessions are migrated on connection
// migration signal.
TEST_P(QuicStreamFactoryTest,
       MigrateMultipleSessionsToBadSocketsAfterDisconnected) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddWrite(ASYNC, OK);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddWrite(ASYNC, OK);
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server1(kDefaultServerHostName, 443);
  HostPortPair server2(kServer2HostName, 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(server1.host(), "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.2", "");

  // Create request and QuicHttpStream to create session1.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(OK,
            request1.Request(
                server1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  // Create request and QuicHttpStream to create session2.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession* session1 = GetActiveSession(server1);
  QuicChromiumClientSession* session2 = GetActiveSession(server2);
  EXPECT_NE(session1, session2);

  // Cause QUIC stream to be created and send GET so session1 has an open
  // stream.
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = url_;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));
  HttpResponseInfo response1;
  HttpRequestHeaders request_headers1;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers1, &response1,
                                     callback_.callback()));

  // Cause QUIC stream to be created and send GET so session2 has an open
  // stream.
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = url_;
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback_.callback()));

  // Cause both sessions to be paused due to DISCONNECTED.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // Ensure that both sessions are paused but alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session1));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  // Add new sockets to use post migration. Those are bad sockets and will cause
  // migration to fail.
  MockConnect connect_result =
      MockConnect(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  SequencedSocketData socket_data3(connect_result, base::span<MockRead>(),
                                   base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data3);
  SequencedSocketData socket_data4(connect_result, base::span<MockRead>(),
                                   base::span<MockWrite>());
  socket_factory_->AddSocketDataProvider(&socket_data4);

  // Connect the new network and cause migration to bad sockets, causing
  // sessions to close.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session1));
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// This test verifies that session attempts connection migration with signals
// delivered in the following order (no alternate network is available):
// - path degrading is detected: session attempts connection migration but no
//   alternate network is available, session caches path degrading signal in
//   connection and stays on the original network.
// - original network backs up, request is served in the orignal network,
//   session is not marked as going away.
TEST_P(QuicStreamFactoryTest, MigrateOnPathDegradingWithNoNewNetwork) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause for path degrading signal.

  // The rest of the data will still flow in the original socket as there is no
  // new network after path degrading.
  quic_data.AddRead(ASYNC, ConstructOkResponsePacket(
                               1, GetNthClientInitiatedBidirectionalStreamId(0),
                               false, false));
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Trigger connection migration on path degrading. Since there are no networks
  // to migrate to, the session will remain on the original network, not marked
  // as going away.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_TRUE(session->connection()->IsPathDegrading());

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Resume so that rest of the data will flow in the original socket.
  quic_data.Resume();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// This test verifies that session with non-migratable stream will probe the
// alternate network on path degrading, and close the non-migratable streams
// when probe is successful.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionEarlyNonMigratableStream_DoNotMigrateIdleSessions) {
  TestMigrateSessionEarlyNonMigratableStream(false);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionEarlyNonMigratableStream_MigrateIdleSessions) {
  TestMigrateSessionEarlyNonMigratableStream(true);
}

void QuicStreamFactoryTestBase::TestMigrateSessionEarlyNonMigratableStream(
    bool migrate_idle_sessions) {
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }

  // Set up the second socket data provider that is used for probing.
  MockQuicData quic_data1(version_);
  // Connectivity probe to be sent on the new path.
  quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_num++, true));
  quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Connectivity probe to receive from the server.
  quic_data1.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(1, false));

  if (migrate_idle_sessions) {
    quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    if (VersionUsesHttp3(version_.transport_version)) {
      quic_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeDataAndRstPacket(
                              packet_num++, true, GetQpackDecoderStreamId(),
                              StreamCancellationQpackDecoderInstruction(0),
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
    } else {
      quic_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_num++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
    }
    // Ping packet to send after migration is completed.
    quic_data1.AddWrite(SYNCHRONOUS, client_maker_.MakeAckAndPingPacket(
                                         packet_num++, false, 1, 1, 1));
  } else {
    if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
      if (VersionUsesHttp3(version_.transport_version)) {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeDataRstAckAndConnectionClosePacket(
                packet_num++, false, GetQpackDecoderStreamId(),
                StreamCancellationQpackDecoderInstruction(0),
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      } else {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeRstAckAndConnectionClosePacket(
                packet_num++, false,
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      }
    } else {
      if (VersionUsesHttp3(version_.transport_version)) {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeDataRstAckAndConnectionClosePacket(
                packet_num++, false, GetQpackDecoderStreamId(),
                StreamCancellationQpackDecoderInstruction(0),
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      } else {
        socket_data.AddWrite(
            SYNCHRONOUS,
            client_maker_.MakeRstAckAndConnectionClosePacket(
                packet_num++, false,
                GetNthClientInitiatedBidirectionalStreamId(0),
                quic::QUIC_STREAM_CANCELLED, 1, 1, 1,
                quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                "net error"));
      }
    }
  }

  socket_data.AddSocketDataToFactory(socket_factory_.get());
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to migrate.
  session->OnPathDegrading();

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Resume the data to read the connectivity probing response to declare probe
  // as successful. Non-migratable streams will be closed.
  quic_data1.Resume();
  if (migrate_idle_sessions)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(host_port_pair_));
  EXPECT_EQ(0u, session->GetNumActiveStreams());

  EXPECT_TRUE(quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionEarlyConnectionMigrationDisabled) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, true,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Trigger connection migration. Since there is a non-migratable stream,
  // this should cause session to be continue without migrating.
  session->OnPathDegrading();

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Regression test for http://crbug.com/791886.
// This test verifies that the old packet writer which encountered an
// asynchronous write error will be blocked during migration on write error. New
// packets would not be written until the one with write error is rewritten on
// the new network.
TEST_P(QuicStreamFactoryTest, MigrateSessionOnAsyncWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  // base::RunLoop() controls mocked socket writes and reads.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
          GetNthClientInitiatedBidirectionalStreamId(0), true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(),
                         /* should_include_version = */ true,
                         /* fin = */ false,
                         StreamCancellationQpackDecoderInstruction(1, false)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED,
                                  /*include_stop_sending_if_v99=*/true));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Request #2 returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream1. This should cause an async write error.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Run the message loop so that asynchronous write completes and a connection
  // migration on write error attempt is posted in QuicStreamFactory's task
  // runner.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  // Send GET request on stream. This will cause another write attempt before
  // migration on write error is exectued.
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback2.callback()));

  // Run the task runner so that migration on write error is finally executed.
  task_runner->RunUntilIdle();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());
  // There should be one task posted to migrate back to the default network in
  // kMinRetryTimeForDefaultNetworkSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs),
            task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// Verify session is not marked as going away after connection migration on
// write error and migrate back to default network logic is applied to bring the
// migrated session back to the default network. Migration singals delivered
// in the following order (alternate network is always availabe):
// - session on the default network encountered a write error;
// - session successfully migrated to the non-default network;
// - session attempts to migrate back to default network post migration;
// - migration back to the default network is successful.
TEST_P(QuicStreamFactoryTest, MigrateBackToDefaultPostMigrationOnWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData quic_data2(version_);
  quic_data2.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  quic_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request. This should cause an async write error.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Run the message loop so that asynchronous write completes and a connection
  // migration on write error attempt is posted in QuicStreamFactory's task
  // runner.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  // Run the task runner so that migration on write error is finally executed.
  task_runner->RunUntilIdle();

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  // There should be one task posted to migrate back to the default network in
  // kMinRetryTimeForDefaultNetworkSecs.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta expected_delay =
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs);
  EXPECT_EQ(expected_delay, task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Set up the third socket data provider for migrate back to default network.
  MockQuicData quic_data3(version_);
  // Connectivity probe to be sent on the new path.
  quic_data3.AddWrite(SYNCHRONOUS, client_maker_.MakeConnectivityProbingPacket(
                                       packet_num++, false));
  // Connectivity probe to receive from the server.
  quic_data3.AddRead(ASYNC,
                     server_maker_.MakeConnectivityProbingPacket(2, false));
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data3.AddWrite(ASYNC,
                      client_maker_.MakeAckPacket(packet_num++, 1, 2, 1, 1));
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data3.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  quic_data3.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED,
                                  /*include_stop_sending_if_v99=*/true));
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  // Fast forward to fire the migrate back timer and verify the session
  // successfully migrates back to the default network.
  task_runner->FastForwardBy(expected_delay);

  // Verify the session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // There should be one task posted to one will resend a connectivity probe and
  // the other will retry migrate back, both are cancelled.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  task_runner->FastForwardBy(
      base::TimeDelta::FromSeconds(2 * kMinRetryTimeForDefaultNetworkSecs));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  stream1.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data3.AllReadDataConsumed());
  EXPECT_TRUE(quic_data3.AllWriteDataConsumed());
}

// This test verifies that the connection will not attempt connection migration
// (send connectivity probes on alternate path) when path degrading is detected
// and handshake is not confirmed.
TEST_P(QuicStreamFactoryTest,
       NoMigrationOnPathDegradingBeforeHandshakeConfirmed) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Cause the connection to report path degrading to the session.
  // Session will ignore the signal as handshake is not completed.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// This test verifies that if a connection is closed with
// QUIC_NETWORK_IDLE_TIMEOUT before handshake is completed and there is no
// alternate network, no new connection will be created.
TEST_P(QuicStreamFactoryTest, NoAlternateNetworkBeforeHandshakeOnIdleTimeout) {
  TestNoAlternateNetworkBeforeHandshake(quic::QUIC_NETWORK_IDLE_TIMEOUT);
}

// This test verifies that if a connection is closed with QUIC_HANDSHAKE_TIMEOUT
// and there is no alternate network, no new connection will be created.
TEST_P(QuicStreamFactoryTest, NoAlternateNetworkOnHandshakeTimeout) {
  TestNoAlternateNetworkBeforeHandshake(quic::QUIC_HANDSHAKE_TIMEOUT);
}

void QuicStreamFactoryTestBase::TestNoAlternateNetworkBeforeHandshake(
    quic::QuicErrorCode quic_error) {
  DCHECK(quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
         quic_error == quic::QUIC_HANDSHAKE_TIMEOUT);
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Cause the connection to report path degrading to the session.
  // Session will ignore the signal as handshake is not completed.
  session->connection()->OnPathDegradingTimeout();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Cause the connection to close due to |quic_error| before handshake.
  std::string error_details;
  if (quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT) {
    error_details = "No recent network activity.";
  } else {
    error_details = "Handshake timeout expired.";
  }
  session->connection()->CloseConnection(
      quic_error, error_details, quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // A task will be posted to clean up the session in the factory.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  task_runner->FastForwardUntilNoTasksRemain();

  // No new session should be created as there is no alternate network.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, NewConnectionBeforeHandshakeAfterIdleTimeout) {
  TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QUIC_NETWORK_IDLE_TIMEOUT);
}

TEST_P(QuicStreamFactoryTest, NewConnectionAfterHandshakeTimeout) {
  TestNewConnectionOnAlternateNetworkBeforeHandshake(
      quic::QUIC_HANDSHAKE_TIMEOUT);
}

// Sets up a test to verify that a new connection will be created on the
// alternate network after the initial connection fails before handshake with
// signals delivered in the following order (alternate network is available):
// - the default network is not able to complete crypto handshake;
// - the original connection is closed with |quic_error|;
// - a new connection is created on the alternate network and is able to finish
//   crypto handshake;
// - the new session on the alternate network attempts to migrate back to the
//   default network by sending probes;
// - default network being disconnected is delivered: session will stop probing
//   the original network.
// - alternate network is made by default.
void QuicStreamFactoryTestBase::
    TestNewConnectionOnAlternateNetworkBeforeHandshake(
        quic::QuicErrorCode quic_error) {
  DCHECK(quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
         quic_error == quic::QUIC_HANDSHAKE_TIMEOUT);
  quic_params_->retry_on_alternate_network_before_handshake = true;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  // Use cold start mode to send crypto message for handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // Socket data for connection on the default network.
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for connection on the alternate network.
  MockQuicData socket_data2(version_);
  int packet_num = 1;
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDummyCHLOPacket(packet_num++));
  socket_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket(packet_num++));
  socket_data2.AddWrite(
      ASYNC, ConstructGetRequestPacket(
                 packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                 true, true));
  socket_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  int probing_packet_num = packet_num++;

  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for probing on the default network.
  MockQuicData probing_data(version_);
  probing_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  probing_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeConnectivityProbingPacket(probing_packet_num, false));
  probing_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Ensure that session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  EXPECT_FALSE(failed_on_default_network_);

  std::string error_details;
  if (quic_error == quic::QUIC_NETWORK_IDLE_TIMEOUT) {
    error_details = "No recent network activity.";
  } else {
    error_details = "Handshake timeout expired.";
  }
  session->connection()->CloseConnection(
      quic_error, error_details, quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // A task will be posted to clean up the session in the factory.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  task_runner->FastForwardUntilNoTasksRemain();

  // Verify a new session is created on the alternate network.
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  QuicChromiumClientSession* session2 = GetPendingSession(host_port_pair_);
  EXPECT_NE(session, session2);
  EXPECT_TRUE(failed_on_default_network_);

  // Confirm the handshake on the alternate network.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  // Resume the data now so that data can be sent and read.
  socket_data2.Resume();

  // Create the stream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));
  // Send the request.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop to finish asynchronous mock write.
  base::RunLoop().RunUntilIdle();
  // Read the response.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // There should be a new task posted to migrate back to the default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  base::TimeDelta next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs),
            next_task_delay);
  task_runner->FastForwardBy(next_task_delay);

  // There should be two tasks posted. One will retry probing and the other
  // will retry migrate back.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  next_task_delay = task_runner->NextPendingTaskDelay();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2 * kDefaultRTTMilliSecs),
            next_task_delay);

  // Deliver the signal that the default network is disconnected.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // Verify no connectivity probes will be sent as probing will be cancelled.
  task_runner->FastForwardUntilNoTasksRemain();
  // Deliver the signal that the alternate network is made default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Test that connection will be closed with PACKET_WRITE_ERROR if a write error
// is triggered before handshake is confirmed and connection migration is turned
// on.
TEST_P(QuicStreamFactoryTest, MigrationOnWriteErrorBeforeHandshakeConfirmed) {
  DCHECK(!quic_params_->retry_on_alternate_network_before_handshake);
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(ERR_QUIC_HANDSHAKE_FAILED, callback_.WaitForResult());
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Verify new requests can be sent normally.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  // Run the message loop to complete host resolution.
  base::RunLoop().RunUntilIdle();

  // Complete handshake. QuicStreamFactory::Job should complete and succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Create QuicHttpStream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request2);
  EXPECT_TRUE(stream.get());
  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Test that if the original connection is closed with QUIC_PACKET_WRITE_ERROR
// before handshake is confirmed and new connection before handshake is turned
// on, a new connection will be retried on the alternate network.
TEST_P(QuicStreamFactoryTest,
       RetryConnectionOnWriteErrorBeforeHandshakeConfirmed) {
  quic_params_->retry_on_alternate_network_before_handshake = true;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // Socket data for connection on the default network.
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Trigger PACKET_WRITE_ERROR when sending packets in crypto connect.
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket data for connection on the alternate network.
  MockQuicData socket_data2(version_);
  int packet_num = 1;
  socket_data2.AddWrite(SYNCHRONOUS,
                        client_maker_.MakeDummyCHLOPacket(packet_num++));
  socket_data2.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Change the encryption level after handshake is confirmed.
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket(packet_num++));
  socket_data2.AddWrite(
      ASYNC, ConstructGetRequestPacket(
                 packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                 true, true));
  socket_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request, should fail after the write of the CHLO fails.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  // Ensure that the session is alive but not active.
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));
  base::RunLoop().RunUntilIdle();
  QuicChromiumClientSession* session = GetPendingSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));

  // Confirm the handshake on the alternate network.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Resume the data now so that data can be sent and read.
  socket_data2.Resume();

  // Create the stream.
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));
  // Send the request.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop to finish asynchronous mock write.
  base::RunLoop().RunUntilIdle();
  // Read the response.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteError(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that session is alive and not marked as going awya.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnWriteErrorSynchronous) {
  TestMigrationOnWriteError(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnWriteErrorAsync) {
  TestMigrationOnWriteError(ASYNC);
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorNoNewNetwork(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner, to force the migration alarm timeout later.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream. This causes a write error, which triggers
  // a connection migration attempt. Since there are no networks
  // to migrate to, this causes the session to wait for a new network.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Complete any pending writes. Pending async MockQuicData writes
  // are run on the message loop, not on the test runner.
  base::RunLoop().RunUntilIdle();

  // Write error causes migration task to be posted. Spin the loop.
  if (write_error_mode == ASYNC)
    runner_->RunNextTask();

  // Migration has not yet failed. The session should be alive and active.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_TRUE(session->connection()->writer()->IsWriteBlocked());

  // The migration will not fail until the migration alarm timeout.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Force migration alarm timeout to run.
  RunTestLoopUntilIdle();

  // The connection should be closed. A request for response headers
  // should fail.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));

  NetErrorDetails error_details;
  stream->PopulateNetErrorDetails(&error_details);
  EXPECT_EQ(error_details.quic_connection_error,
            quic::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK);

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNoNewNetworkSynchronous) {
  TestMigrationOnWriteErrorNoNewNetwork(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnWriteErrorNoNewNetworkAsync) {
  TestMigrationOnWriteErrorNoNewNetwork(ASYNC);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithMultipleRequestsSync) {
  TestMigrationOnWriteErrorWithMultipleRequests(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithMultipleRequestsAsync) {
  TestMigrationOnWriteErrorWithMultipleRequests(ASYNC);
}

// Sets up a test which verifies that connection migration on write error can
// eventually succeed and rewrite the packet on the new network with *multiple*
// migratable streams.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorWithMultipleRequests(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }

  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(1, false)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED,
                                  /*include_stop_sending_if_v99=*/true));

  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequestsSync) {
  TestMigrationOnWriteErrorMixedStreams(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequestsAsync) {
  TestMigrationOnWriteErrorMixedStreams(ASYNC);
}

// Sets up a test that verifies connection migration manages to migrate to
// alternate network after encountering a SYNC/ASYNC write error based on
// |write_error_mode| on the original network.
// Note there are mixed types of unfinished requests before migration: one
// migratable and one non-migratable. The *migratable* one triggers write
// error.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMixedStreams(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_number = 1;
  MockQuicData socket_data(version_);

  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_number++));
  }
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstAndDataPacket(
                         packet_number++, true,
                         GetNthClientInitiatedBidirectionalStreamId(1),
                         quic::QUIC_STREAM_CANCELLED, GetQpackDecoderStreamId(),
                         StreamCancellationQpackDecoderInstruction(1)));
  } else {
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_number++, true,
                              GetNthClientInitiatedBidirectionalStreamId(1),
                              quic::QUIC_STREAM_CANCELLED,
                              /*include_stop_sending_if_v99=*/true));
  }
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0, false)));
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_number++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeAckAndRstPacket(
                              packet_number++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream 1. This should cause a write error, which
  // triggers a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that the session is still alive and not marked as going away.
  // Non-migratable stream should be closed due to migration.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequests2Sync) {
  TestMigrationOnWriteErrorMixedStreams2(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateOnWriteErrorWithMixedRequests2Async) {
  TestMigrationOnWriteErrorMixedStreams2(ASYNC);
}

// The one triggers write error is a non-migratable stream.
// Sets up a test that verifies connection migration manages to migrate to
// alternate network after encountering a SYNC/ASYNC write error based on
// |write_error_mode| on the original network.
// Note there are mixed types of unfinished requests before migration: one
// migratable and one non-migratable. The *non-migratable* one triggers write
// error.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMixedStreams2(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_number = 1;
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_number++));
  }
  socket_data.AddWrite(write_error_mode,
                       ERR_ADDRESS_UNREACHABLE);  // Write error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  // The packet triggered writer error will be sent anyway even if the stream
  // will be cancelled later.
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(1),
                                true, true));
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeRstAndDataPacket(
                         packet_number++, true,
                         GetNthClientInitiatedBidirectionalStreamId(1),
                         quic::QUIC_STREAM_CANCELLED, GetQpackDecoderStreamId(),
                         StreamCancellationQpackDecoderInstruction(1)));
  } else {
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_number++, true,
                              GetNthClientInitiatedBidirectionalStreamId(1),
                              quic::QUIC_STREAM_CANCELLED,
                              /*include_stop_sending_if_v99=*/true));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_number++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0, false)));
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeRstPacket(
                              packet_number++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(SYNCHRONOUS,
                          client_maker_.MakeAckAndRstPacket(
                              packet_number++, false,
                              GetNthClientInitiatedBidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request #1 and QuicHttpStream.
  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());

  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org/");
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream1->InitializeStream(&request_info1, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info2.url = GURL("https://www.example.org/");
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info2, true, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(2u, session->GetNumActiveStreams());

  // Send GET request on stream 2 which is non-migratable. This should cause a
  // write error, which triggers a connection migration attempt.
  HttpResponseInfo response2;
  HttpRequestHeaders request_headers2;
  EXPECT_EQ(OK, stream2->SendRequest(request_headers2, &response2,
                                     callback2.callback()));

  // Run the message loop so that the migration attempt is executed and
  // data queued in the new socket is read by the packet reader. Session is
  // still alive and not marked as going away, non-migratable stream will be
  // closed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream 1.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream1->SendRequest(request_headers, &response,
                                     callback_.callback()));

  base::RunLoop().RunUntilIdle();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream1->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream1.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when a connection encounters a packet write error, it
// will cancel non-migratable streams, and migrate to the alternate network.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorNonMigratableStream(
    IoMode write_error_mode,
    bool migrate_idle_sessions) {
  DVLOG(1) << "Write error mode: "
           << ((write_error_mode == SYNCHRONOUS) ? "SYNCHRONOUS" : "ASYNC");
  DVLOG(1) << "Migrate idle sessions: " << migrate_idle_sessions;
  quic_params_->migrate_idle_sessions = migrate_idle_sessions;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData failed_socket_data(version_);
  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (migrate_idle_sessions) {
    // The socket data provider for the original socket before migration.
    failed_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (VersionUsesHttp3(version_.transport_version)) {
      failed_socket_data.AddWrite(SYNCHRONOUS,
                                  ConstructInitialSettingsPacket(packet_num++));
    }
    failed_socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
    failed_socket_data.AddSocketDataToFactory(socket_factory_.get());

    // Set up second socket data provider that is used after migration.
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    // Although the write error occurs when writing a packet for the
    // non-migratable stream and the stream will be cancelled during migration,
    // the packet will still be retransimitted at the connection level.
    socket_data.AddWrite(
        SYNCHRONOUS,
        ConstructGetRequestPacket(packet_num++,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  true, true));
    // A RESET will be sent to the peer to cancel the non-migratable stream.
    if (VersionUsesHttp3(version_.transport_version)) {
      socket_data.AddWrite(
          SYNCHRONOUS,
          client_maker_.MakeRstAndDataPacket(
              packet_num++, true, GetNthClientInitiatedBidirectionalStreamId(0),
              quic::QUIC_STREAM_CANCELLED, GetQpackDecoderStreamId(),
              StreamCancellationQpackDecoderInstruction(0)));
    } else {
      socket_data.AddWrite(
          SYNCHRONOUS,
          client_maker_.MakeRstPacket(
              packet_num++, true, GetNthClientInitiatedBidirectionalStreamId(0),
              quic::QUIC_STREAM_CANCELLED));
    }
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  } else {
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    if (VersionUsesHttp3(version_.transport_version))
      socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
    socket_data.AddSocketDataToFactory(socket_factory_.get());
  }

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created, but marked as non-migratable.
  HttpRequestInfo request_info;
  request_info.load_flags |= LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Run message loop to execute migration attempt.
  base::RunLoop().RunUntilIdle();

  // Migration closes the non-migratable stream and:
  // if migrate idle session is enabled, it migrates to the alternate network
  // successfully; otherwise the connection is closed.
  EXPECT_EQ(migrate_idle_sessions,
            QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(migrate_idle_sessions, HasActiveSession(host_port_pair_));

  if (migrate_idle_sessions) {
    EXPECT_TRUE(failed_socket_data.AllReadDataConsumed());
    EXPECT_TRUE(failed_socket_data.AllWriteDataConsumed());
  }
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(
    QuicStreamFactoryTest,
    MigrateSessionOnWriteErrorNonMigratableStreamSync_DoNotMigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(SYNCHRONOUS, false);
}

TEST_P(
    QuicStreamFactoryTest,
    MigrateSessionOnWriteErrorNonMigratableStreamAsync_DoNotMigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(ASYNC, false);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNonMigratableStreamSync_MigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(SYNCHRONOUS, true);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorNonMigratableStreamAsync_MigrateIdleSessions) {
  TestMigrationOnWriteErrorNonMigratableStream(ASYNC, true);
}

void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorMigrationDisabled(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddWrite(write_error_mode, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set session config to have connection migration disabled.
  quic::test::QuicConfigPeer::SetReceivedDisableConnectionMigration(
      session->config());
  EXPECT_TRUE(session->config()->DisableConnectionMigration());

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run message loop to execute migration attempt.
  base::RunLoop().RunUntilIdle();
  // Migration fails, and session is closed and deleted.
  EXPECT_FALSE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorMigrationDisabledSynchronous) {
  TestMigrationOnWriteErrorMigrationDisabled(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorMigrationDisabledAsync) {
  TestMigrationOnWriteErrorMigrationDisabled(ASYNC);
}

// Sets up a test which verifies that connection migration on write error can
// eventually succeed and rewrite the packet on the new network with singals
// delivered in the following order (alternate network is always availabe):
// - original network encounters a SYNC/ASYNC write error based on
//   |write_error_mode_on_old_network|, the packet failed to be written is
//   cached, session migrates immediately to the alternate network.
// - an immediate SYNC/ASYNC write error based on
//   |write_error_mode_on_new_network| is encountered after migration to the
//   alternate network, session migrates immediately to the original network.
// - an immediate SYNC/ASYNC write error based on
//   |write_error_mode_on_old_network| is encountered after migration to the
//   original network, session migrates immediately to the alternate network.
// - finally, session successfully sends the packet and reads the response on
//   the alternate network.
// TODO(zhongyi): once https://crbug.com/855666 is fixed, this test should be
// modified to test that session is closed early if hopping between networks
// with consecutive write errors is detected.
void QuicStreamFactoryTestBase::TestMigrationOnMultipleWriteErrors(
    IoMode write_error_mode_on_old_network,
    IoMode write_error_mode_on_new_network) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set up the socket data used by the original network, which encounters a
  // write erorr.
  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(write_error_mode_on_old_network,
                        ERR_ADDRESS_UNREACHABLE);  // Write Error
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the socket data used by the alternate network, which also
  // encounters a write error.
  MockQuicData failed_quic_data2(version_);
  failed_quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  failed_quic_data2.AddWrite(write_error_mode_on_new_network, ERR_FAILED);
  failed_quic_data2.AddSocketDataToFactory(socket_factory_.get());

  // Set up the third socket data used by original network, which encounters a
  // write error again.
  MockQuicData failed_quic_data1(version_);
  failed_quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  failed_quic_data1.AddWrite(write_error_mode_on_old_network, ERR_FAILED);
  failed_quic_data1.AddSocketDataToFactory(socket_factory_.get());

  // Set up the last socket data used by the alternate network, which will
  // finish migration successfully. The request is rewritten to this new socket,
  // and the response to the request is read on this socket.
  MockQuicData socket_data2(version_);
  socket_data2.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  // This should encounter a write error on network 1,
  // then migrate to network 2, which encounters another write error,
  // and migrate again to network 1, which encoutners one more write error.
  // Finally the session migrates to network 2 successfully.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(failed_quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(failed_quic_data2.AllWriteDataConsumed());
  EXPECT_TRUE(failed_quic_data1.AllReadDataConsumed());
  EXPECT_TRUE(failed_quic_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsSyncSync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ SYNCHRONOUS,
      /*write_error_mode_on_new_network*/ SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsSyncAsync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ SYNCHRONOUS,
      /*write_error_mode_on_new_network*/ ASYNC);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsAsyncSync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ ASYNC,
      /*write_error_mode_on_new_network*/ SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest, MigrateSessionOnMultipleWriteErrorsAsyncAsync) {
  TestMigrationOnMultipleWriteErrors(
      /*write_error_mode_on_old_network*/ ASYNC,
      /*write_error_mode_on_new_network*/ ASYNC);
}

// Verifies that a connection is closed when connection migration is triggered
// on network being disconnected and the handshake is not confirmed.
TEST_P(QuicStreamFactoryTest, NoMigrationBeforeHandshakeOnNetworkDisconnected) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  // Use cold start mode to do crypto connect, and send CHLO packet on wire.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  // Deliver the network notification, which should cause the connection to be
  // closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  EXPECT_EQ(ERR_NETWORK_CHANGED, callback_.WaitForResult());

  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_FALSE(HasActiveJob(host_port_pair_, privacy_mode_));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Sets up the connection migration test where network change notification is
// queued BEFORE connection migration attempt on write error is posted.
void QuicStreamFactoryTestBase::
    TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
        bool disconnected) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // First queue a network change notification in the message loop.
  if (disconnected) {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkDisconnected(kDefaultNetworkForTests);
  } else {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkMadeDefault(kNewNetworkForTests);
  }
  // Send GET request on stream. This should cause a write error,
  // which triggers a connection migration attempt. This will queue a
  // migration attempt behind the notification in the message loop.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  base::RunLoop().RunUntilIdle();
  // Verify the session is still alive and not marked as going away post
  // migration.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - a notification that default network is disconnected is queued.
// - write error is triggered: session posts a task to attempt connection
//   migration, |migration_pending_| set to true.
// - default network disconnected is delivered: session immediately migrates to
//   the alternate network, |migration_pending_| set to false.
// - connection migration on write error attempt aborts: writer encountered
//   error is no longer in active use.
TEST_P(QuicStreamFactoryTest,
       MigrateOnNetworkDisconnectedWithWriteErrorQueuedLater) {
  TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
      /*disconnected=*/true);
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - a notification that alternate network is made default is queued.
// - write error is triggered: session posts a task to attempt connection
//   migration, block future migrations.
// - new default notification is delivered: migrate back timer spins and task is
//   posted to migrate to the new default network.
// - connection migration on write error attempt proceeds successfully: session
// is
//   marked as going away, future migrations unblocked.
// - migrate back to default network task executed: session is already on the
//   default network, no-op.
TEST_P(QuicStreamFactoryTest,
       MigrateOnWriteErrorWithNetworkMadeDefaultQueuedEarlier) {
  TestMigrationOnNetworkNotificationWithWriteErrorQueuedLater(
      /*disconnected=*/false);
}

// Sets up the connection migration test where network change notification is
// queued AFTER connection migration attempt on write error is posted.
void QuicStreamFactoryTestBase::
    TestMigrationOnWriteErrorWithNotificationQueuedLater(bool disconnected) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream. This should cause a write error,
  // which triggers a connection migration attempt. This will queue a
  // migration attempt in the message loop.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Now queue a network change notification in the message loop behind
  // the migration attempt.
  if (disconnected) {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkDisconnected(kDefaultNetworkForTests);
  } else {
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->QueueNetworkMadeDefault(kNewNetworkForTests);
  }

  base::RunLoop().RunUntilIdle();
  // Verify session is still alive and not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - write error is triggered: session posts a task to complete connection
//   migration.
// - a notification that alternate network is made default is queued.
// - connection migration attempt proceeds successfully, session is marked as
//   going away.
// - new default notification is delivered after connection migration has been
//   completed.
TEST_P(QuicStreamFactoryTest,
       MigrateOnWriteErrorWithNetworkMadeDefaultQueuedLater) {
  TestMigrationOnWriteErrorWithNotificationQueuedLater(/*disconnected=*/false);
}

// This test verifies that session attempts connection migration successfully
// with signals delivered in the following order (alternate network is always
// available):
// - write error is triggered: session posts a task to complete connection
//   migration.
// - a notification that default network is diconnected is queued.
// - connection migration attempt proceeds successfully, session is marked as
//   going away.
// - disconnect notification is delivered after connection migration has been
//   completed.
TEST_P(QuicStreamFactoryTest,
       MigrateOnWriteErrorWithNetworkDisconnectedQueuedLater) {
  TestMigrationOnWriteErrorWithNotificationQueuedLater(/*disconnected=*/true);
}

// This tests connection migration on write error with signals delivered in the
// following order:
// - a synchronous/asynchronous write error is triggered base on
//   |write_error_mode|: connection migration attempt is posted.
// - old default network disconnects, migration waits for a new network.
// - after a pause, new network is connected: session will migrate to new
//   network immediately.
// - migration on writer error is exectued and aborts as writer passed in is no
//   longer active in use.
// - new network is made default.
void QuicStreamFactoryTestBase::TestMigrationOnWriteErrorPauseBeforeConnected(
    IoMode write_error_mode) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Use the test task runner.
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(write_error_mode, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = url_;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // The connection should still be alive, not marked as going away.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after migration.
  // The response to the earlier request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // On a DISCONNECTED notification, nothing happens.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // Add a new network and notify the stream factory of a new connected network.
  // This causes a PING packet to be sent over the new network.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList({kNewNetworkForTests});
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkConnected(kNewNetworkForTests);

  // Ensure that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop migration for write error can finish.
  runner_->RunUntilIdle();

  // Response headers are received over the new network.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Check that the session is still alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // There should be no posted tasks not executed, no way to migrate back to
  // default network.
  EXPECT_TRUE(runner_->GetPostedTasks().empty());

  // Receive signal to mark new network as default.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnSyncWriteErrorPauseBeforeConnected) {
  TestMigrationOnWriteErrorPauseBeforeConnected(SYNCHRONOUS);
}

TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnAsyncWriteErrorPauseBeforeConnected) {
  TestMigrationOnWriteErrorPauseBeforeConnected(ASYNC);
}

// This test verifies that when session successfully migrate to the alternate
// network, packet write error on the old writer will be ignored and will not
// trigger connection migration on write error.
TEST_P(QuicStreamFactoryTest, IgnoreWriteErrorFromOldWriterAfterMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can verify whether the migrate on
  // write error task is posted.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddWrite(
      ASYNC, ERR_ADDRESS_UNREACHABLE,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // There will be two pending task, one will complete migration with no delay
  // and the other will attempt to migrate back to the default network with
  // delay.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a write error will be delivered to the old
  // packet writer. Verify no additional task is posted.
  socket_data.Resume();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  stream.reset();
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when session successfully migrate to the alternate
// network, packet read error on the old reader will be ignored and will not
// close the connection.
TEST_P(QuicStreamFactoryTest, IgnoreReadErrorFromOldReaderAfterMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // There will be two pending task, one will complete migration with no delay
  // and the other will attempt to migrate back to the default network with
  // delay.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that after migration on network is executed, packet
// read error on the old reader will be ignored and will not close the
// connection.
TEST_P(QuicStreamFactoryTest, IgnoreReadErrorOnOldReaderDuringMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  // There will be two pending task, one will complete migration with no delay
  // and the other will attempt to migrate back to the default network with
  // delay.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());

  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when connection migration on path degrading is
// enabled, and no custom retransmittable on wire timeout is specified, the
// default value is used.
TEST_P(QuicStreamFactoryTest, DefaultRetransmittableOnWireTimeoutForMigration) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  // The PING packet sent post migration.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++, true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immediately.
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 base::StringPiece("Hello World")));

  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 3, 1, 1));
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeAckPacket(packet_num++, 2, 1, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++, false));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Ack delay time.
  base::TimeDelta delay = task_runner->NextPendingTaskDelay();
  EXPECT_GT(kDefaultRetransmittableOnWireTimeout, delay);
  // Fire the ack alarm, since ack has been sent, no ack will be sent.
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  delay = kDefaultRetransmittableOnWireTimeout - delay;
  EXPECT_EQ(delay, task_runner->NextPendingTaskDelay());
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when connection migration on path degrading is
// enabled, and a custom retransmittable on wire timeout is specified, the
// custom value is used.
TEST_P(QuicStreamFactoryTest, CustomRetransmittableOnWireTimeoutForMigration) {
  constexpr base::TimeDelta custom_timeout_value =
      base::TimeDelta::FromMilliseconds(200);
  quic_params_->retransmittable_on_wire_timeout = custom_timeout_value;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  // The PING packet sent post migration.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++, true));
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 base::StringPiece("Hello World")));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 3, 1, 1));
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeAckPacket(packet_num++, 2, 1, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++, false));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Now notify network is disconnected, cause the migration to complete
  // immediately.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Ack delay time.
  base::TimeDelta delay = task_runner->NextPendingTaskDelay();
  EXPECT_GT(custom_timeout_value, delay);
  // Fire the ack alarm, since ack has been sent, no ack will be sent.
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  delay = custom_timeout_value - delay;
  EXPECT_EQ(delay, task_runner->NextPendingTaskDelay());
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  socket_data.Resume();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when no migration is enabled, but a custom value for
// retransmittable-on-wire timeout is specified, the ping alarm is set up to
// send retransmittable pings with the custom value.
TEST_P(QuicStreamFactoryTest, CustomRetransmittableOnWireTimeout) {
  constexpr base::TimeDelta custom_timeout_value =
      base::TimeDelta::FromMilliseconds(200);
  quic_params_->retransmittable_on_wire_timeout = custom_timeout_value;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 base::StringPiece("Hello World")));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1, 1));
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeAckPacket(packet_num++, 2, 1, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++, false));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Ack delay time.
  base::TimeDelta delay = task_runner->NextPendingTaskDelay();
  EXPECT_GT(custom_timeout_value, delay);
  // Fire the ack alarm, since ack has been sent, no ack will be sent.
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  delay = custom_timeout_value - delay;
  EXPECT_EQ(delay, task_runner->NextPendingTaskDelay());
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when no migration is enabled, and no custom value
// for retransmittable-on-wire timeout is specified, the ping alarm will not
// send any retransmittable pings.
TEST_P(QuicStreamFactoryTest, NoRetransmittableOnWireTimeout) {
  // Use non-default initial srtt so that if QPACK emits additional setting
  // packet, it will not have the same retransmission timeout as the
  // default value of retransmittable-on-wire-ping timeout.
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMilliseconds(200);
  http_server_properties_->SetServerNetworkStats(url::SchemeHostPort(url_),
                                                 NetworkIsolationKey(), stats);
  quic_params_->estimate_initial_rtt = true;

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 base::StringPiece("Hello World")));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1, 1));
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeAckPacket(packet_num++, 2, 1, 1));
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Ack delay time.
  base::TimeDelta delay = task_runner->NextPendingTaskDelay();
  EXPECT_GT(kDefaultRetransmittableOnWireTimeout, delay);
  // Fire the ack alarm, since ack has been sent, no ack will be sent.
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Verify that the ping alarm is not set with any default value.
  base::TimeDelta wrong_delay = kDefaultRetransmittableOnWireTimeout - delay;
  delay = task_runner->NextPendingTaskDelay();
  EXPECT_NE(wrong_delay, delay);
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when only migration on network change is enabled, and
// a custom value for retransmittable-on-wire is specified, the ping alarm will
// send retransmittable pings to the peer with custom value.
TEST_P(QuicStreamFactoryTest,
       CustomRetransmittableOnWireTimeoutWithMigrationOnNetworkChangeOnly) {
  constexpr base::TimeDelta custom_timeout_value =
      base::TimeDelta::FromMilliseconds(200);
  quic_params_->retransmittable_on_wire_timeout = custom_timeout_value;
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 base::StringPiece("Hello World")));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1, 1));
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeAckPacket(packet_num++, 2, 1, 1));
  // The PING packet sent for retransmittable on wire.
  socket_data1.AddWrite(SYNCHRONOUS,
                        client_maker_.MakePingPacket(packet_num++, false));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Ack delay time.
  base::TimeDelta delay = task_runner->NextPendingTaskDelay();
  EXPECT_GT(custom_timeout_value, delay);
  // Fire the ack alarm, since ack has been sent, no ack will be sent.
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Fire the ping alarm with retransmittable-on-wire timeout, send PING.
  delay = custom_timeout_value - delay;
  EXPECT_EQ(delay, task_runner->NextPendingTaskDelay());
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  socket_data1.Resume();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that when only migration on network change is enabled, and
// no custom value for retransmittable-on-wire is specified, the ping alarm will
// NOT send retransmittable pings to the peer with custom value.
TEST_P(QuicStreamFactoryTest,
       NoRetransmittableOnWireTimeoutWithMigrationOnNetworkChangeOnly) {
  // Use non-default initial srtt so that if QPACK emits additional setting
  // packet, it will not have the same retransmission timeout as the
  // default value of retransmittable-on-wire-ping timeout.
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMilliseconds(200);
  http_server_properties_->SetServerNetworkStats(url::SchemeHostPort(url_),
                                                 NetworkIsolationKey(), stats);
  quic_params_->estimate_initial_rtt = true;
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  // Read two packets so that client will send ACK immedaitely.
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(
      ASYNC, server_maker_.MakeDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 base::StringPiece("Hello World")));
  // Read an ACK from server which acks all client data.
  socket_data1.AddRead(SYNCHRONOUS, server_maker_.MakeAckPacket(3, 2, 1, 1));
  socket_data1.AddWrite(ASYNC,
                        client_maker_.MakeAckPacket(packet_num++, 2, 1, 1));
  std::string header = ConstructDataHeader(6);
  socket_data1.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read.
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Complete migration.
  task_runner->RunUntilIdle();
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  socket_data1.Resume();
  // Spin up the message loop to read incoming data from server till the ACK.
  base::RunLoop().RunUntilIdle();

  // Ack delay time.
  base::TimeDelta delay = task_runner->NextPendingTaskDelay();
  EXPECT_GT(kDefaultRetransmittableOnWireTimeout, delay);
  // Fire the ack alarm, since ack has been sent, no ack will be sent.
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Verify the ping alarm is not set with default value.
  base::TimeDelta wrong_delay = kDefaultRetransmittableOnWireTimeout - delay;
  delay = task_runner->NextPendingTaskDelay();
  EXPECT_NE(wrong_delay, delay);
  context_.AdvanceTime(
      quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume the old socket data, a read error will be delivered to the old
  // packet reader. Verify that the session is not affected.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies that after migration on write error is posted, packet
// read error on the old reader will be ignored and will not close the
// connection.
TEST_P(QuicStreamFactoryTest,
       IgnoreReadErrorOnOldReaderDuringPendingMigrationOnWriteError) {
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(ASYNC, ERR_FAILED);              // Write error.
  socket_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);  // Read error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Set up second socket data provider that is used after
  // migration. The request is written to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));

  socket_data1.AddRead(ASYNC, ERR_IO_PENDING);  // Pause.
  socket_data1.AddRead(ASYNC, ERR_FAILED);  // Read error to close connection.
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());
  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop to complete asynchronous write and read with errors.
  base::RunLoop().RunUntilIdle();
  // There will be one pending task to complete migration on write error.
  // Verify session is not closed with read error.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Complete migration.
  task_runner->RunUntilIdle();
  // There will be one more task posted attempting to migrate back to the
  // default network.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  // Resume to consume the read error on new socket, which will close
  // the connection.
  socket_data1.Resume();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// Migrate on asynchronous write error, old network disconnects after alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectAfterConnectAsync) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      ASYNC, /*disconnect_before_connect*/ false);
}

// Migrate on synchronous write error, old network disconnects after alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectAfterConnectSync) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      SYNCHRONOUS, /*disconnect_before_connect*/ false);
}

// Migrate on asynchronous write error, old network disconnects before alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectBeforeConnectAsync) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      ASYNC, /*disconnect_before_connect*/ true);
}

// Migrate on synchronous write error, old network disconnects before alternate
// network connects.
TEST_P(QuicStreamFactoryTest,
       MigrateSessionOnWriteErrorWithDisconnectBeforeConnectSync) {
  TestMigrationOnWriteErrorWithMultipleNotifications(
      SYNCHRONOUS, /*disconnect_before_connect*/ true);
}

// Setps up test which verifies that session successfully migrate to alternate
// network with signals delivered in the following order:
// *NOTE* Signal (A) and (B) can reverse order based on
// |disconnect_before_connect|.
// - (No alternate network is connected) session connects to
//   kDefaultNetworkForTests.
// - An async/sync write error is encountered based on |write_error_mode|:
//   session posted task to migrate session on write error.
// - Posted task is executed, miration moves to pending state due to lack of
//   alternate network.
// - (A) An alternate network is connected, pending migration completes.
// - (B) Old default network disconnects, no migration will be attempted as
//   session has already migrate to the alternate network.
// - The alternate network is made default.
void QuicStreamFactoryTestBase::
    TestMigrationOnWriteErrorWithMultipleNotifications(
        IoMode write_error_mode,
        bool disconnect_before_connect) {
  InitializeConnectionMigrationV2Test({kDefaultNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(write_error_mode, ERR_FAILED);  // Write error.
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream. This should cause a write error, which triggers
  // a connection migration attempt.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));
  // Run the message loop so that posted task to migrate to socket will be
  // executed. A new task will be posted to wait for a new network.
  base::RunLoop().RunUntilIdle();

  // In this particular code path, the network will not yet be marked
  // as going away and the session will still be alive.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());
  EXPECT_EQ(ERR_IO_PENDING, stream->ReadResponseHeaders(callback_.callback()));

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data1(version_);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data1.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->SetConnectedNetworksList(
          {kDefaultNetworkForTests, kNewNetworkForTests});
  if (disconnect_before_connect) {
    // Now deliver a DISCONNECT notification.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

    // Now deliver a CONNECTED notification and completes migration.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkConnected(kNewNetworkForTests);
  } else {
    // Now deliver a CONNECTED notification and completes migration.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkConnected(kNewNetworkForTests);

    // Now deliver a DISCONNECT notification.
    scoped_mock_network_change_notifier_->mock_network_change_notifier()
        ->NotifyNetworkDisconnected(kDefaultNetworkForTests);
  }
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // This is the callback for the response headers that returned
  // pending previously, because no result was available.  Check that
  // the result is now available due to the successful migration.
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(200, response.headers->response_code());

  // Deliver a MADEDEFAULT notification.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(session, GetActiveSession(host_port_pair_));

  stream.reset();
  stream2.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

// This test verifies after session migrates off the default network, it keeps
// retrying migrate back to the default network until successfully gets on the
// default network or the idle migration period threshold is exceeded.
// The default threshold is 30s.
TEST_P(QuicStreamFactoryTest, DefaultIdleMigrationPeriod) {
  quic_params_->migrate_idle_sessions = true;
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner and a test tick tock.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetTickClock(factory_.get(),
                                      task_runner->GetMockTickClock());

  MockQuicData default_socket_data(version_);
  default_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    default_socket_data.AddWrite(SYNCHRONOUS,
                                 ConstructInitialSettingsPacket(packet_num++));
  }
  default_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after migration.
  MockQuicData alternate_socket_data(version_);
  alternate_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  // Ping packet to send after migration.
  alternate_socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  alternate_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up probing socket for migrating back to the default network.
  MockQuicData quic_data(version_);                // retry count: 0.
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data1(version_);                // retry count: 1
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2(version_);                // retry count: 2
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data3(version_);                // retry count: 3
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data3.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data4(version_);                // retry count: 4
  quic_data4.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data4.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data4.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data5(version_);                // retry count: 5
  quic_data5.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data5.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data5.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is active.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The nearest task will complete migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // The migrate back timer will fire. Due to default network
  // being disconnected, no attempt will be exercised to migrate back.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs),
            task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Deliver the signal that the old default network now backs up.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  // A task is posted to migrate back to the default network immediately.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // Retry migrate back in 1, 2, 4, 8, 16s.
  // Session will be closed due to idle migration timeout.
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(HasActiveSession(host_port_pair_));
    // A task is posted to migrate back to the default network in 2^i seconds.
    EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
    EXPECT_EQ(base::TimeDelta::FromSeconds(UINT64_C(1) << i),
              task_runner->NextPendingTaskDelay());
    task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  }

  EXPECT_TRUE(default_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(default_socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, CustomIdleMigrationPeriod) {
  // The customized threshold is 15s.
  quic_params_->migrate_idle_sessions = true;
  quic_params_->idle_session_migration_period =
      base::TimeDelta::FromSeconds(15);
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner and a test tick tock.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetTickClock(factory_.get(),
                                      task_runner->GetMockTickClock());

  MockQuicData default_socket_data(version_);
  default_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    default_socket_data.AddWrite(SYNCHRONOUS,
                                 ConstructInitialSettingsPacket(packet_num++));
  }
  default_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up second socket data provider that is used after migration.
  MockQuicData alternate_socket_data(version_);
  alternate_socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  // Ping packet to send after migration.
  alternate_socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  alternate_socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Set up probing socket for migrating back to the default network.
  MockQuicData quic_data(version_);                // retry count: 0.
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data1(version_);                // retry count: 1
  quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data1.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2(version_);                // retry count: 2
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data3(version_);                // retry count: 3
  quic_data3.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data3.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data3.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data4(version_);                // retry count: 4
  quic_data4.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data4.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data4.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Ensure that session is active.
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Trigger connection migration. Since there are no active streams,
  // the session will be closed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  // The nearest task will complete migration.
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // The migrate back timer will fire. Due to default network
  // being disconnected, no attempt will be exercised to migrate back.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs),
            task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  // Deliver the signal that the old default network now backs up.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kDefaultNetworkForTests);

  // A task is posted to migrate back to the default network immediately.
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(base::TimeDelta(), task_runner->NextPendingTaskDelay());
  task_runner->FastForwardBy(base::TimeDelta());

  // Retry migrate back in 1, 2, 4, 8s.
  // Session will be closed due to idle migration timeout.
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(HasActiveSession(host_port_pair_));
    // A task is posted to migrate back to the default network in 2^i seconds.
    EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
    EXPECT_EQ(base::TimeDelta::FromSeconds(UINT64_C(1) << i),
              task_runner->NextPendingTaskDelay());
    task_runner->FastForwardBy(task_runner->NextPendingTaskDelay());
  }

  EXPECT_TRUE(default_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(default_socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllReadDataConsumed());
  EXPECT_TRUE(alternate_socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigration) {
  quic_params_->allow_server_migration = true;
  Initialize();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      ConstructGetRequestPacket(packet_num++,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                true, true));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Send GET request on stream.
  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  EXPECT_EQ(OK, stream->SendRequest(request_headers, &response,
                                    callback_.callback()));

  IPEndPoint ip;
  session->GetDefaultSocket()->GetPeerAddress(&ip);
  DVLOG(1) << "Socket connected to: " << ip.address().ToString() << " "
           << ip.port();

  // Set up second socket data provider that is used after
  // migration. The request is rewritten to this new socket, and the
  // response to the request is read on this new socket.
  MockQuicData socket_data2(version_);
  socket_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakePingPacket(packet_num++, /*include_version=*/true));
  socket_data2.AddRead(
      ASYNC,
      ConstructOkResponsePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), false, false));
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED));
  } else {
    socket_data2.AddWrite(
        SYNCHRONOUS,
        client_maker_.MakeAckAndRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  const uint8_t kTestIpAddress[] = {1, 2, 3, 4};
  const uint16_t kTestPort = 123;
  session->Migrate(NetworkChangeNotifier::kInvalidNetworkHandle,
                   IPEndPoint(IPAddress(kTestIpAddress), kTestPort), true,
                   net_log_);

  session->GetDefaultSocket()->GetPeerAddress(&ip);
  DVLOG(1) << "Socket migrated to: " << ip.address().ToString() << " "
           << ip.port();

  // The session should be alive and active.
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_EQ(1u, session->GetNumActiveStreams());

  // Run the message loop so that data queued in the new socket is read by the
  // packet reader.
  base::RunLoop().RunUntilIdle();

  // Verify that response headers on the migrated socket were delivered to the
  // stream.
  EXPECT_EQ(OK, stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(200, response.headers->response_code());

  stream.reset();

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv4ToIPv4) {
  // Add alternate IPv4 server address to config.
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));
  VerifyServerMigration(config, alt_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv6ToIPv6) {
  // Add a resolver rule to make initial connection to an IPv6 address.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "fe80::aebc:32ff:febb:1e33", "");
  // Add alternate IPv6 server address to config.
  IPEndPoint alt_address = IPEndPoint(
      IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 123);
  quic::QuicConfig config;
  config.SetIPv6AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));
  VerifyServerMigration(config, alt_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv6ToIPv4) {
  // Add a resolver rule to make initial connection to an IPv6 address.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "fe80::aebc:32ff:febb:1e33", "");
  // Add alternate IPv4 server address to config.
  IPEndPoint alt_address = IPEndPoint(IPAddress(1, 2, 3, 4), 123);
  quic::QuicConfig config;
  config.SetIPv4AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));
  IPEndPoint expected_address(
      ConvertIPv4ToIPv4MappedIPv6(alt_address.address()), alt_address.port());
  VerifyServerMigration(config, expected_address);
}

TEST_P(QuicStreamFactoryTest, ServerMigrationIPv4ToIPv6Fails) {
  quic_params_->allow_server_migration = true;
  Initialize();

  // Add a resolver rule to make initial connection to an IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(), "1.2.3.4",
                                            "");
  // Add alternate IPv6 server address to config.
  IPEndPoint alt_address = IPEndPoint(
      IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 123);
  quic::QuicConfig config;
  config.SetIPv6AlternateServerAddressToSend(ToQuicSocketAddress(alt_address));

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  crypto_client_stream_factory_.SetConfig(config);

  // Set up only socket data provider.
  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
    socket_data1.AddWrite(
        SYNCHRONOUS, client_maker_.MakeDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(packet_num++, true,
                                  GetNthClientInitiatedBidirectionalStreamId(0),
                                  quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // Create request and QuicHttpStream.
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  // Cause QUIC stream to be created.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org/");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, true, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  // Ensure that session is alive and active.
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  IPEndPoint actual_address;
  session->GetDefaultSocket()->GetPeerAddress(&actual_address);
  // No migration should have happened.
  IPEndPoint expected_address =
      IPEndPoint(IPAddress(1, 2, 3, 4), kDefaultServerPort);
  EXPECT_EQ(actual_address, expected_address);
  DVLOG(1) << "Socket connected to: " << actual_address.address().ToString()
           << " " << actual_address.port();
  DVLOG(1) << "Expected address: " << expected_address.address().ToString()
           << " " << expected_address.port();

  stream.reset();
  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, OnCertDBChanged) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream);
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  // Change the CA cert and verify that stream saw the event.
  factory_->OnCertDBChanged();

  EXPECT_TRUE(factory_->is_quic_known_to_work_on_current_network());
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_FALSE(HasActiveSession(host_port_pair_));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2);
  QuicChromiumClientSession* session2 = GetActiveSession(host_port_pair_);
  EXPECT_TRUE(HasActiveSession(host_port_pair_));
  EXPECT_NE(session, session2);
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session));
  EXPECT_TRUE(QuicStreamFactoryPeer::IsLiveSession(factory_.get(), session2));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, SharedCryptoConfig) {
  Initialize();

  std::vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r1_host_name("r1");
    string r2_host_name("r2");
    r1_host_name.append(cannoncial_suffixes[i]);
    r2_host_name.append(cannoncial_suffixes[i]);

    HostPortPair host_port_pair1(r1_host_name, 80);
    // Need to hold onto this through the test, to keep the
    // QuicCryptoClientConfig alive.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               NetworkIsolationKey());
    quic::QuicServerId server_id1(host_port_pair1.host(),
                                  host_port_pair1.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r1_host_name);
    cached1->SetProofValid();

    HostPortPair host_port_pair2(r2_host_name, 80);
    quic::QuicServerId server_id2(host_port_pair2.host(),
                                  host_port_pair2.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id2);
    EXPECT_EQ(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, CryptoConfigWhenProofIsInvalid) {
  Initialize();
  std::vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r3_host_name("r3");
    string r4_host_name("r4");
    r3_host_name.append(cannoncial_suffixes[i]);
    r4_host_name.append(cannoncial_suffixes[i]);

    HostPortPair host_port_pair1(r3_host_name, 80);
    // Need to hold onto this through the test, to keep the
    // QuicCryptoClientConfig alive.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               NetworkIsolationKey());
    quic::QuicServerId server_id1(host_port_pair1.host(),
                                  host_port_pair1.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id1);
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r3_host_name);
    cached1->SetProofInvalid();

    HostPortPair host_port_pair2(r4_host_name, 80);
    quic::QuicServerId server_id2(host_port_pair2.host(),
                                  host_port_pair2.port(), privacy_mode_);
    quic::QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config_handle->GetConfig()->LookupOrCreate(server_id2);
    EXPECT_NE(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->source_address_token().empty());
    EXPECT_FALSE(cached2->proof_valid());
  }
}

TEST_P(QuicStreamFactoryTest, EnableNotLoadFromDiskCache) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // If we are waiting for disk cache, we would have posted a task. Verify that
  // the CancelWaitForDataReady task hasn't been posted.
  ASSERT_EQ(0u, runner_->GetPostedTasks().size());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ReducePingTimeoutOnConnectionTimeOutOpenStreams) {
  quic_params_->reduced_ping_timeout = base::TimeDelta::FromSeconds(10);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  HostPortPair server2(kServer2HostName, kDefaultServerPort);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");
  host_resolver_->rules()->AddIPLiteralRule(server2.host(), "192.168.0.1", "");

  // Quic should use default PING timeout when no previous connection times out
  // with open stream.
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs),
            QuicStreamFactoryPeer::GetPingTimeout(factory_.get()));
  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs),
            session->connection()->ping_timeout());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(OK, stream->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                         net_log_, CompletionOnceCallback()));

  DVLOG(1)
      << "Created 1st session and initialized a stream. Now trigger timeout";
  session->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // The first connection times out with open stream, QUIC should reduce initial
  // PING time for subsequent connections.
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(10),
            QuicStreamFactoryPeer::GetPingTimeout(factory_.get()));

  // Test two-in-a-row timeouts with open streams.
  DVLOG(1) << "Create 2nd session and timeout with open stream";
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(OK,
            request2.Request(
                server2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
                NetworkIsolationKey(), false /* disable_secure_dns */,
                /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
                failed_on_default_network_callback_, callback2.callback()));
  QuicChromiumClientSession* session2 = GetActiveSession(server2);
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(10),
            session2->connection()->ping_timeout());

  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());
  EXPECT_EQ(OK,
            stream2->InitializeStream(&request_info, false, DEFAULT_PRIORITY,
                                      net_log_, CompletionOnceCallback()));
  session2->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "test",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
  // Need to spin the loop now to ensure that
  // QuicStreamFactory::OnSessionClosed() runs.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Verifies that the QUIC stream factory is initialized correctly.
TEST_P(QuicStreamFactoryTest, MaybeInitialize) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  VerifyInitialization(false /* vary_network_isolation_key */);
}

TEST_P(QuicStreamFactoryTest, MaybeInitializeWithNetworkIsolationKey) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkIsolationKey for
       // QuicSessionAliasKey to include NetworkIsolationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  VerifyInitialization(true /* vary_network_isolation_key */);
}

// Without NetworkIsolationKeys enabled for HttpServerProperties, there should
// only be one global CryptoCache.
TEST_P(QuicStreamFactoryTest, CryptoConfigCache) {
  const char kUserAgentId[] = "spoon";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);

  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://baz.test/"));
  const NetworkIsolationKey kNetworkIsolationKey3(kOrigin3, kOrigin3);

  Initialize();

  // Create a QuicCryptoClientConfigHandle for kNetworkIsolationKey1, and set
  // the user agent.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey1);
  crypto_config_handle1->GetConfig()->set_user_agent_id(kUserAgentId);
  EXPECT_EQ(kUserAgentId, crypto_config_handle1->GetConfig()->user_agent_id());

  // Create another crypto config handle using a different NetworkIsolationKey
  // while the first one is still alive should return the same config, with the
  // user agent that was just set.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey2);
  EXPECT_EQ(kUserAgentId, crypto_config_handle2->GetConfig()->user_agent_id());

  // Destroying both handles and creating a new one with yet another
  // NetworkIsolationKey should again return the same config.
  crypto_config_handle1.reset();
  crypto_config_handle2.reset();

  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle3 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey3);
  EXPECT_EQ(kUserAgentId, crypto_config_handle3->GetConfig()->user_agent_id());
}

// With different NetworkIsolationKeys enabled for HttpServerProperties, there
// should only be one global CryptoCache per NetworkIsolationKey.
TEST_P(QuicStreamFactoryTest, CryptoConfigCacheWithNetworkIsolationKey) {
  const char kUserAgentId1[] = "spoon";
  const char kUserAgentId2[] = "fork";
  const char kUserAgentId3[] = "another spoon";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkIsolationKey for
       // QuicSessionAliasKey to include NetworkIsolationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);

  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://baz.test/"));
  const NetworkIsolationKey kNetworkIsolationKey3(kOrigin3, kOrigin3);

  Initialize();

  // Create a QuicCryptoClientConfigHandle for kNetworkIsolationKey1, and set
  // the user agent.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey1);
  crypto_config_handle1->GetConfig()->set_user_agent_id(kUserAgentId1);
  EXPECT_EQ(kUserAgentId1, crypto_config_handle1->GetConfig()->user_agent_id());

  // Create another crypto config handle using a different NetworkIsolationKey
  // while the first one is still alive should return a different config.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey2);
  EXPECT_EQ("", crypto_config_handle2->GetConfig()->user_agent_id());
  crypto_config_handle2->GetConfig()->set_user_agent_id(kUserAgentId2);
  EXPECT_EQ(kUserAgentId1, crypto_config_handle1->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId2, crypto_config_handle2->GetConfig()->user_agent_id());

  // Creating handles with the same NIKs while the old handles are still alive
  // should result in getting the same CryptoConfigs.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle1_2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey1);
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle2_2 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey2);
  EXPECT_EQ(kUserAgentId1,
            crypto_config_handle1_2->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId2,
            crypto_config_handle2_2->GetConfig()->user_agent_id());

  // Destroying all handles and creating a new one with yet another
  // NetworkIsolationKey return yet another config.
  crypto_config_handle1.reset();
  crypto_config_handle2.reset();
  crypto_config_handle1_2.reset();
  crypto_config_handle2_2.reset();

  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle3 =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             kNetworkIsolationKey3);
  EXPECT_EQ("", crypto_config_handle3->GetConfig()->user_agent_id());
  crypto_config_handle3->GetConfig()->set_user_agent_id(kUserAgentId3);
  EXPECT_EQ(kUserAgentId3, crypto_config_handle3->GetConfig()->user_agent_id());
  crypto_config_handle3.reset();

  // The old CryptoConfigs should be recovered when creating handles with the
  // same NIKs as before.
  crypto_config_handle2 = QuicStreamFactoryPeer::GetCryptoConfig(
      factory_.get(), kNetworkIsolationKey2);
  crypto_config_handle1 = QuicStreamFactoryPeer::GetCryptoConfig(
      factory_.get(), kNetworkIsolationKey1);
  crypto_config_handle3 = QuicStreamFactoryPeer::GetCryptoConfig(
      factory_.get(), kNetworkIsolationKey3);
  EXPECT_EQ(kUserAgentId1, crypto_config_handle1->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId2, crypto_config_handle2->GetConfig()->user_agent_id());
  EXPECT_EQ(kUserAgentId3, crypto_config_handle3->GetConfig()->user_agent_id());
}

// Makes Verifies MRU behavior of the crypto config caches. Without
// NetworkIsolationKeys enabled, behavior is uninteresting, since there's only
// one cache, so nothing is ever evicted.
TEST_P(QuicStreamFactoryTest, CryptoConfigCacheMRUWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkIsolationKey for
       // QuicSessionAliasKey to include NetworkIsolationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});

  const int kNumSessionsToMake = kMaxRecentCryptoConfigs + 5;

  Initialize();

  // Make more entries than the maximum, setting a unique user agent for each,
  // and keeping the handles alives.
  std::vector<std::unique_ptr<QuicCryptoClientConfigHandle>>
      crypto_config_handles;
  std::vector<NetworkIsolationKey> network_isolation_keys;
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://foo%i.test/", i)));
    network_isolation_keys.push_back(NetworkIsolationKey(origin, origin));

    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_isolation_keys[i]);
    crypto_config_handle->GetConfig()->set_user_agent_id(
        base::NumberToString(i));
    crypto_config_handles.emplace_back(std::move(crypto_config_handle));
  }

  // Since all the handles are still alive, nothing should be evicted yet.
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(base::NumberToString(i),
              crypto_config_handles[i]->GetConfig()->user_agent_id());

    // A new handle for the same NIK returns the same crypto config.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_isolation_keys[i]);
    EXPECT_EQ(base::NumberToString(i),
              crypto_config_handle->GetConfig()->user_agent_id());
  }

  // Destroying the only remaining handle for a NIK results in evicting entries,
  // until there are exactly |kMaxRecentCryptoConfigs| handles.
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(base::NumberToString(i),
              crypto_config_handles[i]->GetConfig()->user_agent_id());

    crypto_config_handles[i].reset();

    // A new handle for the same NIK will return a new config, if the config was
    // evicted. Otherwise, it will return the same one.
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
        QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                               network_isolation_keys[i]);
    if (kNumSessionsToMake - i > kNumSessionsToMake) {
      EXPECT_EQ("", crypto_config_handle->GetConfig()->user_agent_id());
    } else {
      EXPECT_EQ(base::NumberToString(i),
                crypto_config_handle->GetConfig()->user_agent_id());
    }
  }
}

// Similar to above test, but uses real requests, and doesn't keep Handles
// around, so evictions happen immediately.
TEST_P(QuicStreamFactoryTest,
       CryptoConfigCacheMRUWithRealRequestsAndWithNetworkIsolationKey) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  const int kNumSessionsToMake = kMaxRecentCryptoConfigs + 5;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       // Need to partition connections by NetworkIsolationKey for
       // QuicSessionAliasKey to include NetworkIsolationKeys.
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  std::vector<NetworkIsolationKey> network_isolation_keys;
  for (int i = 0; i < kNumSessionsToMake; ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://foo%i.test/", i)));
    network_isolation_keys.push_back(NetworkIsolationKey(origin, origin));
  }

  const quic::QuicServerId kQuicServerId(
      kDefaultServerHostName, kDefaultServerPort, PRIVACY_MODE_DISABLED);

  quic_params_->max_server_configs_stored_in_properties = 1;
  quic_params_->idle_connection_timeout = base::TimeDelta::FromSeconds(500);
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(500, config->IdleNetworkTimeout().ToSeconds());
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();

  for (int i = 0; i < kNumSessionsToMake; ++i) {
    SCOPED_TRACE(i);
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

    QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), runner_.get());

    const AlternativeService alternative_service1(
        kProtoQUIC, host_port_pair_.host(), host_port_pair_.port());
    AlternativeServiceInfoVector alternative_service_info_vector;
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    alternative_service_info_vector.push_back(
        AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
            alternative_service1, expiration, {version_}));
    http_server_properties_->SetAlternativeServices(
        url::SchemeHostPort(url_), network_isolation_keys[i],
        alternative_service_info_vector);

    http_server_properties_->SetMaxServerConfigsStoredInProperties(
        kDefaultMaxQuicServerEntries);

    std::unique_ptr<QuicServerInfo> quic_server_info =
        std::make_unique<PropertiesBasedQuicServerInfo>(
            kQuicServerId, network_isolation_keys[i],
            http_server_properties_.get());

    // Update quic_server_info's server_config and persist it.
    QuicServerInfo::State* state = quic_server_info->mutable_state();
    // Minimum SCFG that passes config validation checks.
    const char scfg[] = {// SCFG
                         0x53, 0x43, 0x46, 0x47,
                         // num entries
                         0x01, 0x00,
                         // padding
                         0x00, 0x00,
                         // EXPY
                         0x45, 0x58, 0x50, 0x59,
                         // EXPY end offset
                         0x08, 0x00, 0x00, 0x00,
                         // Value
                         '1', '2', '3', '4', '5', '6', '7', '8'};

    // Create temporary strings because Persist() clears string data in |state|.
    string server_config(reinterpret_cast<const char*>(&scfg), sizeof(scfg));
    string source_address_token("test_source_address_token");
    string cert_sct("test_cert_sct");
    string chlo_hash("test_chlo_hash");
    string signature("test_signature");
    string test_cert("test_cert");
    std::vector<string> certs;
    certs.push_back(test_cert);
    state->server_config = server_config;
    state->source_address_token = source_address_token;
    state->cert_sct = cert_sct;
    state->chlo_hash = chlo_hash;
    state->server_config_sig = signature;
    state->certs = certs;

    quic_server_info->Persist();

    // Create a session and verify that the cached state is loaded.
    MockQuicData socket_data(version_);
    socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
    if (VersionUsesHttp3(version_.transport_version))
      socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
    // For the close socket message.
    socket_data.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);
    socket_data.AddSocketDataToFactory(socket_factory_.get());
    client_maker_.Reset();

    QuicStreamRequest request(factory_.get());
    int rv = request.Request(
        HostPortPair(kDefaultServerHostName, kDefaultServerPort), version_,
        privacy_mode_, DEFAULT_PRIORITY, SocketTag(), network_isolation_keys[i],
        false /* disable_secure_dns */,
        /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
        failed_on_default_network_callback_, callback_.callback());
    EXPECT_THAT(callback_.GetResult(rv), IsOk());

    // While the session is still alive, there should be
    // kMaxRecentCryptoConfigs+1 CryptoConfigCaches alive, since active configs
    // don't count towards the limit.
    for (int j = 0; j < kNumSessionsToMake; ++j) {
      SCOPED_TRACE(j);
      EXPECT_EQ(i - (kMaxRecentCryptoConfigs + 1) < j && j <= i,
                !QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
                    factory_.get(), kQuicServerId, network_isolation_keys[j]));
    }

    // Close the sessions, which should cause its CryptoConfigCache to be moved
    // to the MRU cache, potentially evicting the oldest entry..
    factory_->CloseAllSessions(ERR_FAILED, quic::QUIC_PEER_GOING_AWAY);

    // There should now be at most kMaxRecentCryptoConfigs live
    // CryptoConfigCaches
    for (int j = 0; j < kNumSessionsToMake; ++j) {
      SCOPED_TRACE(j);
      EXPECT_EQ(i - kMaxRecentCryptoConfigs < j && j <= i,
                !QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
                    factory_.get(), kQuicServerId, network_isolation_keys[j]));
    }
  }
}

TEST_P(QuicStreamFactoryTest, YieldAfterPackets) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  QuicStreamFactoryPeer::SetYieldAfterPackets(factory_.get(), 0);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ConstructClientConnectionClosePacket(1));
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddRead(ASYNC, OK);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicChromiumPacketReader::StartReading
  // posts a task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_chromium_packet_reader.cc",
                                       "StartReading");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Call run_loop so that QuicChromiumPacketReader::OnReadComplete() gets
  // called.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicChromiumPacketReader::StartReading() has posted only one task and
  // yielded the read.
  EXPECT_EQ(1u, observer.executed_count());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_FALSE(stream.get());  // Session is already closed.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, YieldAfterDuration) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  QuicStreamFactoryPeer::SetYieldAfterDuration(
      factory_.get(), quic::QuicTime::Delta::FromMilliseconds(-1));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ConstructClientConnectionClosePacket(1));
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddRead(ASYNC, OK);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            "192.168.0.1", "");

  // Set up the TaskObserver to verify QuicChromiumPacketReader::StartReading
  // posts a task.
  // TODO(rtenneti): Change SpdySessionTestTaskObserver to NetTestTaskObserver??
  SpdySessionTestTaskObserver observer("quic_chromium_packet_reader.cc",
                                       "StartReading");

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      OK,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Call run_loop so that QuicChromiumPacketReader::OnReadComplete() gets
  // called.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Verify task that the observer's executed_count is 1, which indicates
  // QuicChromiumPacketReader::StartReading() has posted only one task and
  // yielded the read.
  EXPECT_EQ(1u, observer.executed_count());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_FALSE(stream.get());  // Session is already closed.
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ServerPushSessionAffinity) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));

  string url = "https://www.example.org/";

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  quic::QuicClientPromisedInfo promised(
      session, GetNthServerInitiatedUnidirectionalStreamId(0), kDefaultUrl);
  (*QuicStreamFactoryPeer::GetPushPromiseIndex(factory_.get())
        ->promised_by_url())[kDefaultUrl] = &promised;

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_EQ(1, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));
}

TEST_P(QuicStreamFactoryTest, ServerPushPrivacyModeMismatch) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data1.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRstPacket(
          packet_num++, true, GetNthServerInitiatedUnidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED));
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));

  string url = "https://www.example.org/";
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  quic::QuicClientPromisedInfo promised(
      session, GetNthServerInitiatedUnidirectionalStreamId(0), kDefaultUrl);

  quic::QuicClientPushPromiseIndex* index =
      QuicStreamFactoryPeer::GetPushPromiseIndex(factory_.get());

  (*index->promised_by_url())[kDefaultUrl] = &promised;
  EXPECT_EQ(index->GetPromised(kDefaultUrl), &promised);

  // Doing the request should not use the push stream, but rather
  // cancel it because the privacy modes do not match.
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, PRIVACY_MODE_ENABLED, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_EQ(0, QuicStreamFactoryPeer::GetNumPushStreamsCreated(factory_.get()));
  EXPECT_EQ(index->GetPromised(kDefaultUrl), nullptr);

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// Pool to existing session with matching quic::QuicServerId
// even if destination is different.
TEST_P(QuicStreamFactoryTest, PoolByOrigin) {
  Initialize();

  HostPortPair destination1("first.example.com", 443);
  HostPortPair destination2("second.example.com", 443);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination1, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(host_port_pair_));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          destination2, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_TRUE(session1->SharesSameSession(*session2));
  EXPECT_EQ(quic::QuicServerId(host_port_pair_.host(), host_port_pair_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

class QuicStreamFactoryWithDestinationTest
    : public QuicStreamFactoryTestBase,
      public ::testing::TestWithParam<PoolingTestParams> {
 protected:
  QuicStreamFactoryWithDestinationTest()
      : QuicStreamFactoryTestBase(
            GetParam().version,
            GetParam().client_headers_include_h2_stream_dependency),
        destination_type_(GetParam().destination_type),
        hanging_read_(SYNCHRONOUS, ERR_IO_PENDING, 0) {}

  HostPortPair GetDestination() {
    switch (destination_type_) {
      case SAME_AS_FIRST:
        return origin1_;
      case SAME_AS_SECOND:
        return origin2_;
      case DIFFERENT:
        return HostPortPair(kDifferentHostname, 443);
      default:
        NOTREACHED();
        return HostPortPair();
    }
  }

  void AddHangingSocketData() {
    std::unique_ptr<SequencedSocketData> sequenced_socket_data(
        new SequencedSocketData(base::make_span(&hanging_read_, 1),
                                base::span<MockWrite>()));
    socket_factory_->AddSocketDataProvider(sequenced_socket_data.get());
    sequenced_socket_data_vector_.push_back(std::move(sequenced_socket_data));
  }

  bool AllDataConsumed() {
    for (const auto& socket_data_ptr : sequenced_socket_data_vector_) {
      if (!socket_data_ptr->AllReadDataConsumed() ||
          !socket_data_ptr->AllWriteDataConsumed()) {
        return false;
      }
    }
    return true;
  }

  DestinationType destination_type_;
  HostPortPair origin1_;
  HostPortPair origin2_;
  MockRead hanging_read_;
  std::vector<std::unique_ptr<SequencedSocketData>>
      sequenced_socket_data_vector_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicStreamFactoryWithDestinationTest,
                         ::testing::ValuesIn(GetPoolingTestParams()),
                         ::testing::PrintToStringParamName());

// A single QUIC request fails because the certificate does not match the origin
// hostname, regardless of whether it matches the alternative service hostname.
TEST_P(QuicStreamFactoryWithDestinationTest, InvalidCertificate) {
  if (destination_type_ == DIFFERENT)
    return;

  Initialize();

  GURL url("https://mail.example.com/");
  origin1_ = HostPortPair::FromURL(url);

  // Not used for requests, but this provides a test case where the certificate
  // is valid for the hostname of the alternative service.
  origin2_ = HostPortPair("mail.example.org", 433);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_FALSE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  AddHangingSocketData();

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_QUIC_HANDSHAKE_FAILED));

  EXPECT_TRUE(AllDataConsumed());
}

// QuicStreamRequest is pooled based on |destination| if certificate matches.
TEST_P(QuicStreamFactoryWithDestinationTest, SharedCertificate) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  // Second request returns synchronously because it pools to existing session.
  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      OK,
      request2.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_TRUE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(origin1_.host(), origin1_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// QuicStreamRequest is not pooled if PrivacyMode differs.
TEST_P(QuicStreamFactoryWithDestinationTest, DifferentPrivacyMode) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          destination, version_, PRIVACY_MODE_ENABLED, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  EXPECT_EQ(OK, callback2.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // |request2| does not pool to the first session, because PrivacyMode does not
  // match.  Instead, another session is opened to the same destination, but
  // with a different quic::QuicServerId.
  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_FALSE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(origin1_.host(), origin1_.port(), false),
            session1->server_id());
  EXPECT_EQ(quic::QuicServerId(origin2_.host(), origin2_.port(), true),
            session2->server_id());

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// QuicStreamRequest is not pooled if the disable_secure_dns field differs.
TEST_P(QuicStreamFactoryWithDestinationTest, DifferentDisableSecureDns) {
  Initialize();

  GURL url1("https://www.example.org/");
  GURL url2("https://mail.example.org/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_.host()));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          destination, version_, PRIVACY_MODE_DISABLED, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), true /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  EXPECT_EQ(OK, callback2.WaitForResult());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // |request2| does not pool to the first session, because |disable_secure_dns|
  // does not match.
  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_FALSE(session1->SharesSameSession(*session2));

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// QuicStreamRequest is not pooled if certificate does not match its origin.
TEST_P(QuicStreamFactoryWithDestinationTest, DisjointCertificate) {
  Initialize();

  GURL url1("https://news.example.org/");
  GURL url2("https://mail.example.com/");
  origin1_ = HostPortPair::FromURL(url1);
  origin2_ = HostPortPair::FromURL(url2);

  HostPortPair destination = GetDestination();

  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert1->VerifyNameMatch(origin1_.host()));
  ASSERT_FALSE(cert1->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert1->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert1;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert2->VerifyNameMatch(origin2_.host()));
  ASSERT_FALSE(cert2->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert2;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  MockQuicData socket_data1(version_);
  socket_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request1(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request1.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url1, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream1 = CreateStream(&request1);
  EXPECT_TRUE(stream1.get());
  EXPECT_TRUE(HasActiveSession(origin1_));

  TestCompletionCallback callback2;
  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          destination, version_, privacy_mode_, DEFAULT_PRIORITY, SocketTag(),
          NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url2, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback2.callback()));
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&request2);
  EXPECT_TRUE(stream2.get());

  // |request2| does not pool to the first session, because the certificate does
  // not match.  Instead, another session is opened to the same destination, but
  // with a different quic::QuicServerId.
  QuicChromiumClientSession::Handle* session1 =
      QuicHttpStreamPeer::GetSessionHandle(stream1.get());
  QuicChromiumClientSession::Handle* session2 =
      QuicHttpStreamPeer::GetSessionHandle(stream2.get());
  EXPECT_FALSE(session1->SharesSameSession(*session2));

  EXPECT_EQ(quic::QuicServerId(origin1_.host(), origin1_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session1->server_id());
  EXPECT_EQ(quic::QuicServerId(origin2_.host(), origin2_.port(),
                               privacy_mode_ == PRIVACY_MODE_ENABLED),
            session2->server_id());

  EXPECT_TRUE(socket_data1.AllReadDataConsumed());
  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
}

// This test verifies that QuicStreamFactory::ClearCachedStatesInCryptoConfig
// correctly transform an origin filter to a ServerIdFilter. Whether the
// deletion itself works correctly is tested in QuicCryptoClientConfigTest.
TEST_P(QuicStreamFactoryTest, ClearCachedStatesInCryptoConfig) {
  Initialize();
  // Need to hold onto this through the test, to keep the QuicCryptoClientConfig
  // alive.
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
      QuicStreamFactoryPeer::GetCryptoConfig(factory_.get(),
                                             NetworkIsolationKey());

  struct TestCase {
    TestCase(const std::string& host,
             int port,
             PrivacyMode privacy_mode,
             quic::QuicCryptoClientConfig* crypto_config)
        : server_id(host, port, privacy_mode),
          state(crypto_config->LookupOrCreate(server_id)) {
      std::vector<string> certs(1);
      certs[0] = "cert";
      state->SetProof(certs, "cert_sct", "chlo_hash", "signature");
      state->set_source_address_token("TOKEN");
      state->SetProofValid();

      EXPECT_FALSE(state->certs().empty());
    }

    quic::QuicServerId server_id;
    quic::QuicCryptoClientConfig::CachedState* state;
  } test_cases[] = {TestCase("www.google.com", 443, privacy_mode_,
                             crypto_config_handle->GetConfig()),
                    TestCase("www.example.com", 443, privacy_mode_,
                             crypto_config_handle->GetConfig()),
                    TestCase("www.example.com", 4433, privacy_mode_,
                             crypto_config_handle->GetConfig())};

  // Clear cached states for the origin https://www.example.com:4433.
  GURL origin("https://www.example.com:4433");
  factory_->ClearCachedStatesInCryptoConfig(base::BindRepeating(
      static_cast<bool (*)(const GURL&, const GURL&)>(::operator==), origin));
  EXPECT_FALSE(test_cases[0].state->certs().empty());
  EXPECT_FALSE(test_cases[1].state->certs().empty());
  EXPECT_TRUE(test_cases[2].state->certs().empty());

  // Clear all cached states.
  factory_->ClearCachedStatesInCryptoConfig(
      base::RepeatingCallback<bool(const GURL&)>());
  EXPECT_TRUE(test_cases[0].state->certs().empty());
  EXPECT_TRUE(test_cases[1].state->certs().empty());
  EXPECT_TRUE(test_cases[2].state->certs().empty());
}

// Passes connection options and client connection options to QuicStreamFactory,
// then checks that its internal quic::QuicConfig is correct.
TEST_P(QuicStreamFactoryTest, ConfigConnectionOptions) {
  quic_params_->connection_options.push_back(quic::kTIME);
  quic_params_->connection_options.push_back(quic::kTBBR);
  quic_params_->connection_options.push_back(quic::kREJ);

  quic_params_->client_connection_options.push_back(quic::kTBBR);
  quic_params_->client_connection_options.push_back(quic::k1RTT);

  Initialize();

  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(quic_params_->connection_options, config->SendConnectionOptions());
  EXPECT_TRUE(config->HasClientRequestedIndependentOption(
      quic::kTBBR, quic::Perspective::IS_CLIENT));
  EXPECT_TRUE(config->HasClientRequestedIndependentOption(
      quic::k1RTT, quic::Perspective::IS_CLIENT));
}

// Verifies that the host resolver uses the request priority passed to
// QuicStreamRequest::Request().
TEST_P(QuicStreamFactoryTest, HostResolverUsesRequestPriority) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, MAXIMUM_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->last_request_priority());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, HostResolverRequestReprioritizedOnSetPriority) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, MAXIMUM_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->last_request_priority());
  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->request_priority(1));

  QuicStreamRequest request2(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request2.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url2_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->last_request_priority());
  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->request_priority(2));

  request.SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, host_resolver_->request_priority(1));
  EXPECT_EQ(DEFAULT_PRIORITY, host_resolver_->request_priority(2));
}

// Verifies that the host resolver uses the disable secure DNS setting and
// NetworkIsolationKey passed to QuicStreamRequest::Request().
TEST_P(QuicStreamFactoryTest, HostResolverUsesParams) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey(kOrigin1, kOrigin1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionConnectionsByNetworkIsolationKey,
       features::kSplitHostCacheByNetworkIsolationKey},
      // disabled_features
      {});

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), kNetworkIsolationKey, true /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  ASSERT_TRUE(host_resolver_->last_secure_dns_mode_override().has_value());
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF,
            host_resolver_->last_secure_dns_mode_override().value());
  ASSERT_TRUE(host_resolver_->last_request_network_isolation_key().has_value());
  EXPECT_EQ(kNetworkIsolationKey,
            host_resolver_->last_request_network_isolation_key().value());

  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Passes |quic_max_time_before_crypto_handshake| and
// |quic_max_idle_time_before_crypto_handshake| to QuicStreamFactory,
// checks that its internal quic::QuicConfig is correct.
TEST_P(QuicStreamFactoryTest, ConfigMaxTimeBeforeCryptoHandshake) {
  quic_params_->max_time_before_crypto_handshake =
      base::TimeDelta::FromSeconds(11);
  quic_params_->max_idle_time_before_crypto_handshake =
      base::TimeDelta::FromSeconds(13);
  Initialize();

  const quic::QuicConfig* config =
      QuicStreamFactoryPeer::GetConfig(factory_.get());
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(11),
            config->max_time_before_crypto_handshake());
  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(13),
            config->max_idle_time_before_crypto_handshake());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds asynchronously, then crypto handshake fails synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackAsyncSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  // |host_resolver_| has not finished host resolution at this point, so
  // |host_resolution_callback| should not have a result.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  // Allow |host_resolver_| to finish host resolution.
  // Since the request fails immediately after host resolution (getting
  // ERR_FAILED from socket reads/writes), |host_resolution_callback| should be
  // called with ERR_QUIC_PROTOCOL_ERROR since that's the next result in
  // forming the connection.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolution_callback.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, host_resolution_callback.WaitForResult());

  // Calling WaitForHostResolution() a second time should return
  // false since host resolution has finished already.
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds asynchronously, then crypto handshake fails asynchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackAsyncAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  factory_->set_is_quic_known_to_work_on_current_network(false);

  MockQuicData socket_data(version_);
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_FAILED);
  socket_data.AddWrite(ASYNC, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  // |host_resolver_| has not finished host resolution at this point, so
  // |host_resolution_callback| should not have a result.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  // Allow |host_resolver_| to finish host resolution. Since crypto handshake
  // will hang after host resolution, |host_resolution_callback| should run with
  // ERR_IO_PENDING since that's the next result in forming the connection.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolution_callback.have_result());
  EXPECT_EQ(ERR_IO_PENDING, host_resolution_callback.WaitForResult());

  // Calling WaitForHostResolution() a second time should return
  // false since host resolution has finished already.
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  EXPECT_FALSE(callback_.have_result());
  socket_data.GetSequencedSocketData()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds synchronously, then crypto handshake fails synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackSyncSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_synchronous_mode(true);

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_QUIC_PROTOCOL_ERROR,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has finished already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());
  EXPECT_FALSE(callback_.have_result());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution
// succeeds synchronously, then crypto handshake fails asynchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackSyncAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Host resolution will succeed synchronously, but Request() as a whole
  // will fail asynchronously.
  host_resolver_->set_synchronous_mode(true);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  factory_->set_is_quic_known_to_work_on_current_network(false);

  MockQuicData socket_data(version_);
  socket_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  socket_data.AddRead(ASYNC, ERR_FAILED);
  socket_data.AddWrite(ASYNC, ERR_FAILED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has finished already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  EXPECT_FALSE(callback_.have_result());
  socket_data.GetSequencedSocketData()->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback_.WaitForResult());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution fails
// synchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackFailSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Host resolution will fail synchronously.
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());
  host_resolver_->set_synchronous_mode(true);

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_NAME_NOT_RESOLVED,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // WaitForHostResolution() should return false since host
  // resolution has failed already.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());
}

// Verify ResultAfterHostResolutionCallback behavior when host resolution fails
// asynchronously.
TEST_P(QuicStreamFactoryTest, ResultAfterHostResolutionCallbackFailAsync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  // Allow |host_resolver_| to fail host resolution. |host_resolution_callback|
  // Should run with ERR_NAME_NOT_RESOLVED since that's the error host
  // resolution failed with.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolution_callback.have_result());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, host_resolution_callback.WaitForResult());

  EXPECT_TRUE(callback_.have_result());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());
}

// With dns race experiment turned on, and DNS resolve succeeds synchronously,
// the final connection is established through the resolved DNS. No racing
// connection.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceAndHostResolutionSync) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for synchronous return.
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_THAT(
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()),
      IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, DNS resolve returns async, no matching cache in
// host resolver, connection should be successful and through resolved DNS. No
// racing connection.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceAndHostResolutionAsync) {
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  // Cause the host resolution to return.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(host_resolution_callback.WaitForResult(), IsOk());
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());
  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, DNS resolve returns async, stale dns used,
// connects synchrounously, and then the resolved DNS matches.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveAsyncStaleMatch) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check that the racing job is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Resolve dns and return.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(session->peer_address().host().ToString(),
            kCachedIPAddress.ToString());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale dns used, connect
// async, and then the result matches.
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceHostResolveAsyncConnectAsyncStaleMatch) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Send Crypto handshake so connect will call back.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  base::RunLoop().RunUntilIdle();

  // Check that the racing job is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Resolve dns and call back, make sure job finishes.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(session->peer_address().host().ToString(),
            kCachedIPAddress.ToString());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale dns used, dns resolve
// return, then connection finishes and matches with the result.
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceHostResolveAsyncStaleMatchConnectAsync) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Finish dns async, check we still need to wait for stale connection async.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_.have_result());

  // Finish stale connection async, and the stale connection should pass dns
  // validation.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(session->peer_address().host().ToString(),
            kCachedIPAddress.ToString());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connects
// sync, but dns no match
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceHostResolveAsyncStaleSyncNoMatch) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  // Socket for the stale connection which will invoke connection closure.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeConnectionClosePacket(
                         packet_num++, true,
                         quic::QUIC_STALE_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket for the new connection.
  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check the stale connection is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish dns resolution and check the job has finished.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connects
// async, finishes before dns, but no match
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleAsyncResolveAsyncNoMatch) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // TODO(fayang): 0-rtt is not supported in IETF QUIC yet. Fix it.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolvercache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeConnectionClosePacket(
                         packet_num++, true,
                         quic::QUIC_STALE_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  }
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Finish the stale connection.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish host resolution and check the job is done.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connects
// async, dns finishes first, but no match
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceResolveAsyncStaleAsyncNoMatch) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // TODO(fayang): 0-rtt is not supported in IETF QUIC yet. Fix it.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeConnectionClosePacket(
                         packet_number++, true,
                         quic::QUIC_STALE_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  // Finish dns resolution, but need to wait for stale connection.
  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve returns error sync, same behavior
// as experiment is not on
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveError) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set synchronous failure in resolver.
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  MockQuicData quic_data(version_);
  quic_data.AddSocketDataToFactory(socket_factory_.get());
  QuicStreamRequest request(factory_.get());

  EXPECT_EQ(
      ERR_NAME_NOT_RESOLVED,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
}

// With dns race experiment on, no cache available, dns resolve returns error
// async
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveAsyncError) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set asynchronous failure in resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  MockQuicData quic_data(version_);
  quic_data.AddSocketDataToFactory(socket_factory_.get());
  QuicStreamRequest request(factory_.get());

  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Resolve and expect result that shows the resolution error.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
}

// With dns race experiment on, dns resolve async, staled used and connects
// sync, dns returns error and no connection is established.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleSyncHostResolveError) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set asynchronous failure in resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  // Set up an address in the stale cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  // Socket for the stale connection which is supposed to disconnect.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeConnectionClosePacket(
                         packet_num++, true,
                         quic::QUIC_STALE_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check that the stale connection is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish host resolution.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connection
// return error, then dns matches.
// This serves as a regression test for crbug.com/956374.
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleErrorDNSMatches) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in host resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kCachedIPAddress.ToString(), "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  // Simulate synchronous connect failure.
  MockQuicData quic_data(version_);
  quic_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData quic_data2(version_);
  quic_data2.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  EXPECT_FALSE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));
}

// With dns race experiment on, dns resolve async, stale used and connection
// returns error, dns no match, new connection is established
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleErrorDNSNoMatch) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in host resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  // Add failure for the stale connection.
  MockQuicData quic_data(version_);
  quic_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check that the stale connection fails.
  EXPECT_FALSE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Finish host resolution and check the job finishes ok.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async, stale used and connection
// returns error, dns no match, new connection error
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceStaleErrorDNSNoMatchError) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in host resolver asynchronously.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  // Add failure for stale connection.
  MockQuicData quic_data(version_);
  quic_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Add failure for resolved dns connection.
  MockQuicData quic_data2(version_);
  quic_data2.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check the stale connection fails.
  EXPECT_FALSE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // Check the resolved dns connection fails.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));
}

// With dns race experiment on, dns resolve async and stale connect async, dns
// resolve returns error and then preconnect finishes
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceResolveAsyncErrorStaleAsync) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // TODO(fayang): 0-rtt is not supported in IETF QUIC yet. Fix it.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Add asynchronous failure in host resolver.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  // Set up an address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  // Socket data for stale connection which is supposed to disconnect.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeConnectionClosePacket(
                         packet_number++, true,
                         quic::QUIC_STALE_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // host resolution returned but stale connection hasn't finished yet.
  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With dns race experiment on, dns resolve async and stale connect async, dns
// resolve returns error and then preconnect fails.
TEST_P(QuicStreamFactoryTest,
       ResultAfterDNSRaceResolveAsyncErrorStaleAsyncError) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // TODO(fayang): 0-rtt is not supported in IETF QUIC yet. Fix it.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Add asynchronous failure to host resolver.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddSimulatedFailure(host_port_pair_.host());

  // Set up an address in stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeConnectionClosePacket(
                         packet_number++, true,
                         quic::QUIC_STALE_CONNECTION_CANCELLED, "net error"));
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Host Resolution returns failure but stale connection hasn't finished.
  host_resolver_->ResolveAllPending();

  // Check that the final error is on resolution failure.
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
}

// With dns race experiment on, test that host resolution callback behaves
// normal as experiment is not on
TEST_P(QuicStreamFactoryTest, ResultAfterDNSRaceHostResolveAsync) {
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check that expect_on_host_resolution_ is properlly set.
  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(
      request.WaitForHostResolution(host_resolution_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_resolution_callback.have_result());

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(host_resolution_callback.WaitForResult(), IsOk());

  // Check that expect_on_host_resolution_ is flipped back.
  EXPECT_FALSE(
      request.WaitForHostResolution(host_resolution_callback.callback()));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// With stale dns and migration before handshake experiment on, migration failed
// after handshake confirmed, and then fresh resolve returns.
TEST_P(QuicStreamFactoryTest, StaleNetworkFailedAfterHandshake) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();

  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up the same address in the stale resolver cache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  // Socket for the new connection.
  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check that the racing job is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // By disconnecting the network, the stale session will be killed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);

  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

// With stale dns experiment on,  the stale session is killed while waiting for
// handshake
TEST_P(QuicStreamFactoryTest, StaleNetworkFailedBeforeHandshake) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 &&
      version_.HasIetfQuicFrames()) {
    // 0-rtt is not supported in IETF QUIC yet.
    return;
  }
  quic_params_->race_stale_dns_on_connection = true;
  host_resolver_ = std::make_unique<MockCachingHostResolver>();
  InitializeConnectionMigrationV2Test(
      {kDefaultNetworkForTests, kNewNetworkForTests});
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Set an address in resolver for asynchronous return.
  host_resolver_->set_ondemand_mode(true);
  factory_->set_is_quic_known_to_work_on_current_network(false);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  host_resolver_->rules()->AddIPLiteralRule(host_port_pair_.host(),
                                            kNonCachedIPAddress, "");

  // Set up a different address in the stale resolvercache.
  HostCache::Key key(host_port_pair_.host(), DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  HostCache::Entry entry(OK,
                         AddressList::CreateFromIPAddress(kCachedIPAddress, 0),
                         HostCache::Entry::SOURCE_DNS);
  base::TimeDelta zero;
  HostCache* cache = host_resolver_->GetHostCache();
  cache->Set(key, entry, base::TimeTicks::Now(), zero);
  // Expire the cache
  cache->Invalidate();

  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  quic_data2.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));

  // Check that the racing job is running.
  EXPECT_TRUE(HasLiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // By disconnecting the network, the stale session will be killed.
  scoped_mock_network_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkDisconnected(kDefaultNetworkForTests);

  host_resolver_->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  // Make sure the fresh session is established.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(session->peer_address().host().ToString(), kNonCachedIPAddress);

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicStreamFactoryTest, ConfigInitialRttForHandshake) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // IETF QUIC uses a different handshake timeout management system.
    return;
  }
  constexpr base::TimeDelta kInitialRtt =
      base::TimeDelta::FromMilliseconds(400);
  quic_params_->initial_rtt_for_handshake = kInitialRtt;
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  Initialize();
  factory_->set_is_quic_known_to_work_on_current_network(false);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Using a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  QuicStreamFactoryPeer::SetTaskRunner(factory_.get(), task_runner.get());
  QuicStreamFactoryPeer::SetAlarmFactory(
      factory_.get(), std::make_unique<QuicChromiumAlarmFactory>(
                          task_runner.get(), context_.clock()));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(1));
  socket_data.AddWrite(ASYNC, client_maker_.MakeDummyCHLOPacket(2));
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(3));
  }

  socket_data.AddSocketDataToFactory(socket_factory_.get());

  QuicStreamRequest request(factory_.get());
  EXPECT_EQ(
      ERR_IO_PENDING,
      request.Request(
          host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY,
          SocketTag(), NetworkIsolationKey(), false /* disable_secure_dns */,
          /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
          failed_on_default_network_callback_, callback_.callback()));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(HasActiveSession(host_port_pair_));
  EXPECT_TRUE(HasActiveJob(host_port_pair_, privacy_mode_));

  // The pending task is scheduled for handshake timeout retransmission,
  // which is 2 * 400ms with crypto frames and 1.5 * 400ms otherwise.
  base::TimeDelta handshake_timeout =
      QuicVersionUsesCryptoFrames(version_.transport_version)
          ? 2 * kInitialRtt
          : 1.5 * kInitialRtt;
  EXPECT_EQ(handshake_timeout, task_runner->NextPendingTaskDelay());

  // The alarm factory dependes on |clock_|, so clock is advanced to trigger
  // retransmission alarm.
  context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(
      handshake_timeout.InMilliseconds()));
  task_runner->FastForwardBy(handshake_timeout);

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  QuicChromiumClientSession* session = GetActiveSession(host_port_pair_);
  EXPECT_EQ(400000u, session->config()->GetInitialRoundTripTimeUsToSend());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Test that QuicStreamRequests with similar and different tags results in
// reused and unique QUIC streams using appropriately tagged sockets.
TEST_P(QuicStreamFactoryTest, Tag) {
  MockTaggingClientSocketFactory* socket_factory =
      new MockTaggingClientSocketFactory();
  socket_factory_.reset(socket_factory);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Prepare to establish two QUIC sessions.
  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  if (VersionUsesHttp3(version_.transport_version))
    socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

#if defined(OS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
#else
  // On non-Android platforms we can only use the default constructor.
  SocketTag tag1, tag2;
#endif

  // Request a stream with |tag1|.
  QuicStreamRequest request1(factory_.get());
  int rv = request1.Request(
      host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, tag1,
      NetworkIsolationKey(), false /* disable_secure_dns */,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_THAT(callback_.GetResult(rv), IsOk());
  EXPECT_EQ(socket_factory->GetLastProducedUDPSocket()->tag(), tag1);
  EXPECT_TRUE(socket_factory->GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());
  std::unique_ptr<QuicChromiumClientSession::Handle> stream1 =
      request1.ReleaseSessionHandle();
  EXPECT_TRUE(stream1);
  EXPECT_TRUE(stream1->IsConnected());

  // Request a stream with |tag1| and verify underlying session is reused.
  QuicStreamRequest request2(factory_.get());
  rv = request2.Request(
      host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, tag1,
      NetworkIsolationKey(), false /* disable_secure_dns */,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_THAT(callback_.GetResult(rv), IsOk());
  std::unique_ptr<QuicChromiumClientSession::Handle> stream2 =
      request2.ReleaseSessionHandle();
  EXPECT_TRUE(stream2);
  EXPECT_TRUE(stream2->IsConnected());
  EXPECT_TRUE(stream2->SharesSameSession(*stream1));

  // Request a stream with |tag2| and verify a new session is created.
  QuicStreamRequest request3(factory_.get());
  rv = request3.Request(
      host_port_pair_, version_, privacy_mode_, DEFAULT_PRIORITY, tag2,
      NetworkIsolationKey(), false /* disable_secure_dns */,
      /*cert_verify_flags=*/0, url_, net_log_, &net_error_details_,
      failed_on_default_network_callback_, callback_.callback());
  EXPECT_THAT(callback_.GetResult(rv), IsOk());
  EXPECT_EQ(socket_factory->GetLastProducedUDPSocket()->tag(), tag2);
  EXPECT_TRUE(socket_factory->GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());
  std::unique_ptr<QuicChromiumClientSession::Handle> stream3 =
      request3.ReleaseSessionHandle();
  EXPECT_TRUE(stream3);
  EXPECT_TRUE(stream3->IsConnected());
#if defined(OS_ANDROID)
  EXPECT_FALSE(stream3->SharesSameSession(*stream1));
#else
  // Same tag should reuse session.
  EXPECT_TRUE(stream3->SharesSameSession(*stream1));
#endif
}

}  // namespace test
}  // namespace net

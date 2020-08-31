// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_proxy_delegate.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_stream_factory_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_str_cat.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/spdy/core/spdy_frame_builder.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::ElementsAre;
using ::testing::Key;

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

static const char kQuicAlternativeServiceHeader[] =
    "Alt-Svc: quic=\":443\"\r\n\r\n";
static const char kQuicAlternativeServiceWithProbabilityHeader[] =
    "Alt-Svc: quic=\":443\";p=\".5\"\r\n\r\n";
static const char kQuicAlternativeServiceDifferentPortHeader[] =
    "Alt-Svc: quic=\":137\"\r\n\r\n";

const char kDefaultServerHostName[] = "mail.example.org";
const char kDifferentHostname[] = "different.example.com";

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

// Run QuicNetworkTransactionWithDestinationTest instances with all value
// combinations of version and destination_type.
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

std::string GenerateQuicAltSvcHeader(
    const quic::ParsedQuicVersionVector& versions) {
  std::string altsvc_header = "Alt-Svc: ";
  std::string version_string;
  bool first_version = true;
  for (const auto& version : versions) {
    if (first_version) {
      first_version = false;
    } else {
      altsvc_header.append(", ");
    }
    altsvc_header.append(quic::AlpnForVersion(version));
    altsvc_header.append("=\":443\"");
    if (version.SupportsGoogleAltSvcFormat()) {
      if (!version_string.empty()) {
        version_string.append(",");
      }
      version_string.append(base::NumberToString(version.transport_version));
    }
  }
  if (!version_string.empty()) {
    altsvc_header.append(", quic=\":443\"; v=\"" + version_string + "\"");
  }
  altsvc_header.append("\r\n");

  return altsvc_header;
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  quic::ParsedQuicVersionVector all_supported_versions =
      quic::AllSupportedVersions();
  for (const quic::ParsedQuicVersion& version : all_supported_versions) {
    params.push_back(TestParams{version, false});
    params.push_back(TestParams{version, true});
  }
  return params;
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

class TestSocketPerformanceWatcher : public SocketPerformanceWatcher {
 public:
  TestSocketPerformanceWatcher(bool* should_notify_updated_rtt,
                               bool* rtt_notification_received)
      : should_notify_updated_rtt_(should_notify_updated_rtt),
        rtt_notification_received_(rtt_notification_received) {}
  ~TestSocketPerformanceWatcher() override {}

  bool ShouldNotifyUpdatedRTT() const override {
    return *should_notify_updated_rtt_;
  }

  void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) override {
    *rtt_notification_received_ = true;
  }

  void OnConnectionChanged() override {}

 private:
  bool* should_notify_updated_rtt_;
  bool* rtt_notification_received_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketPerformanceWatcher);
};

class TestSocketPerformanceWatcherFactory
    : public SocketPerformanceWatcherFactory {
 public:
  TestSocketPerformanceWatcherFactory()
      : watcher_count_(0u),
        should_notify_updated_rtt_(true),
        rtt_notification_received_(false) {}
  ~TestSocketPerformanceWatcherFactory() override {}

  // SocketPerformanceWatcherFactory implementation:
  std::unique_ptr<SocketPerformanceWatcher> CreateSocketPerformanceWatcher(
      const Protocol protocol,
      const AddressList& /* address_list */) override {
    if (protocol != PROTOCOL_QUIC) {
      return nullptr;
    }
    ++watcher_count_;
    return std::unique_ptr<SocketPerformanceWatcher>(
        new TestSocketPerformanceWatcher(&should_notify_updated_rtt_,
                                         &rtt_notification_received_));
  }

  size_t watcher_count() const { return watcher_count_; }

  bool rtt_notification_received() const { return rtt_notification_received_; }

  void set_should_notify_updated_rtt(bool should_notify_updated_rtt) {
    should_notify_updated_rtt_ = should_notify_updated_rtt;
  }

 private:
  size_t watcher_count_;
  bool should_notify_updated_rtt_;
  bool rtt_notification_received_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketPerformanceWatcherFactory);
};

class QuicNetworkTransactionTest
    : public PlatformTest,
      public ::testing::WithParamInterface<TestParams>,
      public WithTaskEnvironment {
 protected:
  QuicNetworkTransactionTest()
      : version_(GetParam().version),
        client_headers_include_h2_stream_dependency_(
            GetParam().client_headers_include_h2_stream_dependency),
        supported_versions_(quic::test::SupportedVersions(version_)),
        client_maker_(new QuicTestPacketMaker(
            version_,
            quic::QuicUtils::CreateRandomConnectionId(
                context_.random_generator()),
            context_.clock(),
            kDefaultServerHostName,
            quic::Perspective::IS_CLIENT,
            client_headers_include_h2_stream_dependency_)),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(
                          context_.random_generator()),
                      context_.clock(),
                      kDefaultServerHostName,
                      quic::Perspective::IS_SERVER,
                      false),
        quic_task_runner_(new TestTaskRunner(context_.mock_clock())),
        cert_transparency_verifier_(new MultiLogCTVerifier()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        http_server_properties_(std::make_unique<HttpServerProperties>()),
        ssl_data_(ASYNC, OK) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    request_.method = "GET";
    std::string url("https://");
    url.append(kDefaultServerHostName);
    request_.url = GURL(url);
    request_.load_flags = 0;
    request_.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    verify_details_.cert_verify_result.verified_cert = cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  }

  void SetUp() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::RunLoop().RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();
    session_.reset();
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientConnectionClosePacket(uint64_t num) {
    return client_maker_->MakeConnectionClosePacket(
        num, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!");
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerConnectionClosePacket(uint64_t num) {
    return server_maker_.MakeConnectionClosePacket(
        num, false, quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!");
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerGoAwayPacket(
      uint64_t num,
      quic::QuicErrorCode error_code,
      std::string reason_phrase) {
    return server_maker_.MakeGoAwayPacket(num, error_code, reason_phrase);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked) {
    return client_maker_->MakeAckPacket(packet_number, largest_received,
                                        smallest_received, least_unacked);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckAndRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked) {
    return client_maker_->MakeAckAndRstPacket(num, false, stream_id, error_code,
                                              largest_received,
                                              smallest_received, least_unacked);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientRstPacket(
      uint64_t num,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code) {
    return client_maker_->MakeRstPacket(num, false, stream_id, error_code,
                                        /*include_stop_sending_if_v99=*/true);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientAckAndConnectionClosePacket(uint64_t packet_number,
                                             uint64_t largest_received,
                                             uint64_t smallest_received,
                                             uint64_t least_unacked) {
    return client_maker_->MakeAckPacket(packet_number, largest_received,
                                        smallest_received, least_unacked);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientAckAndConnectionClosePacket(
      uint64_t num,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      quic::QuicErrorCode quic_error,
      const std::string& quic_error_details,
      uint64_t frame_type) {
    return client_maker_->MakeAckAndConnectionClosePacket(
        num, false, largest_received, smallest_received, least_unacked,
        quic_error, quic_error_details, frame_type);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerRstPacket(
      uint64_t num,
      bool include_version,
      quic::QuicStreamId stream_id,
      quic::QuicRstStreamErrorCode error_code) {
    return server_maker_.MakeRstPacket(num, include_version, stream_id,
                                       error_code);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number) {
    return client_maker_->MakeInitialSettingsPacket(packet_number);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked) {
    return server_maker_.MakeAckPacket(packet_number, largest_received,
                                       smallest_received, least_unacked);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructClientPriorityPacket(
      uint64_t packet_number,
      bool should_include_version,
      quic::QuicStreamId id,
      quic::QuicStreamId parent_stream_id,
      RequestPriority request_priority) {
    return client_maker_->MakePriorityPacket(
        packet_number, should_include_version, id, parent_stream_id,
        ConvertRequestPriorityToQuicPriority(request_priority));
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientAckAndPriorityFramesPacket(
      uint64_t packet_number,
      bool should_include_version,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      const std::vector<QuicTestPacketMaker::Http2StreamDependency>&
          priority_frames) {
    return client_maker_->MakeAckAndMultiplePriorityFramesPacket(
        packet_number, should_include_version, largest_received,
        smallest_received, least_unacked, priority_frames);
  }

  // Uses default QuicTestPacketMaker.
  spdy::SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                          const std::string& scheme,
                                          const std::string& path) {
    return GetRequestHeaders(method, scheme, path, client_maker_.get());
  }

  // Uses customized QuicTestPacketMaker.
  spdy::SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                          const std::string& scheme,
                                          const std::string& path,
                                          QuicTestPacketMaker* maker) {
    return maker->GetRequestHeaders(method, scheme, path);
  }

  spdy::SpdyHeaderBlock ConnectRequestHeaders(const std::string& host_port) {
    return client_maker_->ConnectRequestHeaders(host_port);
  }

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status) {
    return server_maker_.GetResponseHeaders(status);
  }

  // Appends alt_svc headers in the response headers.
  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status,
                                           const std::string& alt_svc) {
    return server_maker_.GetResponseHeaders(status, alt_svc);
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

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      quiche::QuicheStringPiece data) {
    return client_maker_->MakeDataPacket(packet_number, stream_id,
                                         should_include_version, fin, data);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckAndDataPacket(
      uint64_t packet_number,
      bool include_version,
      quic::QuicStreamId stream_id,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      bool fin,
      quiche::QuicheStringPiece data) {
    return client_maker_->MakeAckAndDataPacket(
        packet_number, include_version, stream_id, largest_received,
        smallest_received, least_unacked, fin, data);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(uint64_t packet_number,
                                      quic::QuicStreamId stream_id,
                                      bool should_include_version,
                                      bool fin,
                                      spdy::SpdyHeaderBlock headers) {
    return ConstructClientRequestHeadersPacket(packet_number, stream_id,
                                               should_include_version, fin,
                                               std::move(headers), 0);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(uint64_t packet_number,
                                      quic::QuicStreamId stream_id,
                                      bool should_include_version,
                                      bool fin,
                                      spdy::SpdyHeaderBlock headers,
                                      quic::QuicStreamId parent_stream_id) {
    return ConstructClientRequestHeadersPacket(
        packet_number, stream_id, should_include_version, fin, DEFAULT_PRIORITY,
        std::move(headers), parent_stream_id);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(uint64_t packet_number,
                                      quic::QuicStreamId stream_id,
                                      bool should_include_version,
                                      bool fin,
                                      RequestPriority request_priority,
                                      spdy::SpdyHeaderBlock headers,
                                      quic::QuicStreamId parent_stream_id) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_->MakeRequestHeadersPacket(
        packet_number, stream_id, should_include_version, fin, priority,
        std::move(headers), parent_stream_id, nullptr);
  }

  std::unique_ptr<quic::QuicReceivedPacket>
  ConstructClientRequestHeadersAndDataFramesPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      RequestPriority request_priority,
      spdy::SpdyHeaderBlock headers,
      quic::QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return client_maker_->MakeRequestHeadersAndMultipleDataFramesPacket(
        packet_number, stream_id, should_include_version, fin, priority,
        std::move(headers), parent_stream_id, spdy_headers_frame_length,
        data_writes);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerPushPromisePacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      quic::QuicStreamId promised_stream_id,
      bool should_include_version,
      spdy::SpdyHeaderBlock headers,
      QuicTestPacketMaker* maker) {
    return maker->MakePushPromisePacket(
        packet_number, stream_id, promised_stream_id, should_include_version,
        false, std::move(headers), nullptr);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerResponseHeadersPacket(uint64_t packet_number,
                                       quic::QuicStreamId stream_id,
                                       bool should_include_version,
                                       bool fin,
                                       spdy::SpdyHeaderBlock headers) {
    return server_maker_.MakeResponseHeadersPacket(packet_number, stream_id,
                                                   should_include_version, fin,
                                                   std::move(headers), nullptr);
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

  void CreateSession(const quic::ParsedQuicVersionVector& supported_versions) {
    session_params_.enable_quic = true;
    context_.params()->supported_versions = supported_versions;
    context_.params()->headers_include_h2_stream_dependency =
        client_headers_include_h2_stream_dependency_;

    session_context_.quic_context = &context_;
    session_context_.client_socket_factory = &socket_factory_;
    session_context_.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;
    session_context_.host_resolver = &host_resolver_;
    session_context_.cert_verifier = &cert_verifier_;
    session_context_.transport_security_state = &transport_security_state_;
    session_context_.cert_transparency_verifier =
        cert_transparency_verifier_.get();
    session_context_.ct_policy_enforcer = &ct_policy_enforcer_;
    session_context_.socket_performance_watcher_factory =
        &test_socket_performance_watcher_factory_;
    session_context_.proxy_resolution_service = proxy_resolution_service_.get();
    session_context_.ssl_config_service = ssl_config_service_.get();
    session_context_.http_auth_handler_factory = auth_handler_factory_.get();
    session_context_.http_server_properties = http_server_properties_.get();
    session_context_.net_log = net_log_.bound().net_log();

    session_.reset(new HttpNetworkSession(session_params_, session_context_));
    session_->quic_stream_factory()
        ->set_is_quic_known_to_work_on_current_network(true);
    SpdySessionPoolPeer spdy_pool_peer(session_->spdy_session_pool());
    spdy_pool_peer.SetEnableSendingInitialData(false);
  }

  void CreateSession() { return CreateSession(supported_versions_); }

  void CheckWasQuicResponse(HttpNetworkTransaction* trans,
                            const std::string& status_line,
                            const quic::ParsedQuicVersion& version) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ(status_line, response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_alpn_negotiated);
    EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version),
              response->connection_info);
  }

  void CheckWasQuicResponse(HttpNetworkTransaction* trans,
                            const std::string& status_line) {
    CheckWasQuicResponse(trans, status_line, version_);
  }

  void CheckWasQuicResponse(HttpNetworkTransaction* trans) {
    CheckWasQuicResponse(trans, "HTTP/1.1 200 OK", version_);
  }

  void CheckResponsePort(HttpNetworkTransaction* trans, uint16_t port) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(port, response->remote_endpoint.port());
  }

  void CheckWasHttpResponse(HttpNetworkTransaction* trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_FALSE(response->was_fetched_via_spdy);
    EXPECT_FALSE(response->was_alpn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP1_1,
              response->connection_info);
  }

  void CheckWasSpdyResponse(HttpNetworkTransaction* trans) {
    const HttpResponseInfo* response = trans->GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200", response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_alpn_negotiated);
    EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_HTTP2,
              response->connection_info);
  }

  void CheckResponseData(HttpNetworkTransaction* trans,
                         const std::string& expected) {
    std::string response_data;
    ASSERT_THAT(ReadTransaction(trans, &response_data), IsOk());
    EXPECT_EQ(expected, response_data);
  }

  void RunTransaction(HttpNetworkTransaction* trans) {
    TestCompletionCallback callback;
    int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
  }

  void SendRequestAndExpectHttpResponse(const std::string& expected) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasHttpResponse(&trans);
    CheckResponseData(&trans, expected);
  }

  void SendRequestAndExpectHttpResponseFromProxy(const std::string& expected,
                                                 bool used_proxy,
                                                 uint16_t port) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasHttpResponse(&trans);
    CheckResponsePort(&trans, port);
    CheckResponseData(&trans, expected);
    if (used_proxy) {
      EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_https());
    } else {
      EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_direct());
    }
  }
  void SendRequestAndExpectQuicResponse(const std::string& expected,
                                        const std::string& status_line) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, false, 443,
                                                   status_line);
  }

  void SendRequestAndExpectQuicResponse(const std::string& expected) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, false, 443);
  }

  void SendRequestAndExpectQuicResponseFromProxyOnPort(
      const std::string& expected,
      uint16_t port) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, true, port);
  }

  void AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode,
      const NetworkIsolationKey& network_isolation_key =
          NetworkIsolationKey()) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    url::SchemeHostPort server(request_.url);
    AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    http_server_properties_->SetQuicAlternativeService(
        server, network_isolation_key, alternative_service, expiration,
        supported_versions_);
  }

  void AddQuicRemoteAlternativeServiceMapping(
      MockCryptoClientStream::HandshakeMode handshake_mode,
      const HostPortPair& alternative) {
    crypto_client_stream_factory_.set_handshake_mode(handshake_mode);
    url::SchemeHostPort server(request_.url);
    AlternativeService alternative_service(kProtoQUIC, alternative.host(),
                                           alternative.port());
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    http_server_properties_->SetQuicAlternativeService(
        server, NetworkIsolationKey(), alternative_service, expiration,
        supported_versions_);
  }

  void ExpectBrokenAlternateProtocolMapping(
      const NetworkIsolationKey& network_isolation_key =
          NetworkIsolationKey()) {
    const url::SchemeHostPort server(request_.url);
    const AlternativeServiceInfoVector alternative_service_info_vector =
        http_server_properties_->GetAlternativeServiceInfos(
            server, network_isolation_key);
    EXPECT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_TRUE(http_server_properties_->IsAlternativeServiceBroken(
        alternative_service_info_vector[0].alternative_service(),
        network_isolation_key));
  }

  void ExpectQuicAlternateProtocolMapping(
      const NetworkIsolationKey& network_isolation_key =
          NetworkIsolationKey()) {
    const url::SchemeHostPort server(request_.url);
    const AlternativeServiceInfoVector alternative_service_info_vector =
        http_server_properties_->GetAlternativeServiceInfos(
            server, network_isolation_key);
    EXPECT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_EQ(
        kProtoQUIC,
        alternative_service_info_vector[0].alternative_service().protocol);
    EXPECT_FALSE(http_server_properties_->IsAlternativeServiceBroken(
        alternative_service_info_vector[0].alternative_service(),
        network_isolation_key));
  }

  void AddHangingNonAlternateProtocolSocketData() {
    std::unique_ptr<StaticSocketDataProvider> hanging_data;
    hanging_data.reset(new StaticSocketDataProvider());
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data->set_connect_data(hanging_connect);
    hanging_data_.push_back(std::move(hanging_data));
    socket_factory_.AddSocketDataProvider(hanging_data_.back().get());
    socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  }

  void SetUpTestForRetryConnectionOnAlternateNetwork() {
    context_.params()->migrate_sessions_on_network_change_v2 = true;
    context_.params()->migrate_sessions_early_v2 = true;
    context_.params()->retry_on_alternate_network_before_handshake = true;
    scoped_mock_change_notifier_.reset(new ScopedMockNetworkChangeNotifier());
    MockNetworkChangeNotifier* mock_ncn =
        scoped_mock_change_notifier_->mock_network_change_notifier();
    mock_ncn->ForceNetworkHandlesSupported();
    mock_ncn->SetConnectedNetworksList(
        {kDefaultNetworkForTests, kNewNetworkForTests});
  }

  // Fetches two non-cryptographic URL requests via a HTTPS proxy with a QUIC
  // alternative proxy. Verifies that if the alternative proxy job returns
  // |error_code|, the request is fetched successfully by the main job.
  void TestAlternativeProxy(int error_code) {
    // Use a non-cryptographic scheme for the request URL since this request
    // will be fetched via proxy with QUIC as the alternative service.
    request_.url = GURL("http://example.org/");
    // Data for the alternative proxy server job.
    MockWrite quic_writes[] = {MockWrite(SYNCHRONOUS, error_code, 1)};
    MockRead quic_reads[] = {
        MockRead(SYNCHRONOUS, error_code, 0),
    };

    SequencedSocketData quic_data(quic_reads, quic_writes);
    socket_factory_.AddSocketDataProvider(&quic_data);

    // Main job succeeds and the alternative job fails.
    // Add data for two requests that will be read by the main job.
    MockRead http_reads_1[] = {
        MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
        MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
        MockRead(ASYNC, OK)};

    MockRead http_reads_2[] = {
        MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
        MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
        MockRead(ASYNC, OK)};

    StaticSocketDataProvider http_data_1(http_reads_1, base::span<MockWrite>());
    StaticSocketDataProvider http_data_2(http_reads_2, base::span<MockWrite>());
    socket_factory_.AddSocketDataProvider(&http_data_1);
    socket_factory_.AddSocketDataProvider(&http_data_2);
    socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
    socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

    TestProxyDelegate test_proxy_delegate;
    // Proxy URL is different from the request URL.
    test_proxy_delegate.set_alternative_proxy_server(
        ProxyServer::FromPacString("QUIC myproxy.org:443"));

    proxy_resolution_service_ =
        ConfiguredProxyResolutionService::CreateFixedFromPacResult(
            "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);
    proxy_resolution_service_->SetProxyDelegate(&test_proxy_delegate);

    CreateSession();
    EXPECT_TRUE(test_proxy_delegate.alternative_proxy_server().is_valid());

    // The first request should be fetched via the HTTPS proxy.
    SendRequestAndExpectHttpResponseFromProxy("hello from http", true, 443);

    // Since the main job succeeded only the alternative proxy server should be
    // marked as bad.
    EXPECT_THAT(session_->proxy_resolution_service()->proxy_retry_info(),
                ElementsAre(Key("quic://myproxy.org:443")));

    // Verify that the second request completes successfully, and the
    // alternative proxy server job is not started.
    SendRequestAndExpectHttpResponseFromProxy("hello from http", true, 443);
  }

  // Adds a new socket data provider for an HTTP request, and runs a request,
  // expecting it to be used.
  void AddHttpDataAndRunRequest() {
    MockWrite http_writes[] = {
        MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
        MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
        MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

    MockRead http_reads[] = {
        MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
        MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
        MockRead(SYNCHRONOUS, 5, "http used"),
        // Connection closed.
        MockRead(SYNCHRONOUS, OK, 6)};
    SequencedSocketData http_data(http_reads, http_writes);
    socket_factory_.AddSocketDataProvider(&http_data);
    SSLSocketDataProvider ssl_data(ASYNC, OK);
    socket_factory_.AddSSLSocketDataProvider(&ssl_data);
    SendRequestAndExpectHttpResponse("http used");
    EXPECT_TRUE(http_data.AllWriteDataConsumed());
    EXPECT_TRUE(http_data.AllReadDataConsumed());
  }

  // Adds a new socket data provider for a QUIC request, and runs a request,
  // expecting it to be used. The new QUIC session is not closed.
  void AddQuicDataAndRunRequest() {
    QuicTestPacketMaker client_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        client_headers_include_h2_stream_dependency_);
    QuicTestPacketMaker server_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        false);
    MockQuicData quic_data(version_);
    int packet_number = 1;
    client_maker.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
    if (VersionUsesHttp3(version_.transport_version)) {
      quic_data.AddWrite(
          SYNCHRONOUS, client_maker.MakeInitialSettingsPacket(packet_number++));
    }
    quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
            true, true, ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
            GetRequestHeaders("GET", "https", "/", &client_maker), 0, nullptr));
    client_maker.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
    quic_data.AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   false, server_maker.GetResponseHeaders("200 OK"), nullptr));
    std::string header = ConstructDataHeader(9);
    quic_data.AddRead(
        ASYNC, server_maker.MakeDataPacket(
                   2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   true, header + "quic used"));
    // Don't care about the final ack.
    quic_data.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);
    // No more data to read.
    quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    quic_data.AddSocketDataToFactory(&socket_factory_);
    SendRequestAndExpectQuicResponse("quic used");

    EXPECT_TRUE(quic_data.AllReadDataConsumed());
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) const {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) const {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
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

  static void AddCertificate(SSLSocketDataProvider* ssl_data) {
    ssl_data->ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
    ASSERT_TRUE(ssl_data->ssl_info.cert);
  }

  void SendRequestAndExpectQuicResponseMaybeFromProxy(
      const std::string& expected,
      bool used_proxy,
      uint16_t port,
      const std::string& status_line,
      const quic::ParsedQuicVersion& version) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    RunTransaction(&trans);
    CheckWasQuicResponse(&trans, status_line, version);
    CheckResponsePort(&trans, port);
    CheckResponseData(&trans, expected);
    if (used_proxy) {
      EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_quic());
    } else {
      EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_direct());
    }
  }

  const quic::ParsedQuicVersion version_;
  const bool client_headers_include_h2_stream_dependency_;
  quic::ParsedQuicVersionVector supported_versions_;
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  MockQuicContext context_;
  std::unique_ptr<QuicTestPacketMaker> client_maker_;
  QuicTestPacketMaker server_maker_;
  scoped_refptr<TestTaskRunner> quic_task_runner_;
  std::unique_ptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<CTVerifier> cert_transparency_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  TestSocketPerformanceWatcherFactory test_socket_performance_watcher_factory_;
  std::unique_ptr<SSLConfigServiceDefaults> ssl_config_service_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  HttpNetworkSession::Params session_params_;
  HttpNetworkSession::Context session_context_;
  HttpRequestInfo request_;
  RecordingBoundTestNetLog net_log_;
  std::vector<std::unique_ptr<StaticSocketDataProvider>> hanging_data_;
  SSLSocketDataProvider ssl_data_;
  std::unique_ptr<ScopedMockNetworkChangeNotifier> scoped_mock_change_notifier_;

 private:
  void SendRequestAndExpectQuicResponseMaybeFromProxy(
      const std::string& expected,
      bool used_proxy,
      uint16_t port,
      const std::string& status_line) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, used_proxy, port,
                                                   status_line, version_);
  }

  void SendRequestAndExpectQuicResponseMaybeFromProxy(
      const std::string& expected,
      bool used_proxy,
      uint16_t port) {
    SendRequestAndExpectQuicResponseMaybeFromProxy(expected, used_proxy, port,
                                                   "HTTP/1.1 200 OK", version_);
  }
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicNetworkTransactionTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

// TODO(950069): Add testing for frame_origin in NetworkIsolationKey using
// kAppendInitiatingFrameOriginToNetworkIsolationKey.

TEST_P(QuicNetworkTransactionTest, WriteErrorHandshakeConfirmed) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  base::HistogramTester histograms;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    mock_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  mock_quic_data.AddWrite(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data.AddRead(ASYNC, OK);              // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  histograms.ExpectBucketCount("Net.QuicSession.WriteError",
                               -ERR_INTERNET_DISCONNECTED, 1);
  histograms.ExpectBucketCount("Net.QuicSession.WriteError.HandshakeConfirmed",
                               -ERR_INTERNET_DISCONNECTED, 1);
}

TEST_P(QuicNetworkTransactionTest, WriteErrorHandshakeConfirmedAsync) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  base::HistogramTester histograms;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version))
    mock_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  mock_quic_data.AddWrite(ASYNC, ERR_INTERNET_DISCONNECTED);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  mock_quic_data.AddRead(ASYNC, OK);              // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  histograms.ExpectBucketCount("Net.QuicSession.WriteError",
                               -ERR_INTERNET_DISCONNECTED, 1);
  histograms.ExpectBucketCount("Net.QuicSession.WriteError.HandshakeConfirmed",
                               -ERR_INTERNET_DISCONNECTED, 1);
}

TEST_P(QuicNetworkTransactionTest, SocketWatcherEnabled) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  test_socket_performance_watcher_factory_.set_should_notify_updated_rtt(true);

  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

TEST_P(QuicNetworkTransactionTest, SocketWatcherDisabled) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  test_socket_performance_watcher_factory_.set_should_notify_updated_rtt(false);

  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

TEST_P(QuicNetworkTransactionTest, ForceQuic) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");

  // Check that the NetLog was filled reasonably.
  auto entries = net_log_.GetEntries();
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_SESSION_PACKET_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  // ... and also a TYPE_QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0,
      NetLogEventType::QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  EXPECT_EQ(1, GetIntegerValueFromParams(entries[pos], "packet_number"));

  // ... and also a TYPE_QUIC_SESSION_PACKET_AUTHENTICATED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_SESSION_PACKET_AUTHENTICATED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  // ... and also a QUIC_SESSION_STREAM_FRAME_RECEIVED.
  pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_SESSION_STREAM_FRAME_RECEIVED,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);

  int log_stream_id = GetIntegerValueFromParams(entries[pos], "stream_id");
  if (quic::VersionUsesHttp3(version_.transport_version)) {
    EXPECT_EQ(GetNthClientInitiatedBidirectionalStreamId(0),
              static_cast<quic::QuicStreamId>(log_stream_id));
  } else {
    EXPECT_EQ(quic::QuicUtils::GetHeadersStreamId(version_.transport_version),
              static_cast<quic::QuicStreamId>(log_stream_id));
  }
}

// Regression test for https://crbug.com/1043531.
TEST_P(QuicNetworkTransactionTest, ResetOnEmptyResponseHeaders) {
  if (!quic::VersionUsesHttp3(version_.transport_version)) {
    return;
  }

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int write_packet_num = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructInitialSettingsPacket(write_packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          write_packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, true, GetRequestHeaders("GET", "https", "/")));

  const quic::QuicStreamId request_stream_id =
      GetNthClientInitiatedBidirectionalStreamId(0);
  spdy::SpdyHeaderBlock empty_response_headers;
  const std::string response_data = server_maker_.QpackEncodeHeaders(
      request_stream_id, std::move(empty_response_headers), nullptr);
  uint64_t read_packet_num = 1;
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(read_packet_num++, request_stream_id,
                                       false, false, response_data));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddWrite(
      ASYNC,
      ConstructClientAckAndDataPacket(
          write_packet_num++, true, GetQpackDecoderStreamId(), 1, 1, 1, false,
          StreamCancellationQpackDecoderInstruction(request_stream_id)));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_INVALID_RESPONSE));
}

TEST_P(QuicNetworkTransactionTest, LargeResponseHeaders) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  spdy::SpdyHeaderBlock response_headers = GetResponseHeaders("200 OK");
  response_headers["key1"] = std::string(30000, 'A');
  response_headers["key2"] = std::string(30000, 'A');
  response_headers["key3"] = std::string(30000, 'A');
  response_headers["key4"] = std::string(30000, 'A');
  response_headers["key5"] = std::string(30000, 'A');
  response_headers["key6"] = std::string(30000, 'A');
  response_headers["key7"] = std::string(30000, 'A');
  response_headers["key8"] = std::string(30000, 'A');
  quic::QuicStreamId stream_id;
  std::string response_data;
  if (quic::VersionUsesHttp3(version_.transport_version)) {
    stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
    response_data = server_maker_.QpackEncodeHeaders(
        stream_id, std::move(response_headers), nullptr);
  } else {
    stream_id = quic::QuicUtils::GetHeadersStreamId(version_.transport_version);
    spdy::SpdyHeadersIR headers_frame(
        GetNthClientInitiatedBidirectionalStreamId(0),
        std::move(response_headers));
    spdy::SpdyFramer response_framer(spdy::SpdyFramer::ENABLE_COMPRESSION);
    spdy::SpdySerializedFrame spdy_frame =
        response_framer.SerializeFrame(headers_frame);
    response_data = std::string(spdy_frame.data(), spdy_frame.size());
  }

  uint64_t packet_number = 1;
  size_t chunk_size = 1200;
  for (size_t offset = 0; offset < response_data.length();
       offset += chunk_size) {
    size_t len = std::min(chunk_size, response_data.length() - offset);
    mock_quic_data.AddRead(
        ASYNC, ConstructServerDataPacket(
                   packet_number++, stream_id, false, false,
                   base::StringPiece(response_data.data() + offset, len)));
  }

  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 packet_number, GetNthClientInitiatedBidirectionalStreamId(0),
                 false, true, header + "hello!"));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(ASYNC,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddWrite(
      ASYNC, ConstructClientAckPacket(packet_num++, packet_number, 3, 1));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, TooLargeResponseHeaders) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));

  spdy::SpdyHeaderBlock response_headers = GetResponseHeaders("200 OK");
  response_headers["key1"] = std::string(30000, 'A');
  response_headers["key2"] = std::string(30000, 'A');
  response_headers["key3"] = std::string(30000, 'A');
  response_headers["key4"] = std::string(30000, 'A');
  response_headers["key5"] = std::string(30000, 'A');
  response_headers["key6"] = std::string(30000, 'A');
  response_headers["key7"] = std::string(30000, 'A');
  response_headers["key8"] = std::string(30000, 'A');
  response_headers["key9"] = std::string(30000, 'A');

  quic::QuicStreamId stream_id;
  std::string response_data;
  if (quic::VersionUsesHttp3(version_.transport_version)) {
    stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
    response_data = server_maker_.QpackEncodeHeaders(
        stream_id, std::move(response_headers), nullptr);
  } else {
    stream_id = quic::QuicUtils::GetHeadersStreamId(version_.transport_version);
    spdy::SpdyHeadersIR headers_frame(
        GetNthClientInitiatedBidirectionalStreamId(0),
        std::move(response_headers));
    spdy::SpdyFramer response_framer(spdy::SpdyFramer::ENABLE_COMPRESSION);
    spdy::SpdySerializedFrame spdy_frame =
        response_framer.SerializeFrame(headers_frame);
    response_data = std::string(spdy_frame.data(), spdy_frame.size());
  }

  uint64_t packet_number = 1;
  size_t chunk_size = 1200;
  for (size_t offset = 0; offset < response_data.length();
       offset += chunk_size) {
    size_t len = std::min(chunk_size, response_data.length() - offset);
    mock_quic_data.AddRead(
        ASYNC, ConstructServerDataPacket(
                   packet_number++, stream_id, false, false,
                   base::StringPiece(response_data.data() + offset, len)));
  }

  std::string header = ConstructDataHeader(6);

  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 packet_number, GetNthClientInitiatedBidirectionalStreamId(0),
                 false, true, header + "hello!"));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(ASYNC,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddWrite(
      ASYNC, ConstructClientAckAndRstPacket(
                 packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
                 quic::QUIC_HEADERS_TOO_LARGE, packet_number, 3, 1));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_P(QuicNetworkTransactionTest, ForceQuicForAll) {
  context_.params()->origins_to_force_quic_on.insert(HostPortPair());

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

// Regression test for https://crbug.com/695225
TEST_P(QuicNetworkTransactionTest, 408Response) {
  context_.params()->origins_to_force_quic_on.insert(HostPortPair());

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("408 Request Timeout")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!", "HTTP/1.1 408 Request Timeout");
}

TEST_P(QuicNetworkTransactionTest, QuicProxy) {
  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC mail.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  // There is no need to set up an alternate protocol job, because
  // no attempt will be made to speak to the proxy over TCP.

  request_.url = GURL("http://mail.example.org/");
  CreateSession();

  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 70);
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

// Regression test for https://crbug.com/492458.  Test that for an HTTP
// connection through a QUIC proxy, the certificate exhibited by the proxy is
// checked against the proxy hostname, not the origin hostname.
TEST_P(QuicNetworkTransactionTest, QuicProxyWithCert) {
  const std::string origin_host = "mail.example.com";
  const std::string proxy_host = "www.example.org";

  session_params_.enable_quic = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC " + proxy_host + ":70", TRAFFIC_ANNOTATION_FOR_TESTS);

  client_maker_->set_hostname(origin_host);
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert.get());
  // This certificate is valid for the proxy, but not for the origin.
  EXPECT_TRUE(cert->VerifyNameMatch(proxy_host));
  EXPECT_FALSE(cert->VerifyNameMatch(origin_host));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  request_.url = GURL("http://" + origin_host);
  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 70);
}

TEST_P(QuicNetworkTransactionTest, AlternativeServicesDifferentHost) {
  context_.params()->allow_remote_alt_svc = true;
  HostPortPair origin("www.example.org", 443);
  HostPortPair alternative("mail.example.org", 443);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert.get());
  // TODO(rch): the connection should be "to" the origin, so if the cert is
  // valid for the origin but not the alternative, that should work too.
  EXPECT_TRUE(cert->VerifyNameMatch(origin.host()));
  EXPECT_TRUE(cert->VerifyNameMatch(alternative.host()));
  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  client_maker_->set_hostname(origin.host());
  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  request_.url = GURL("https://" + origin.host());
  AddQuicRemoteAlternativeServiceMapping(
      MockCryptoClientStream::CONFIRM_HANDSHAKE, alternative);
  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, DoNotUseQuicForUnsupportedVersion) {
  quic::ParsedQuicVersion unsupported_version = quic::UnsupportedQuicVersion();
  // Add support for another QUIC version besides |version_|. Also find an
  // unsupported version.
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (version == version_)
      continue;
    if (supported_versions_.size() != 2) {
      supported_versions_.push_back(version);
      continue;
    }
    unsupported_version = version;
    break;
  }
  ASSERT_EQ(2u, supported_versions_.size());
  ASSERT_NE(quic::UnsupportedQuicVersion(), unsupported_version);

  // Set up alternative service to use QUIC with a version that is not
  // supported.
  url::SchemeHostPort server(request_.url);
  AlternativeService alternative_service(kProtoQUIC, kDefaultServerHostName,
                                         443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkIsolationKey(), alternative_service, expiration,
      {unsupported_version});

  AlternativeServiceInfoVector alt_svc_info_vector =
      http_server_properties_->GetAlternativeServiceInfos(
          server, NetworkIsolationKey());
  EXPECT_EQ(1u, alt_svc_info_vector.size());
  EXPECT_EQ(kProtoQUIC, alt_svc_info_vector[0].alternative_service().protocol);
  EXPECT_EQ(1u, alt_svc_info_vector[0].advertised_versions().size());
  EXPECT_EQ(unsupported_version,
            alt_svc_info_vector[0].advertised_versions()[0]);

  // First request should still be sent via TCP as the QUIC version advertised
  // in the stored AlternativeService is not supported by the client. However,
  // the response from the server will advertise new Alt-Svc with supported
  // versions.
  std::string altsvc_header = GenerateQuicAltSvcHeader(supported_versions_);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(altsvc_header.c_str()),
      MockRead("\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Second request should be sent via QUIC as a new list of verions supported
  // by the client has been advertised by the server.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();

  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  // Check alternative service list is updated with new versions.
  alt_svc_info_vector =
      session_->http_server_properties()->GetAlternativeServiceInfos(
          server, NetworkIsolationKey());
  // All PROTOCOL_QUIC_CRYPTO versions are sent in a single Alt-Svc entry,
  // therefore they aer accumulated in a single AlternativeServiceInfo, whereas
  // each PROTOCOL_TLS1_3 version has its own Alt-Svc entry and
  // AlternativeServiceInfo entry.  Flatten to compare.
  quic::ParsedQuicVersionVector alt_svc_negotiated_versions;
  for (const auto& alt_svc_info : alt_svc_info_vector) {
    EXPECT_EQ(kProtoQUIC, alt_svc_info.alternative_service().protocol);
    for (const auto& version : alt_svc_info.advertised_versions()) {
      if (std::find(alt_svc_negotiated_versions.begin(),
                    alt_svc_negotiated_versions.end(),
                    version) == alt_svc_negotiated_versions.end()) {
        alt_svc_negotiated_versions.push_back(version);
      }
    }
  }

  ASSERT_EQ(supported_versions_.size(), alt_svc_negotiated_versions.size());
  auto version_compare = [](const quic::ParsedQuicVersion& a,
                            const quic::ParsedQuicVersion& b) {
    return std::tie(a.transport_version, a.handshake_protocol) <
           std::tie(b.transport_version, b.handshake_protocol);
  };
  std::sort(supported_versions_.begin(), supported_versions_.end(),
            version_compare);
  std::sort(alt_svc_negotiated_versions.begin(),
            alt_svc_negotiated_versions.end(), version_compare);
  EXPECT_TRUE(std::equal(supported_versions_.begin(), supported_versions_.end(),
                         alt_svc_negotiated_versions.begin()));
}

// Regression test for https://crbug.com/546991.
// The server might not be able to serve a request on an alternative connection,
// and might send a 421 Misdirected Request response status to indicate this.
// HttpNetworkTransaction should reset the request and retry without using
// alternative services.
TEST_P(QuicNetworkTransactionTest, RetryMisdirectedRequest) {
  // Set up alternative service to use QUIC.
  // Note that |origins_to_force_quic_on| cannot be used in this test, because
  // that overrides |enable_alternative_services|.
  url::SchemeHostPort server(request_.url);
  AlternativeService alternative_service(kProtoQUIC, kDefaultServerHostName,
                                         443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkIsolationKey(), alternative_service, expiration,
      supported_versions_);

  // First try: The alternative job uses QUIC and reports an HTTP 421
  // Misdirected Request error.  The main job uses TCP, but |http_data| below is
  // paused at Connect(), so it will never exit the socket pool. This ensures
  // that the alternate job always wins the race and keeps whether the
  // |http_data| exits the socket pool before the main job is aborted
  // deterministic. The first main job gets aborted without the socket pool ever
  // dispensing the socket, making it available for the second try.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 GetResponseHeaders("421")));
  mock_quic_data.AddRead(ASYNC, OK);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // Second try: The main job uses TCP, and there is no alternate job. Once the
  // Connect() is unblocked, |http_data| will leave the socket pool, binding to
  // the main job of the second request. It then succeeds over HTTP/1.1.
  // Note that if there was an alternative QUIC Job created for the second try,
  // that would read these data, and would fail with ERR_QUIC_PROTOCOL_ERROR.
  // Therefore this test ensures that no alternative Job is created on retry.
  MockWrite writes[] = {MockWrite(ASYNC, 0, "GET / HTTP/1.1\r\n"),
                        MockWrite(ASYNC, 1, "Host: mail.example.org\r\n"),
                        MockWrite(ASYNC, 2, "Connection: keep-alive\r\n\r\n")};
  MockRead reads[] = {MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n\r\n"),
                      MockRead(ASYNC, 4, "hello!"), MockRead(ASYNC, OK, 5)};
  SequencedSocketData http_data(MockConnect(ASYNC, ERR_IO_PENDING) /* pause */,
                                reads, writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());

  // Run until |mock_quic_data| has failed and |http_data| has paused.
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();

  // |mock_quic_data| must have run to completion.
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());

  // Now that the QUIC data has been consumed, unblock |http_data|.
  http_data.socket()->OnConnectComplete(MockConnect());

  // The retry logic must hide the 421 status. The transaction succeeds on
  // |http_data|.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckWasHttpResponse(&trans);
  CheckResponsePort(&trans, 443);
  CheckResponseData(&trans, "hello!");
}

TEST_P(QuicNetworkTransactionTest, ForceQuicWithErrorConnecting) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data1(version_);
  if (VersionUsesHttp3(version_.transport_version))
    mock_quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  mock_quic_data1.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  client_maker_->Reset();
  MockQuicData mock_quic_data2(version_);
  if (VersionUsesHttp3(version_.transport_version))
    mock_quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
  mock_quic_data2.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);

  mock_quic_data1.AddSocketDataToFactory(&socket_factory_);
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  EXPECT_EQ(0U, test_socket_performance_watcher_factory_.watcher_count());
  for (size_t i = 0; i < 2; ++i) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    TestCompletionCallback callback;
    int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
    EXPECT_EQ(1 + i, test_socket_performance_watcher_factory_.watcher_count());

    NetErrorDetails details;
    trans.PopulateNetErrorDetails(&details);
    EXPECT_EQ(quic::QUIC_PACKET_READ_ERROR, details.quic_connection_error);
  }
}

TEST_P(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("www.google.com:443"));

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  EXPECT_EQ(0U, test_socket_performance_watcher_factory_.watcher_count());
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, UseIetfAlternativeServiceForQuic) {
  std::string alt_svc_header =
      "Alt-Svc: " + quic::AlpnForVersion(version_) + "=\":443\"\r\n\r\n";
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(alt_svc_header.data()),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

// Much like above, but makes sure NetworkIsolationKey is respected.
TEST_P(QuicNetworkTransactionTest,
       UseAlternativeServiceForQuicWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  AddCertificate(&ssl_data_);

  // Request with empty NetworkIsolationKey.
  StaticSocketDataProvider http_data1(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data1);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // First request with kNetworkIsolationKey1.
  StaticSocketDataProvider http_data2(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Request with kNetworkIsolationKey2.
  StaticSocketDataProvider http_data3(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data3);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Second request with kNetworkIsolationKey1, can finally use QUIC, since
  // alternative service infrmation has been received in this context before.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  // This is first so that the test fails if alternative service info is
  // written with the right NetworkIsolationKey, but always queried with an
  // empty one.
  request_.network_isolation_key = NetworkIsolationKey();
  SendRequestAndExpectHttpResponse("hello world");
  request_.network_isolation_key = kNetworkIsolationKey1;
  SendRequestAndExpectHttpResponse("hello world");
  request_.network_isolation_key = kNetworkIsolationKey2;
  SendRequestAndExpectHttpResponse("hello world");

  // Only use QUIC when using a NetworkIsolationKey which has been used when
  // alternative service information was received.
  request_.network_isolation_key = kNetworkIsolationKey1;
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceWithVersionForQuic1) {
  // Both server advertises and client supports two QUIC versions.
  // Only |version_| is advertised and supported.
  // The QuicStreamFactoy will pick up |version_|, which is verified as the
  // PacketMakers are using |version_|.

  // Add support for another QUIC version besides |version_| on the client side.
  // Also find a different version advertised by the server.
  quic::ParsedQuicVersion advertised_version_2 = quic::UnsupportedQuicVersion();
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (version == version_)
      continue;
    if (supported_versions_.size() != 2) {
      supported_versions_.push_back(version);
      continue;
    }
    advertised_version_2 = version;
    break;
  }
  ASSERT_EQ(2u, supported_versions_.size());
  ASSERT_NE(quic::UnsupportedQuicVersion(), advertised_version_2);

  std::string QuicAltSvcWithVersionHeader =
      base::StringPrintf("Alt-Svc: %s=\":443\", %s=\":443\"\r\n\r\n",
                         quic::AlpnForVersion(advertised_version_2).c_str(),
                         quic::AlpnForVersion(version_).c_str());

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(QuicAltSvcWithVersionHeader.c_str()), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest,
       PickQuicVersionWhenMultipleVersionsAreSupported) {
  // Client and server both support more than one QUIC_VERSION.
  // Client prefers |version_|, and then common_version_2.
  // Server prefers common_version_2, and then |version_|.
  // We should honor the server's preference.
  // The picked version is verified via checking the version used by the
  // TestPacketMakers and the response.

  // Find an alternative commonly supported version other than |version_|.
  quic::ParsedQuicVersion common_version_2 = quic::UnsupportedQuicVersion();
  if (version_.handshake_protocol == quic::PROTOCOL_QUIC_CRYPTO) {
    for (const quic::ParsedQuicVersion& version :
         quic::AllSupportedVersions()) {
      if (version == version_)
        continue;
      common_version_2 = version;
      break;
    }
  } else if (version_.transport_version == quic::QUIC_VERSION_IETF_DRAFT_27) {
    common_version_2 = quic::ParsedQuicVersion(
        quic::PROTOCOL_TLS1_3, quic::QUIC_VERSION_IETF_DRAFT_25);
  } else {
    common_version_2 = quic::ParsedQuicVersion(
        quic::PROTOCOL_TLS1_3, quic::QUIC_VERSION_IETF_DRAFT_27);
  }
  ASSERT_NE(quic::UnsupportedQuicVersion(), common_version_2);

  // Setting up client's preference list: {|version_|, |common_version_2|}.
  supported_versions_.clear();
  supported_versions_.push_back(version_);
  supported_versions_.push_back(common_version_2);

  // Setting up server's Alt-Svc header in the following preference order:
  // |common_version_2|, |version_|.
  std::string QuicAltSvcWithVersionHeader;
  quic::ParsedQuicVersion picked_version = quic::UnsupportedQuicVersion();
    QuicAltSvcWithVersionHeader =
        "Alt-Svc: " + quic::AlpnForVersion(common_version_2) +
        "=\":443\"; ma=3600, " + quic::AlpnForVersion(version_) +
        "=\":443\"; ma=3600\r\n\r\n";
    picked_version = common_version_2;  // Use server's preference.

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(QuicAltSvcWithVersionHeader.c_str()), MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(picked_version);

  // Reset QuicTestPacket makers as the version picked may not be |version_|.
  client_maker_.reset(new QuicTestPacketMaker(
      picked_version,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_));
  QuicTestPacketMaker server_maker(
      picked_version,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
      false);

  int packet_num = 1;
  if (VersionUsesHttp3(picked_version.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }

  quic::QuicStreamId client_stream_0 =
      quic::test::GetNthClientInitiatedBidirectionalStreamId(
          picked_version.transport_version, 0);
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientRequestHeadersPacket(
                              packet_num++, client_stream_0, true, true,
                              GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, server_maker.MakeResponseHeadersPacket(
                 1, client_stream_0, false, false,
                 server_maker.GetResponseHeaders("200 OK"), nullptr));
  std::string header("");
  if (VersionUsesHttp3(picked_version.transport_version)) {
    std::unique_ptr<char[]> buffer;
    auto header_length =
        quic::HttpEncoder::SerializeDataFrameHeader(6, &buffer);
    header = std::string(buffer.get(), header_length);
  }

  mock_quic_data.AddRead(
      ASYNC, server_maker.MakeDataPacket(2, client_stream_0, false, true,
                                         header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponseMaybeFromProxy(
      "hello!", false, 443, "HTTP/1.1 200 OK", picked_version);
}

TEST_P(QuicNetworkTransactionTest,
       UseAlternativeServiceWithProbabilityForQuic) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternativeServiceWithProbabilityHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, SetAlternativeServiceWithScheme) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\"foo.example.org:443\", quic=\":444\"\r\n\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  // Send https request, ignore alternative service advertising if response
  // header advertises alternative service for mail.example.org.
  request_.url = GURL("https://mail.example.org:443");
  SendRequestAndExpectHttpResponse("hello world");
  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  url::SchemeHostPort http_server("http", "mail.example.org", 443);
  url::SchemeHostPort https_server("https", "mail.example.org", 443);
  // Check alternative service is set for the correct origin.
  EXPECT_EQ(
      2u, http_server_properties
              ->GetAlternativeServiceInfos(https_server, NetworkIsolationKey())
              .size());
  EXPECT_TRUE(
      http_server_properties
          ->GetAlternativeServiceInfos(http_server, NetworkIsolationKey())
          .empty());
}

TEST_P(QuicNetworkTransactionTest, DoNotGetAltSvcForDifferentOrigin) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\"foo.example.org:443\", quic=\":444\"\r\n\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  AddCertificate(&ssl_data_);

  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  // Send https request and set alternative services if response header
  // advertises alternative service for mail.example.org.
  SendRequestAndExpectHttpResponse("hello world");
  HttpServerProperties* http_server_properties =
      session_->http_server_properties();

  const url::SchemeHostPort https_server(request_.url);
  // Check alternative service is set.
  EXPECT_EQ(
      2u, http_server_properties
              ->GetAlternativeServiceInfos(https_server, NetworkIsolationKey())
              .size());

  // Send http request to the same origin but with diffrent scheme, should not
  // use QUIC.
  request_.url = GURL("http://mail.example.org:443");
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest,
       StoreMutuallySupportedVersionsWhenProcessAltSvc) {
  // Add support for another QUIC version besides |version_|.
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (version != version_) {
      supported_versions_.push_back(version);
      break;
    }
  }

  std::string altsvc_header = GenerateQuicAltSvcHeader(supported_versions_);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(altsvc_header.c_str()),
      MockRead("\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();

  CreateSession(supported_versions_);

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  // Alt-Svc header contains all possible versions, so alternative services
  // should contain all of |supported_versions_|.
  const url::SchemeHostPort https_server(request_.url);
  const AlternativeServiceInfoVector alt_svc_info_vector =
      session_->http_server_properties()->GetAlternativeServiceInfos(
          https_server, NetworkIsolationKey());
  // However, all PROTOCOL_QUIC_CRYPTO versions are sent in a single Alt-Svc
  // entry, therefore they aer accumulated in a single AlternativeServiceInfo,
  // whereas each PROTOCOL_TLS1_3 version has its own Alt-Svc entry and
  // AlternativeServiceInfo entry.  Flatten to compare.
  quic::ParsedQuicVersionVector alt_svc_negotiated_versions;
  for (const auto& alt_svc_info : alt_svc_info_vector) {
    EXPECT_EQ(kProtoQUIC, alt_svc_info.alternative_service().protocol);
    for (const auto& version : alt_svc_info.advertised_versions()) {
      if (std::find(alt_svc_negotiated_versions.begin(),
                    alt_svc_negotiated_versions.end(),
                    version) == alt_svc_negotiated_versions.end()) {
        alt_svc_negotiated_versions.push_back(version);
      }
    }
  }

  ASSERT_EQ(supported_versions_.size(), alt_svc_negotiated_versions.size());
  auto version_compare = [](const quic::ParsedQuicVersion& a,
                            const quic::ParsedQuicVersion& b) {
    return std::tie(a.transport_version, a.handshake_protocol) <
           std::tie(b.transport_version, b.handshake_protocol);
  };
  std::sort(supported_versions_.begin(), supported_versions_.end(),
            version_compare);
  std::sort(alt_svc_negotiated_versions.begin(),
            alt_svc_negotiated_versions.end(), version_compare);
  EXPECT_TRUE(std::equal(supported_versions_.begin(), supported_versions_.end(),
                         alt_svc_negotiated_versions.begin()));
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceAllSupportedVersion) {
  std::string altsvc_header = base::StringPrintf(
      "Alt-Svc: %s=\":443\"\r\n\r\n", quic::AlpnForVersion(version_).c_str());
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(altsvc_header.c_str()),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, GoAwayWithConnectionMigrationOnPortsOnly) {
  if (version_.HasIetfQuicFrames()) {
    // GoAway is not available under version 99
    return;
  }
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  // Read a GoAway packet with
  // quic::QuicErrorCode: quic::QUIC_ERROR_MIGRATING_PORT from the peer.
  mock_quic_data.AddRead(SYNCHRONOUS,
                         ConstructServerGoAwayPacket(
                             2, quic::QUIC_ERROR_MIGRATING_PORT,
                             "connection migration with port change only"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       true, header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndRstPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED, 3, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check whether this transaction is correctly marked as received a go-away
  // because of migrating port.
  NetErrorDetails details;
  EXPECT_FALSE(details.quic_port_migration_detected);
  trans.PopulateNetErrorDetails(&details);
  EXPECT_TRUE(details.quic_port_migration_detected);
}

// This test verifies that a new QUIC connection will be attempted on the
// alternate network if the original QUIC connection fails with idle timeout
// before handshake is confirmed.  If TCP succeeds and QUIC fails on the
// alternate network as well, QUIC is marked as broken and the brokenness will
// not expire when default network changes.
TEST_P(QuicNetworkTransactionTest, QuicFailsOnBothNetworksWhileTCPSucceeds) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 ||
      GetQuicReloadableFlag(quic_use_idle_network_detector)) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    // TODO(fayang): Add time driven idle network detection test.
    return;
  }
  SetUpTestForRetryConnectionOnAlternateNetwork();

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));  // CHLO
  // Retranmit the handshake messages.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  // After timeout, connection will be closed with QUIC_NETWORK_IDLE_TIMEOUT.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeConnectionClosePacket(
                         packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                         "No recent network activity after 4s. Timeout:4s"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // Add successful TCP data so that TCP job will succeed.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "TCP succeeds"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Add data for the second QUIC connection to fail.
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);  // Write error.
  quic_data2.AddSocketDataToFactory(&socket_factory_);

  // Resolve the host resolution synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));
  // Add alternate protocol mapping to race QUIC and TCP.
  // QUIC connection requires handshake to be confirmed and sends CHLO to the
  // peer.
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  // Request will be served with TCP job.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckResponseData(&trans, "TCP succeeds");

  // Fast forward to idle timeout the original connection. A new connection will
  // be kicked off on the alternate network.
  quic_task_runner_->FastForwardBy(quic::QuicTime::Delta::FromSeconds(4));
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  // Run the message loop to execute posted tasks, which will report job status.
  base::RunLoop().RunUntilIdle();

  // Verify that QUIC is marked as broken.
  ExpectBrokenAlternateProtocolMapping();

  // Deliver a message to notify the new network becomes default, the brokenness
  // will not expire as QUIC is broken on both networks.
  scoped_mock_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  ExpectBrokenAlternateProtocolMapping();

  ASSERT_TRUE(quic_data2.AllReadDataConsumed());
  ASSERT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that a new QUIC connection will be attempted on the
// alternate network if the original QUIC connection fails with idle timeout
// before handshake is confirmed. If TCP succeeds and QUIC succeeds on the
// alternate network, QUIC is marked as broken. The brokenness will expire when
// the default network changes.
TEST_P(QuicNetworkTransactionTest, RetryOnAlternateNetworkWhileTCPSucceeds) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 ||
      GetQuicReloadableFlag(quic_use_idle_network_detector)) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    // TODO(fayang): Add time driven idle network detection test.
    return;
  }

  SetUpTestForRetryConnectionOnAlternateNetwork();

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));  // CHLO
  // Retranmit the handshake messages.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  // After timeout, connection will be closed with QUIC_NETWORK_IDLE_TIMEOUT.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeConnectionClosePacket(
                         packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                         "No recent network activity after 4s. Timeout:4s"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // Add successful TCP data so that TCP job will succeed.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "TCP succeeds"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Quic connection will be retried on the alternate network after the initial
  // one fails on the default network.
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Handing read.
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_->MakeDummyCHLOPacket(1));  // CHLO

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(2));
  quic_data2.AddSocketDataToFactory(&socket_factory_);

  // Resolve the host resolution synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));
  // Add alternate protocol mapping to race QUIC and TCP.
  // QUIC connection requires handshake to be confirmed and sends CHLO to the
  // peer.
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  // Request will be served with TCP job.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckResponseData(&trans, "TCP succeeds");

  // Fast forward to idle timeout the original connection. A new connection will
  // be kicked off on the alternate network.
  quic_task_runner_->FastForwardBy(quic::QuicTime::Delta::FromSeconds(4));
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  // The second connection hasn't finish handshake, verify that QUIC is not
  // marked as broken.
  ExpectQuicAlternateProtocolMapping();
  // Explicitly confirm the handshake on the second connection.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Run message loop to execute posted tasks, which will notify JoController
  // about the orphaned job status.
  base::RunLoop().RunUntilIdle();

  // Verify that QUIC is marked as broken.
  ExpectBrokenAlternateProtocolMapping();

  // Deliver a message to notify the new network becomes default, the previous
  // brokenness will be clear as the brokenness is bond with old default
  // network.
  scoped_mock_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  ExpectQuicAlternateProtocolMapping();

  ASSERT_TRUE(quic_data2.AllReadDataConsumed());
  ASSERT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Much like above test, but verifies NetworkIsolationKeys are respected.
TEST_P(QuicNetworkTransactionTest,
       RetryOnAlternateNetworkWhileTCPSucceedsWithNetworkIsolationKey) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 ||
      GetQuicReloadableFlag(quic_use_idle_network_detector)) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    // TODO(fayang): Add time driven idle network detection test.
    return;
  }

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

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

  SetUpTestForRetryConnectionOnAlternateNetwork();

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));  // CHLO
  // Retranmit the handshake messages.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  // After timeout, connection will be closed with QUIC_NETWORK_IDLE_TIMEOUT.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeConnectionClosePacket(
                         packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                         "No recent network activity after 4s. Timeout:4s"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // Add successful TCP data so that TCP job will succeed.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "TCP succeeds"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Quic connection will be retried on the alternate network after the initial
  // one fails on the default network.
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Handing read.
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_->MakeDummyCHLOPacket(1));  // CHLO

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version))
    quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(2));
  quic_data2.AddSocketDataToFactory(&socket_factory_);

  // Resolve the host resolution synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));
  // Add alternate protocol mapping to race QUIC and TCP.
  // QUIC connection requires handshake to be confirmed and sends CHLO to the
  // peer.
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT, kNetworkIsolationKey1);
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT, kNetworkIsolationKey2);

  request_.network_isolation_key = kNetworkIsolationKey1;
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  // Request will be served with TCP job.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckResponseData(&trans, "TCP succeeds");

  // Fast forward to idle timeout the original connection. A new connection will
  // be kicked off on the alternate network.
  quic_task_runner_->FastForwardBy(quic::QuicTime::Delta::FromSeconds(4));
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  // The second connection hasn't finish handshake, verify that QUIC is not
  // marked as broken.
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);
  // Explicitly confirm the handshake on the second connection.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Run message loop to execute posted tasks, which will notify JoController
  // about the orphaned job status.
  base::RunLoop().RunUntilIdle();

  // Verify that QUIC is marked as broken for kNetworkIsolationKey1 only.
  ExpectBrokenAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);

  // Deliver a message to notify the new network becomes default, the previous
  // brokenness will be clear as the brokenness is bond with old default
  // network.
  scoped_mock_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);

  ASSERT_TRUE(quic_data2.AllReadDataConsumed());
  ASSERT_TRUE(quic_data2.AllWriteDataConsumed());
}

// This test verifies that a new QUIC connection will be attempted on the
// alternate network if the original QUIC connection fails with idle timeout
// before handshake is confirmed. If TCP doesn't succeed but QUIC on the
// alternative network succeeds, QUIC is not marked as broken.
TEST_P(QuicNetworkTransactionTest, RetryOnAlternateNetworkWhileTCPHanging) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3 ||
      GetQuicReloadableFlag(quic_use_idle_network_detector)) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    // TODO(fayang): Add time driven idle network detection test.
    return;
  }

  SetUpTestForRetryConnectionOnAlternateNetwork();

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));  // CHLO
  // Retranmit the handshake messages.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeDummyCHLOPacket(packet_num++));
  // After timeout, connection will be closed with QUIC_NETWORK_IDLE_TIMEOUT.
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_->MakeConnectionClosePacket(
                         packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                         "No recent network activity after 4s. Timeout:4s"));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // Add hanging TCP data so that TCP job will never succeeded.
  AddHangingNonAlternateProtocolSocketData();

  // Quic connection will then be retried on the alternate network.
  MockQuicData quic_data2(version_);
  packet_num = 1;
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeDummyCHLOPacket(packet_num++));  // CHLO

  const std::string body = "hello!";
  std::string header = ConstructDataHeader(body.length());

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data2.AddWrite(SYNCHRONOUS,
                        ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data2.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  quic_data2.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  quic_data2.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + body));
  quic_data2.AddWrite(SYNCHRONOUS,
                      ConstructClientAckPacket(packet_num++, 2, 1, 1));
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  quic_data2.AddSocketDataToFactory(&socket_factory_);

  // Resolve the host resolution synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));
  // Add alternate protocol mapping to race QUIC and TCP.
  // QUIC connection requires handshake to be confirmed and sends CHLO to the
  // peer.
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();

  // Fast forward to idle timeout the original connection. A new connection will
  // be kicked off on the alternate network.
  quic_task_runner_->FastForwardBy(quic::QuicTime::Delta::FromSeconds(4));
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  // Verify that QUIC is not marked as broken.
  ExpectQuicAlternateProtocolMapping();
  // Explicitly confirm the handshake on the second connection.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  // Read the response.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckResponseData(&trans, body);
  // Verify that QUIC is not marked as broken.
  ExpectQuicAlternateProtocolMapping();

  // Deliver a message to notify the new network becomes default.
  scoped_mock_change_notifier_->mock_network_change_notifier()
      ->NotifyNetworkMadeDefault(kNewNetworkForTests);
  ExpectQuicAlternateProtocolMapping();
  ASSERT_TRUE(quic_data2.AllReadDataConsumed());
  ASSERT_TRUE(quic_data2.AllWriteDataConsumed());
}

// Verify that if a QUIC connection times out, the QuicHttpStream will
// return QUIC_PROTOCOL_ERROR.
TEST_P(QuicNetworkTransactionTest, TimeoutAfterHandshakeConfirmed) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->idle_connection_timeout = base::TimeDelta::FromSeconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  client_maker_->set_save_packet_frames(true);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, priority, GetRequestHeaders("GET", "https", "/"), 0, nullptr));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  if (VersionUsesHttp3(version_.transport_version)) {
    // TLP 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // TLP 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));
    // RTO 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));
    // RTO 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));
    // RTO 3
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));

    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeConnectionClosePacket(
                           packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                           "No recent network activity after 4s. Timeout:4s"));
  } else {
    // Settings were sent in the request packet so there is only 1 packet to
    // retransmit.
    // TLP 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // TLP 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // RTO 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // RTO 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // RTO 3
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));

    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeConnectionClosePacket(
                           packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                           "No recent network activity after 4s. Timeout:4s"));
  }

  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  // Run the QUIC session to completion.
  quic_task_runner_->RunUntilIdle();

  ExpectQuicAlternateProtocolMapping();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

// TODO(fayang): Add time driven TOO_MANY_RTOS test.

// Verify that if a QUIC protocol error occurs after the handshake is confirmed
// the request fails with QUIC_PROTOCOL_ERROR.
TEST_P(QuicNetworkTransactionTest, ProtocolErrorAfterHandshakeConfirmed) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 1, false, GetNthClientInitiatedBidirectionalStreamId(47),
                 quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructClientAckAndConnectionClosePacket(
                         packet_num++, 1, 1, 1, quic::QUIC_INVALID_STREAM_ID,
                         quic_error_details, quic::IETF_RST_STREAM));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  ASSERT_FALSE(quic_data.AllReadDataConsumed());
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_TRUE(quic_data.AllReadDataConsumed());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  ExpectQuicAlternateProtocolMapping();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// connection times out, then QUIC will be marked as broken and the request
// retried over TCP.
TEST_P(QuicNetworkTransactionTest, TimeoutAfterHandshakeConfirmedThenBroken2) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->idle_connection_timeout = base::TimeDelta::FromSeconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  client_maker_->set_save_packet_frames(true);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, priority, GetRequestHeaders("GET", "https", "/"), 0, nullptr));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  if (VersionUsesHttp3(version_.transport_version)) {
    // TLP 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // TLP 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));
    // RTO 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));
    // RTO 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));
    // RTO 3
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        2, packet_num++, true));

    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeConnectionClosePacket(
                           packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                           "No recent network activity after 4s. Timeout:4s"));
  } else {
    // TLP 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // TLP 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // RTO 1
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // RTO 2
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));
    // RTO 3
    quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeRetransmissionPacket(
                                        1, packet_num++, true));

    quic_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeConnectionClosePacket(
                           packet_num++, true, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                           "No recent network activity after 4s. Timeout:4s"));
  }

  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, OK);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  // Run the QUIC session to completion.
  quic_task_runner_->RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, "hello world");
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// protocol error occurs after the handshake is confirmed, the request
// retried over TCP and the QUIC will be marked as broken.
TEST_P(QuicNetworkTransactionTest,
       ProtocolErrorAfterHandshakeConfirmedThenBroken) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->idle_connection_timeout = base::TimeDelta::FromSeconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 1, false, GetNthClientInitiatedBidirectionalStreamId(47),
                 quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructClientAckAndConnectionClosePacket(
                         packet_num++, 1, 1, 1, quic::QUIC_INVALID_STREAM_ID,
                         quic_error_details, quic::IETF_RST_STREAM));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, "hello world");
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Much like above test, but verifies that NetworkIsolationKey is respected.
TEST_P(QuicNetworkTransactionTest,
       ProtocolErrorAfterHandshakeConfirmedThenBrokenWithNetworkIsolationKey) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  context_.params()->idle_connection_timeout = base::TimeDelta::FromSeconds(5);

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  uint64_t packet_number = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_number++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 1, false, GetNthClientInitiatedBidirectionalStreamId(47),
                 quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  quic_data.AddWrite(SYNCHRONOUS,
                     ConstructClientAckAndConnectionClosePacket(
                         packet_number++, 1, 1, 1, quic::QUIC_INVALID_STREAM_ID,
                         quic_error_details, quic::IETF_RST_STREAM));
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkIsolationKey1);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkIsolationKey2);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  request_.network_isolation_key = kNetworkIsolationKey1;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, "hello world");
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());

  // The alternative service shouldhave been marked as broken under
  // kNetworkIsolationKey1 but not kNetworkIsolationKey2.
  ExpectBrokenAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);

  // Subsequent requests using kNetworkIsolationKey1 should not use QUIC.
  AddHttpDataAndRunRequest();
  // Requests using other NetworkIsolationKeys can still use QUIC.
  request_.network_isolation_key = kNetworkIsolationKey2;
  AddQuicDataAndRunRequest();

  // The last two requests should not have changed the alternative service
  // mappings.
  ExpectBrokenAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// request is reset from, then QUIC will be marked as broken and the request
// retried over TCP.
TEST_P(QuicNetworkTransactionTest, ResetAfterHandshakeConfirmedThenBroken) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  // The request will initially go out over QUIC.
  MockQuicData quic_data(version_);
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructInitialSettingsPacket(packet_num++));
  }
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, priority, GetRequestHeaders("GET", "https", "/"), 0, nullptr));

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause

  quic_data.AddRead(ASYNC,
                    ConstructServerRstPacket(
                        1, false, GetNthClientInitiatedBidirectionalStreamId(0),
                        quic::QUIC_HEADERS_TOO_LARGE));

  if (VersionUsesHttp3(version_.transport_version)) {
    quic_data.AddWrite(SYNCHRONOUS,
                       ConstructClientDataPacket(
                           packet_num++, GetQpackDecoderStreamId(), true, false,
                           StreamCancellationQpackDecoderInstruction(0)));
  }

  quic_data.AddRead(ASYNC, OK);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Pump the message loop to get the request started.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  quic_data.Resume();

  // Run the QUIC session to completion.
  ASSERT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectQuicAlternateProtocolMapping();

  // Let the transaction proceed which will result in QUIC being marked
  // as broken and the request falling back to TCP.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  ASSERT_TRUE(quic_data.AllWriteDataConsumed());
  ASSERT_FALSE(http_data.AllReadDataConsumed());

  // Read the response body over TCP.
  CheckResponseData(&trans, "hello world");
  ExpectBrokenAlternateProtocolMapping();
  ASSERT_TRUE(http_data.AllWriteDataConsumed());
  ASSERT_TRUE(http_data.AllReadDataConsumed());
}

// Verify that when an origin has two alt-svc advertisements, one local and one
// remote, that when the local is broken the request will go over QUIC via
// the remote Alt-Svc.
// This is a regression test for crbug/825646.
TEST_P(QuicNetworkTransactionTest, RemoteAltSvcWorkingWhileLocalAltSvcBroken) {
  context_.params()->allow_remote_alt_svc = true;

  GURL origin1 = request_.url;  // mail.example.org
  GURL origin2("https://www.example.org/");
  ASSERT_NE(origin1.host(), origin2.host());

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch("www.example.org"));
  ASSERT_TRUE(cert->VerifyNameMatch("mail.example.org"));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  MockQuicData mock_quic_data2(version_);
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  // Set up alternative service for |origin1|.
  AlternativeService local_alternative(kProtoQUIC, "mail.example.org", 443);
  AlternativeService remote_alternative(kProtoQUIC, "www.example.org", 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  AlternativeServiceInfoVector alternative_services;
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          local_alternative, expiration,
          context_.params()->supported_versions));
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          remote_alternative, expiration,
          context_.params()->supported_versions));
  http_server_properties_->SetAlternativeServices(url::SchemeHostPort(origin1),
                                                  NetworkIsolationKey(),
                                                  alternative_services);

  http_server_properties_->MarkAlternativeServiceBroken(local_alternative,
                                                        NetworkIsolationKey());

  SendRequestAndExpectQuicResponse("hello!");
}

// Verify that when multiple alternatives are broken,
// ALTERNATE_PROTOCOL_USAGE_BROKEN is only logged once.
// This is a regression test for crbug/1024613.
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeOnlyRecordedOnce) {
  base::HistogramTester histogram_tester;

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  GURL origin1 = request_.url;  // mail.example.org

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch("mail.example.org"));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  CreateSession();

  // Set up alternative service for |origin1|.
  AlternativeService local_alternative(kProtoQUIC, "mail.example.org", 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  AlternativeServiceInfoVector alternative_services;
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          local_alternative, expiration,
          context_.params()->supported_versions));
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          local_alternative, expiration,
          context_.params()->supported_versions));
  http_server_properties_->SetAlternativeServices(url::SchemeHostPort(origin1),
                                                  NetworkIsolationKey(),
                                                  alternative_services);

  http_server_properties_->MarkAlternativeServiceBroken(local_alternative,
                                                        NetworkIsolationKey());

  SendRequestAndExpectHttpResponse("hello world");

  histogram_tester.ExpectBucketCount("Net.AlternateProtocolUsage",
                                     ALTERNATE_PROTOCOL_USAGE_BROKEN, 1);
}

// Verify that with retry_without_alt_svc_on_quic_errors enabled, if a QUIC
// request is reset from, then QUIC will be marked as broken and the request
// retried over TCP. Then, subsequent requests will go over a new TCP
// connection instead of going back to the broken QUIC connection.
// This is a regression tests for crbug/731303.
TEST_P(QuicNetworkTransactionTest,
       ResetPooledAfterHandshakeConfirmedThenBroken) {
  context_.params()->allow_remote_alt_svc = true;

  GURL origin1 = request_.url;
  GURL origin2("https://www.example.org/");
  ASSERT_NE(origin1.host(), origin2.host());

  MockQuicData mock_quic_data(version_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch("www.example.org"));
  ASSERT_TRUE(cert->VerifyNameMatch("mail.example.org"));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  // First request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  // Second request will go over the pooled QUIC connection, but will be
  // reset by the server.
  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_);
  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_SERVER, false);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/", &client_maker2),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 3, false, GetNthClientInitiatedBidirectionalStreamId(1),
                 quic::QUIC_HEADERS_TOO_LARGE));

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(1)));
  }

  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // After that fails, it will be resent via TCP.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: www.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};
  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // Then the next request to the second origin will be sent over TCP.
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  // Set up alternative service for |origin1|.
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  AlternativeService alternative1(kProtoQUIC, origin1.host(), 443);
  http_server_properties_->SetQuicAlternativeService(
      url::SchemeHostPort(origin1), NetworkIsolationKey(), alternative1,
      expiration, supported_versions_);

  // Set up alternative service for |origin2|.
  AlternativeService alternative2(kProtoQUIC, origin2.host(), 443);
  http_server_properties_->SetQuicAlternativeService(
      url::SchemeHostPort(origin2), NetworkIsolationKey(), alternative2,
      expiration, supported_versions_);

  // First request opens connection to |destination1|
  // with quic::QuicServerId.host() == origin1.host().
  SendRequestAndExpectQuicResponse("hello!");

  // Second request pools to existing connection with same destination,
  // because certificate matches, even though quic::QuicServerId is different.
  // After it is reset, it will fail back to TCP and mark QUIC as broken.
  request_.url = origin2;
  SendRequestAndExpectHttpResponse("hello world");
  EXPECT_FALSE(http_server_properties_->IsAlternativeServiceBroken(
      alternative1, NetworkIsolationKey()))
      << alternative1.ToString();
  EXPECT_TRUE(http_server_properties_->IsAlternativeServiceBroken(
      alternative2, NetworkIsolationKey()))
      << alternative2.ToString();

  // The third request should use a new TCP connection, not the broken
  // QUIC connection.
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest,
       DoNotUseAlternativeServiceQuicUnsupportedVersion) {
  std::string altsvc_header =
      base::StringPrintf("Alt-Svc: quic=\":443\"; v=\"%u\"\r\n\r\n",
                         version_.transport_version - 1);
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(altsvc_header.c_str()),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

// When multiple alternative services are advertised, HttpStreamFactory should
// select the alternative service which uses existing QUIC session if available.
// If no existing QUIC session can be used, use the first alternative service
// from the list.
TEST_P(QuicNetworkTransactionTest, UseExistingAlternativeServiceForQuic) {
  context_.params()->allow_remote_alt_svc = true;
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\"foo.example.org:443\", quic=\":444\"\r\n\r\n"),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // First QUIC request data.
  // Open a session to foo.example.org:443 using the first entry of the
  // alternative service list.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));

  std::string alt_svc_list =
      "quic=\"mail.example.org:444\", quic=\"foo.example.org:443\", "
      "quic=\"bar.example.org:445\"";
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK", alt_svc_list)));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  // Second QUIC request data.
  // Connection pooling, using existing session, no need to include version
  // as version negotiation has been completed.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(packet_num++, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  SendRequestAndExpectHttpResponse("hello world");

  SendRequestAndExpectQuicResponse("hello!");
  SendRequestAndExpectQuicResponse("hello!");
}

// Check that an existing QUIC connection to an alternative proxy server is
// used.
TEST_P(QuicNetworkTransactionTest, UseExistingQUICAlternativeProxy) {
  base::HistogramTester histogram_tester;

  // First QUIC request data.
  // Open a session to foo.example.org:443 using the first entry of the
  // alternative service list.
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "http", "/")));

  std::string alt_svc_list;
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK", alt_svc_list)));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  // Second QUIC request data.
  // Connection pooling, using existing session, no need to include version
  // as version negotiation has been completed.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "http", "/"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(packet_num++, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();

  TestProxyDelegate test_proxy_delegate;

  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS mail.example.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  test_proxy_delegate.set_alternative_proxy_server(
      ProxyServer::FromPacString("QUIC mail.example.org:443"));
  proxy_resolution_service_->SetProxyDelegate(&test_proxy_delegate);

  request_.url = GURL("http://mail.example.org/");

  CreateSession();
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 443);
  histogram_tester.ExpectUniqueSample("Net.QuicAlternativeProxy.Usage",
                                      1 /* ALTERNATIVE_PROXY_USAGE_WON_RACE */,
                                      1);

  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 443);
  histogram_tester.ExpectTotalCount("Net.QuicAlternativeProxy.Usage", 2);
  histogram_tester.ExpectBucketCount("Net.QuicAlternativeProxy.Usage",
                                     0 /* ALTERNATIVE_PROXY_USAGE_NO_RACE */,
                                     1);
}

// Pool to existing session with matching quic::QuicServerId
// even if alternative service destination is different.
TEST_P(QuicNetworkTransactionTest, PoolByOrigin) {
  context_.params()->allow_remote_alt_svc = true;
  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  // First request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  // Second request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(packet_num++, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  const char destination1[] = "first.example.com";
  const char destination2[] = "second.example.com";

  // Set up alternative service entry to destination1.
  url::SchemeHostPort server(request_.url);
  AlternativeService alternative_service(kProtoQUIC, destination1, 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkIsolationKey(), alternative_service, expiration,
      supported_versions_);
  // First request opens connection to |destination1|
  // with quic::QuicServerId.host() == kDefaultServerHostName.
  SendRequestAndExpectQuicResponse("hello!");

  // Set up alternative service entry to a different destination.
  alternative_service = AlternativeService(kProtoQUIC, destination2, 443);
  http_server_properties_->SetQuicAlternativeService(
      server, NetworkIsolationKey(), alternative_service, expiration,
      supported_versions_);
  // Second request pools to existing connection with same quic::QuicServerId,
  // even though alternative service destination is different.
  SendRequestAndExpectQuicResponse("hello!");
}

// Pool to existing session with matching destination and matching certificate
// even if origin is different, and even if the alternative service with
// matching destination is not the first one on the list.
TEST_P(QuicNetworkTransactionTest, PoolByDestination) {
  context_.params()->allow_remote_alt_svc = true;
  GURL origin1 = request_.url;
  GURL origin2("https://www.example.org/");
  ASSERT_NE(origin1.host(), origin2.host());

  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  // First request.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  // Second request.
  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_);
  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2.host(), quic::Perspective::IS_SERVER, false);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/", &client_maker2),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(packet_num++, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  const char destination1[] = "first.example.com";
  const char destination2[] = "second.example.com";

  // Set up alternative service for |origin1|.
  AlternativeService alternative_service1(kProtoQUIC, destination1, 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  http_server_properties_->SetQuicAlternativeService(
      url::SchemeHostPort(origin1), NetworkIsolationKey(), alternative_service1,
      expiration, supported_versions_);

  // Set up multiple alternative service entries for |origin2|,
  // the first one with a different destination as for |origin1|,
  // the second one with the same.  The second one should be used,
  // because the request can be pooled to that one.
  AlternativeService alternative_service2(kProtoQUIC, destination2, 443);
  AlternativeServiceInfoVector alternative_services;
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service2, expiration,
          context_.params()->supported_versions));
  alternative_services.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service1, expiration,
          context_.params()->supported_versions));
  http_server_properties_->SetAlternativeServices(url::SchemeHostPort(origin2),
                                                  NetworkIsolationKey(),
                                                  alternative_services);
  // First request opens connection to |destination1|
  // with quic::QuicServerId.host() == origin1.host().
  SendRequestAndExpectQuicResponse("hello!");

  // Second request pools to existing connection with same destination,
  // because certificate matches, even though quic::QuicServerId is different.
  request_.url = origin2;

  SendRequestAndExpectQuicResponse("hello!");
}

// Multiple origins have listed the same alternative services. When there's a
// existing QUIC session opened by a request to other origin,
// if the cert is valid, should select this QUIC session to make the request
// if this is also the first existing QUIC session.
TEST_P(QuicNetworkTransactionTest,
       UseSharedExistingAlternativeServiceForQuicWithValidCert) {
  context_.params()->allow_remote_alt_svc = true;
  // Default cert is valid for *.example.org

  // HTTP data for request to www.example.org.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":443\"\r\n\r\n"),
      MockRead("hello world from www.example.org"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // HTTP data for request to mail.example.org.
  MockRead http_reads2[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead("Alt-Svc: quic=\":444\", quic=\"www.example.org:443\"\r\n\r\n"),
      MockRead("hello world from mail.example.org"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data2(http_reads2, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  QuicTestPacketMaker client_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), "mail.example.org", quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_);
  server_maker_.set_hostname("www.example.org");
  client_maker_->set_hostname("www.example.org");
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  // First QUIC request data.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(21);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello from mail QUIC!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  // Second QUIC request data.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/", &client_maker),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header + "hello from mail QUIC!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(packet_num++, 4, 3, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  // Send two HTTP requests, responses set up alt-svc lists for the origins.
  request_.url = GURL("https://www.example.org/");
  SendRequestAndExpectHttpResponse("hello world from www.example.org");
  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectHttpResponse("hello world from mail.example.org");

  // Open a QUIC session to mail.example.org:443 when making request
  // to mail.example.org.
  request_.url = GURL("https://www.example.org/");
  SendRequestAndExpectQuicResponse("hello from mail QUIC!");

  // Uses the existing QUIC session when making request to www.example.org.
  request_.url = GURL("https://mail.example.org/");
  SendRequestAndExpectQuicResponse("hello from mail QUIC!");
}

TEST_P(QuicNetworkTransactionTest, AlternativeServiceDifferentPort) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"),
      MockRead(kQuicAlternativeServiceDifferentPortHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");

  url::SchemeHostPort http_server("https", kDefaultServerHostName, 443);
  AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_properties_->GetAlternativeServiceInfos(
          http_server, NetworkIsolationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  const AlternativeService alternative_service =
      alternative_service_info_vector[0].alternative_service();
  EXPECT_EQ(kProtoQUIC, alternative_service.protocol);
  EXPECT_EQ(kDefaultServerHostName, alternative_service.host);
  EXPECT_EQ(137, alternative_service.port);
}

TEST_P(QuicNetworkTransactionTest, ConfirmAlternativeService) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  AlternativeService alternative_service(kProtoQUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey());
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  mock_quic_data.Resume();

  EXPECT_FALSE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_NE(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         NetworkIsolationKey()));
}

TEST_P(QuicNetworkTransactionTest,
       ConfirmAlternativeServiceWithNetworkIsolationKey) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  AlternativeService alternative_service(kProtoQUIC,
                                         HostPortPair::FromURL(request_.url));
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkIsolationKey1);
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkIsolationKey2);
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkIsolationKey1));
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkIsolationKey2));

  request_.network_isolation_key = kNetworkIsolationKey1;
  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");

  mock_quic_data.Resume();

  EXPECT_FALSE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkIsolationKey1));
  EXPECT_NE(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         kNetworkIsolationKey1));
  EXPECT_TRUE(http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, kNetworkIsolationKey2));
  EXPECT_EQ(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         kNetworkIsolationKey2));
}

TEST_P(QuicNetworkTransactionTest, UseAlternativeServiceForQuicForHttps) {
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, 0);  // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  // TODO(rtenneti): Test QUIC over HTTPS, GetSSLInfo().
  SendRequestAndExpectHttpResponse("hello world");
}

// Tests that the connection to an HTTPS proxy is raced with an available
// alternative proxy server.
TEST_P(QuicNetworkTransactionTest, QuicProxyWithRacing) {
  base::HistogramTester histogram_tester;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS mail.example.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "http", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // There is no need to set up main job, because no attempt will be made to
  // speak to the proxy over TCP.
  request_.url = GURL("http://mail.example.org/");
  TestProxyDelegate test_proxy_delegate;
  const HostPortPair host_port_pair("mail.example.org", 443);

  test_proxy_delegate.set_alternative_proxy_server(
      ProxyServer::FromPacString("QUIC mail.example.org:443"));
  proxy_resolution_service_->SetProxyDelegate(&test_proxy_delegate);
  CreateSession();
  EXPECT_TRUE(test_proxy_delegate.alternative_proxy_server().is_quic());

  // The main job needs to hang in order to guarantee that the alternative
  // proxy server job will "win".
  AddHangingNonAlternateProtocolSocketData();

  SendRequestAndExpectQuicResponseFromProxyOnPort("hello!", 443);

  // Verify that the alternative proxy server is not marked as broken.
  EXPECT_TRUE(test_proxy_delegate.alternative_proxy_server().is_quic());

  // Verify that the proxy server is not marked as broken.
  EXPECT_TRUE(session_->proxy_resolution_service()->proxy_retry_info().empty());

  histogram_tester.ExpectUniqueSample("Net.QuicAlternativeProxy.Usage",
                                      1 /* ALTERNATIVE_PROXY_USAGE_WON_RACE */,
                                      1);
}

TEST_P(QuicNetworkTransactionTest, HungAlternativeService) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};

  SequencedSocketData http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // The QUIC transaction will not be allowed to complete.
  MockWrite quic_writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  MockRead quic_reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };
  SequencedSocketData quic_data(quic_reads, quic_writes);
  socket_factory_.AddSocketDataProvider(&quic_data);

  // The HTTP transaction will complete.
  SequencedSocketData http_data2(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data2);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  // Run the first request.
  SendRequestAndExpectHttpResponse("hello world");
  ASSERT_TRUE(http_data.AllReadDataConsumed());
  ASSERT_TRUE(http_data.AllWriteDataConsumed());

  // Now run the second request in which the QUIC socket hangs,
  // and verify the the transaction continues over HTTP.
  SendRequestAndExpectHttpResponse("hello world");
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(http_data2.AllReadDataConsumed());
  ASSERT_TRUE(http_data2.AllWriteDataConsumed());
  ASSERT_TRUE(quic_data.AllReadDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithHttpRace) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");

  EXPECT_EQ(nullptr, http_server_properties_->GetServerNetworkStats(
                         url::SchemeHostPort("https", request_.url.host(), 443),
                         NetworkIsolationKey()));
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithNoHttpRace) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakeInitialSettingsPacket(packet_number++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_number++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithProxy) {
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "PROXY myproxy:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  // Since we are using a proxy, the QUIC job will not succeed.
  MockWrite http_writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET http://mail.example.org/ HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: mail.example.org\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Proxy-Connection: keep-alive\r\n\r\n")};

  MockRead http_reads[] = {
      MockRead(SYNCHRONOUS, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 4, kQuicAlternativeServiceHeader),
      MockRead(SYNCHRONOUS, 5, "hello world"), MockRead(SYNCHRONOUS, OK, 6)};

  StaticSocketDataProvider http_data(http_reads, http_writes);
  socket_factory_.AddSocketDataProvider(&http_data);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  request_.url = GURL("http://mail.example.org/");
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  SendRequestAndExpectHttpResponse("hello world");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithConfirmationRequired) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, "hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithTooEarlyResponse) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  uint64_t packet_number = 1;
  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakeInitialSettingsPacket(packet_number++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("425 TOO_EARLY")));
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            client_maker_->MakeRstPacket(
                                packet_number++, false,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                quic::QUIC_STREAM_CANCELLED));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndRstPacket(
            packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndConnectionClosePacket(packet_number++, 3, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Confirm the handshake after the 425 Too Early.
  base::RunLoop().RunUntilIdle();

  // The handshake hasn't been confirmed yet, so the retry should not have
  // succeeded.
  EXPECT_FALSE(callback.have_result());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, "hello!");
}

TEST_P(QuicNetworkTransactionTest, ZeroRTTWithMultipleTooEarlyResponse) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  uint64_t packet_number = 1;
  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakeInitialSettingsPacket(packet_number++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("425 TOO_EARLY")));
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            client_maker_->MakeRstPacket(
                                packet_number++, false,
                                GetNthClientInitiatedBidirectionalStreamId(0),
                                quic::QUIC_STREAM_CANCELLED));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndRstPacket(
            packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }

  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          true, GetRequestHeaders("GET", "https", "/"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("425 TOO_EARLY")));
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_number++, false, GetQpackDecoderStreamId(), 2, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(1)));
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            client_maker_->MakeRstPacket(
                                packet_number++, false,
                                GetNthClientInitiatedBidirectionalStreamId(1),
                                quic::QUIC_STREAM_CANCELLED));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndRstPacket(
            packet_number++, GetNthClientInitiatedBidirectionalStreamId(1),
            quic::QUIC_STREAM_CANCELLED, 2, 1, 1));
  }
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner_.get(),
                                                 context_.clock()));

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Confirm the handshake after the 425 Too Early.
  base::RunLoop().RunUntilIdle();

  // The handshake hasn't been confirmed yet, so the retry should not have
  // succeeded.
  EXPECT_FALSE(callback.have_result());

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  const HttpResponseInfo* response = trans.GetResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_TRUE(response->headers.get() != nullptr);
  EXPECT_EQ("HTTP/1.1 425 TOO_EARLY", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version_),
            response->connection_info);
}

TEST_P(QuicNetworkTransactionTest,
       LogGranularQuicErrorCodeOnQuicProtocolErrorLocal) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  // Read a close connection packet with
  // quic::QuicErrorCode: quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED from the peer.
  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));

  NetErrorDetails details;
  EXPECT_EQ(quic::QUIC_NO_ERROR, details.quic_connection_error);

  trans.PopulateNetErrorDetails(&details);
  // Verify the error code logged is what sent by the peer.
  EXPECT_EQ(quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED,
            details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest,
       LogGranularQuicErrorCodeOnQuicProtocolErrorRemote) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  // Peer sending data from an non-existing stream causes this end to raise
  // error and close connection.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 1, false, GetNthClientInitiatedBidirectionalStreamId(47),
                 quic::QUIC_STREAM_LAST_ERROR));
  std::string quic_error_details = "Data for nonexistent stream";
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckAndConnectionClosePacket(
                       packet_num++, 1, 1, 1, quic::QUIC_INVALID_STREAM_ID,
                       quic_error_details, quic::IETF_RST_STREAM));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  NetErrorDetails details;
  EXPECT_EQ(quic::QUIC_NO_ERROR, details.quic_connection_error);

  trans.PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QUIC_INVALID_STREAM_ID, details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest, RstStreamErrorHandling) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  // Read the response headers, then a RST_STREAM frame.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 2, false, GetNthClientInitiatedBidirectionalStreamId(0),
                 quic::QUIC_STREAM_CANCELLED));

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 2, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
  } else {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructClientAckPacket(packet_num++, 2, 1, 1));
  }
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Read the headers.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  const HttpResponseInfo* response = trans.GetResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_TRUE(response->headers.get() != nullptr);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_alpn_negotiated);
  EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version_),
            response->connection_info);

  std::string response_data;
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR, ReadTransaction(&trans, &response_data));
}

TEST_P(QuicNetworkTransactionTest, RstStreamBeforeHeaders) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerRstPacket(
                 1, false, GetNthClientInitiatedBidirectionalStreamId(0),
                 quic::QUIC_STREAM_CANCELLED));

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }

  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.  Of course, even though QUIC *could* perform a 0-RTT
  // connection to the the server, in this test we require confirmation
  // before encrypting so the HTTP job will still start.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  // Read the headers.
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocol) {
  // Alternate-protocol job
  std::unique_ptr<quic::QuicEncryptedPacket> close(
      ConstructServerConnectionClosePacket(1));
  MockRead quic_reads[] = {
      MockRead(ASYNC, close->data(), close->length()),
      MockRead(ASYNC, ERR_IO_PENDING),  // No more data to read
      MockRead(ASYNC, OK),              // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest,
       BrokenAlternateProtocolWithNetworkIsolationKey) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  // Alternate-protocol job
  std::unique_ptr<quic::QuicEncryptedPacket> close(
      ConstructServerConnectionClosePacket(1));
  MockRead quic_reads[] = {
      MockRead(ASYNC, close->data(), close->length()),
      MockRead(ASYNC, ERR_IO_PENDING),  // No more data to read
      MockRead(ASYNC, OK),              // EOF
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT, kNetworkIsolationKey1);
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT, kNetworkIsolationKey2);
  request_.network_isolation_key = kNetworkIsolationKey1;
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolReadError) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");
  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, NoBrokenAlternateProtocolIfTcpFails) {
  // Alternate-protocol job will fail when the session attempts to read.
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job will also fail.
  MockRead http_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_SOCKET_NOT_CONNECTED));
  ExpectQuicAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, DelayTCPOnStartWithQuicSupportOnSameIP) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  // Tests that TCP job is delayed and QUIC job does not require confirmation
  // if QUIC was recently supported on the same IP on start.

  // Set QUIC support on the last IP address, which is same with the local IP
  // address. Require confirmation mode will be turned off immediately when
  // local IP address is sorted out after we configure the UDP socket.
  http_server_properties_->SetLastLocalAddressWhenQuicWorked(
      IPAddress(192, 0, 2, 33));

  MockQuicData mock_quic_data(version_);
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  int packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_number++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_number++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_number++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  // No HTTP data is mocked as TCP job never starts in this case.

  CreateSession();
  // QuicStreamFactory by default requires confirmation on construction.
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  // Stall host resolution so that QUIC job will not succeed synchronously.
  // Socket will not be configured immediately and QUIC support is not sorted
  // out, TCP job will still be delayed as server properties indicates QUIC
  // support on last IP address.
  host_resolver_.set_synchronous_mode(false);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  EXPECT_THAT(trans.Start(&request_, callback.callback(), net_log_.bound()),
              IsError(ERR_IO_PENDING));
  // Complete host resolution in next message loop so that QUIC job could
  // proceed.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, "hello!");
}

TEST_P(QuicNetworkTransactionTest,
       DelayTCPOnStartWithQuicSupportOnDifferentIP) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  // Tests that TCP job is delayed and QUIC job requires confirmation if QUIC
  // was recently supported on a different IP address on start.

  // Set QUIC support on the last IP address, which is different with the local
  // IP address. Require confirmation mode will remain when local IP address is
  // sorted out after we configure the UDP socket.
  http_server_properties_->SetLastLocalAddressWhenQuicWorked(
      IPAddress(1, 2, 3, 4));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  client_maker_->SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  // No HTTP data is mocked as TCP job will be delayed and never starts.

  CreateSession();
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  // Stall host resolution so that QUIC job could not proceed and unblocks TCP.
  // Socket will not be configured immediately and QUIC support is not sorted
  // out, TCP job will still be delayed as server properties indicates QUIC
  // support on last IP address.
  host_resolver_.set_synchronous_mode(false);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  EXPECT_THAT(trans.Start(&request_, callback.callback(), net_log_.bound()),
              IsError(ERR_IO_PENDING));

  // Complete host resolution in next message loop so that QUIC job could
  // proceed.
  base::RunLoop().RunUntilIdle();
  // Explicitly confirm the handshake so that QUIC job could succeed.
  crypto_client_stream_factory_.last_stream()
      ->NotifySessionOneRttKeyAvailable();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  CheckWasQuicResponse(&trans);
  CheckResponseData(&trans, "hello!");
}

TEST_P(QuicNetworkTransactionTest, NetErrorDetailsSetBeforeHandshake) {
  // Test that NetErrorDetails is correctly populated, even if the
  // handshake has not yet been confirmed and no stream has been created.

  // QUIC job will pause. When resumed, it will fail.
  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // Main job will also fail.
  MockRead http_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  http_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  // Require handshake confirmation to ensure that no QUIC streams are
  // created, and to ensure that the TCP job does not wait for the QUIC
  // job to fail before it starts.
  session_->quic_stream_factory()->set_is_quic_known_to_work_on_current_network(
      false);

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Allow the TCP job to fail.
  base::RunLoop().RunUntilIdle();
  // Now let the QUIC job fail.
  mock_quic_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  ExpectQuicAlternateProtocolMapping();
  NetErrorDetails details;
  trans.PopulateNetErrorDetails(&details);
  EXPECT_EQ(quic::QUIC_PACKET_READ_ERROR, details.quic_connection_error);
}

TEST_P(QuicNetworkTransactionTest, FailedZeroRttBrokenAlternateProtocol) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest,
       FailedZeroRttBrokenAlternateProtocolWithNetworkIsolationKey) {
  if (version_.handshake_protocol == quic::PROTOCOL_TLS1_3) {
    // QUIC with TLS1.3 handshake doesn't support 0-rtt.
    return;
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       features::kPartitionConnectionsByNetworkIsolationKey},
      // disabled_features
      {});
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  http_server_properties_ = std::make_unique<HttpServerProperties>();

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(ASYNC, ERR_SOCKET_NOT_CONNECTED),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Second Alternate-protocol job which will race with the TCP job.
  StaticSocketDataProvider quic_data2(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data2);

  // Final job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkIsolationKey1);
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT,
                                  kNetworkIsolationKey2);

  request_.network_isolation_key = kNetworkIsolationKey1;
  SendRequestAndExpectHttpResponse("hello from http");
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());

  ExpectBrokenAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);

  // Subsequent requests using kNetworkIsolationKey1 should not use QUIC.
  AddHttpDataAndRunRequest();
  // Requests using other NetworkIsolationKeys can still use QUIC.
  request_.network_isolation_key = kNetworkIsolationKey2;
  AddQuicDataAndRunRequest();

  // The last two requests should not have changed the alternative service
  // mappings.
  ExpectBrokenAlternateProtocolMapping(kNetworkIsolationKey1);
  ExpectQuicAlternateProtocolMapping(kNetworkIsolationKey2);
}

TEST_P(QuicNetworkTransactionTest, DISABLED_HangingZeroRttFallback) {
  // Alternate-protocol job
  MockRead quic_reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING),
  };
  StaticSocketDataProvider quic_data(quic_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job that will proceed when the QUIC job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  AddQuicAlternateProtocolMapping(MockCryptoClientStream::ZERO_RTT);

  SendRequestAndExpectHttpResponse("hello from http");
}

TEST_P(QuicNetworkTransactionTest, BrokenAlternateProtocolOnConnectFailure) {
  // Alternate-protocol job will fail before creating a QUIC session.
  StaticSocketDataProvider quic_data;
  quic_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&quic_data);

  // Main job which will succeed even though the alternate job fails.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead("hello from http"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::COLD_START);
  SendRequestAndExpectHttpResponse("hello from http");

  ExpectBrokenAlternateProtocolMapping();
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnect) {
  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeDummyCHLOPacket(1));
  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("mail.example.org", "192.168.0.1",
                                           "");

  CreateSession();
  // TODO(rch): Check if we need a 0RTT version of ConnectionCloseDuringConnect
  AddQuicAlternateProtocolMapping(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  SendRequestAndExpectHttpResponse("hello world");
}

// For an alternative proxy that supports QUIC, test that the request is
// successfully fetched by the main job when the alternate proxy job encounters
// an error.
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxySocketNotConnected) {
  TestAlternativeProxy(ERR_SOCKET_NOT_CONNECTED);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyConnectionFailed) {
  TestAlternativeProxy(ERR_CONNECTION_FAILED);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyConnectionTimedOut) {
  TestAlternativeProxy(ERR_CONNECTION_TIMED_OUT);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyConnectionRefused) {
  TestAlternativeProxy(ERR_CONNECTION_REFUSED);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyQuicHandshakeFailed) {
  TestAlternativeProxy(ERR_QUIC_HANDSHAKE_FAILED);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyQuicProtocolError) {
  TestAlternativeProxy(ERR_QUIC_PROTOCOL_ERROR);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyIOPending) {
  TestAlternativeProxy(ERR_IO_PENDING);
}
TEST_P(QuicNetworkTransactionTest, BrokenAlternativeProxyAddressUnreachable) {
  TestAlternativeProxy(ERR_ADDRESS_UNREACHABLE);
}

TEST_P(QuicNetworkTransactionTest, ConnectionCloseDuringConnectProxy) {
  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_->MakeDummyCHLOPacket(1));
  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(1));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                       1, GetNthClientInitiatedBidirectionalStreamId(0), true,
                       true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddWrite(SYNCHRONOUS, ConstructClientAckPacket(2, 1, 1, 1));
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // When the QUIC connection fails, we will try the request again over HTTP.
  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  TestProxyDelegate test_proxy_delegate;
  const HostPortPair host_port_pair("myproxy.org", 443);
  test_proxy_delegate.set_alternative_proxy_server(
      ProxyServer::FromPacString("QUIC myproxy.org:443"));
  EXPECT_TRUE(test_proxy_delegate.alternative_proxy_server().is_quic());

  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);
  proxy_resolution_service_->SetProxyDelegate(&test_proxy_delegate);
  request_.url = GURL("http://mail.example.org/");

  // In order for a new QUIC session to be established via alternate-protocol
  // without racing an HTTP connection, we need the host resolution to happen
  // synchronously.
  host_resolver_.set_synchronous_mode(true);
  host_resolver_.rules()->AddIPLiteralRule("myproxy.org", "192.168.0.1", "");

  CreateSession();
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);
  SendRequestAndExpectHttpResponseFromProxy("hello world", true, 443);
  EXPECT_THAT(session_->proxy_resolution_service()->proxy_retry_info(),
              ElementsAre(Key("quic://myproxy.org:443")));
}

TEST_P(QuicNetworkTransactionTest, SecureResourceOverSecureQuic) {
  client_maker_->set_hostname("www.example.org");
  EXPECT_FALSE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  request_.url = GURL("https://www.example.org:443");
  AddHangingNonAlternateProtocolSocketData();
  CreateSession();
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  SendRequestAndExpectQuicResponse("hello!");
  EXPECT_TRUE(
      test_socket_performance_watcher_factory_.rtt_notification_received());
}

// TODO(zhongyi): disabled this broken test as it was not testing the correct
// code path. Need a fix to re-enable this test, tracking at crbug.com/704596.
TEST_P(QuicNetworkTransactionTest,
       DISABLED_QuicUploadToAlternativeProxyServer) {
  base::HistogramTester histogram_tester;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS mail.example.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  TestProxyDelegate test_proxy_delegate;

  test_proxy_delegate.set_alternative_proxy_server(
      ProxyServer::FromPacString("QUIC mail.example.org:443"));
  proxy_resolution_service_->SetProxyDelegate(&test_proxy_delegate);

  request_.url = GURL("http://mail.example.org/");

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_FAILED, 1)};
  SequencedSocketData socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData("1", 1, true);

  request_.upload_data_stream = &upload_data;

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_NE(OK, callback.WaitForResult());

  // Verify that the alternative proxy server is not marked as broken.
  EXPECT_TRUE(test_proxy_delegate.alternative_proxy_server().is_quic());

  // Verify that the proxy server is not marked as broken.
  EXPECT_TRUE(session_->proxy_resolution_service()->proxy_retry_info().empty());

  histogram_tester.ExpectUniqueSample("Net.QuicAlternativeProxy.Usage",
                                      1 /* ALTERNATIVE_PROXY_USAGE_WON_RACE */,
                                      1);
}

TEST_P(QuicNetworkTransactionTest, QuicUpload) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  if (!VersionUsesHttp3(version_.transport_version))
    mock_quic_data.AddRead(SYNCHRONOUS, OK);
  else
    mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  /*MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  //MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_FAILED, 1)};
  MockWrite writes[] = {};
  SequencedSocketData socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);*/

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData("1", 1, true);

  request_.upload_data_stream = &upload_data;

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_NE(OK, callback.WaitForResult());
}

TEST_P(QuicNetworkTransactionTest, QuicUploadWriteError) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  ScopedMockNetworkChangeNotifier network_change_notifier;
  MockNetworkChangeNotifier* mock_ncn =
      network_change_notifier.mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  context_.params()->migrate_sessions_on_network_change_v2 = true;

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false, GetRequestHeaders("POST", "https", "/")));
  socket_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  socket_data.AddSocketDataToFactory(&socket_factory_);

  MockQuicData socket_data2(version_);
  socket_data2.AddConnect(SYNCHRONOUS, ERR_ADDRESS_INVALID);
  socket_data2.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);

  request_.upload_data_stream = &upload_data;

  std::unique_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_.get()));
  TestCompletionCallback callback;
  int rv = trans->Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();
  upload_data.AppendData("1", 1, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(OK, callback.WaitForResult());
  trans.reset();
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, RetryAfterAsyncNoBufferSpace) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(ASYNC, ERR_NO_BUFFER_SPACE);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  socket_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  socket_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructClientAckPacket(packet_num++, 2, 1, 1));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeAckAndConnectionClosePacket(
                           packet_num++, false, 2, 1, 1,
                           quic::QUIC_CONNECTION_CANCELLED, "net error", 0));

  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, RetryAfterSynchronousNoBufferSpace) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(SYNCHRONOUS, ERR_NO_BUFFER_SPACE);
  socket_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  socket_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  socket_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  socket_data.AddWrite(SYNCHRONOUS,
                       ConstructClientAckPacket(packet_num++, 2, 1, 1));
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  socket_data.AddWrite(SYNCHRONOUS,
                       client_maker_->MakeAckAndConnectionClosePacket(
                           packet_num++, false, 2, 1, 1,
                           quic::QUIC_CONNECTION_CANCELLED, "net error", 0));

  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  SendRequestAndExpectQuicResponse("hello!");
  session_.reset();
}

TEST_P(QuicNetworkTransactionTest, MaxRetriesAfterAsyncNoBufferSpace) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  for (int i = 0; i < 13; ++i) {  // 12 retries then one final failure.
    socket_data.AddWrite(ASYNC, ERR_NO_BUFFER_SPACE);
  }
  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetTaskRunner(session_->quic_stream_factory(),
                                       quic_task_runner_.get());

  quic::QuicTime start = context_.clock()->Now();
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  while (!callback.have_result()) {
    base::RunLoop().RunUntilIdle();
    quic_task_runner_->RunUntilIdle();
  }
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  // Backoff should take between 4 - 5 seconds.
  EXPECT_TRUE(context_.clock()->Now() - start >
              quic::QuicTime::Delta::FromSeconds(4));
  EXPECT_TRUE(context_.clock()->Now() - start <
              quic::QuicTime::Delta::FromSeconds(5));
}

TEST_P(QuicNetworkTransactionTest, MaxRetriesAfterSynchronousNoBufferSpace) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  if (VersionUsesHttp3(version_.transport_version))
    socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(1));
  for (int i = 0; i < 13; ++i) {  // 12 retries then one final failure.
    socket_data.AddWrite(ASYNC, ERR_NO_BUFFER_SPACE);
  }
  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();
  // Use a TestTaskRunner to avoid waiting in real time for timeouts.
  QuicStreamFactoryPeer::SetTaskRunner(session_->quic_stream_factory(),
                                       quic_task_runner_.get());

  quic::QuicTime start = context_.clock()->Now();
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  while (!callback.have_result()) {
    base::RunLoop().RunUntilIdle();
    quic_task_runner_->RunUntilIdle();
  }
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  // Backoff should take between 4 - 5 seconds.
  EXPECT_TRUE(context_.clock()->Now() - start >
              quic::QuicTime::Delta::FromSeconds(4));
  EXPECT_TRUE(context_.clock()->Now() - start <
              quic::QuicTime::Delta::FromSeconds(5));
}

TEST_P(QuicNetworkTransactionTest, NoMigrationForMsgTooBig) {
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));
  const std::string error_details =
      quiche::QuicheStrCat("Write failed with error: ", ERR_MSG_TOO_BIG, " (",
                           strerror(ERR_MSG_TOO_BIG), ")");

  MockQuicData socket_data(version_);
  socket_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    socket_data.AddWrite(SYNCHRONOUS,
                         ConstructInitialSettingsPacket(packet_num++));
  }
  socket_data.AddWrite(SYNCHRONOUS, ERR_MSG_TOO_BIG);
  // Connection close packet will be sent for MSG_TOO_BIG.
  socket_data.AddWrite(
      SYNCHRONOUS,
      client_maker_->MakeConnectionClosePacket(
          packet_num + 1, true, quic::QUIC_PACKET_WRITE_ERROR, error_details));
  socket_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
}

// Adds coverage to catch regression such as https://crbug.com/622043
TEST_P(QuicNetworkTransactionTest, QuicServerPush) {
  client_maker_->set_max_allowed_push_id(quic::kMaxQuicStreamId);
  context_.params()->max_allowed_push_id = quic::kMaxQuicStreamId;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  uint64_t client_packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructInitialSettingsPacket(client_packet_number++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          client_packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerPushPromisePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0),
          GetNthServerInitiatedUnidirectionalStreamId(0), false,
          GetRequestHeaders("GET", "https", "/pushed.jpg"), &server_maker_));
  if ((client_headers_include_h2_stream_dependency_ &&
       version_.transport_version >= quic::QUIC_VERSION_43 &&
       !VersionUsesHttp3(version_.transport_version))) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientPriorityPacket(
            client_packet_number++, false,
            GetNthServerInitiatedUnidirectionalStreamId(0),
            GetNthClientInitiatedBidirectionalStreamId(0), DEFAULT_PRIORITY));
  }
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 2, 1, 1));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthServerInitiatedUnidirectionalStreamId(0), false,
                 false, GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 4, 3, 1));
  std::string header2 = ConstructDataHeader(10);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 5, GetNthServerInitiatedUnidirectionalStreamId(0), false, true,
                 header2 + "and hello!"));
  if (!VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructClientAckAndRstPacket(
                                client_packet_number++,
                                GetNthServerInitiatedUnidirectionalStreamId(0),
                                quic::QUIC_RST_ACKNOWLEDGEMENT, 5, 5, 1));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakeAckAndPriorityUpdatePacket(
                         client_packet_number++, true, 5, 5, 1, 3,
                         GetNthServerInitiatedUnidirectionalStreamId(0)));
  }
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  // PUSH_PROMISE handling in the http layer gets exercised here.
  SendRequestAndExpectQuicResponse("hello!");

  request_.url = GURL("https://mail.example.org/pushed.jpg");
  SendRequestAndExpectQuicResponse("and hello!");

  // Check that the NetLog was filled reasonably.
  auto entries = net_log_.GetEntries();
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_HTTP_STREAM_ADOPTED_PUSH_STREAM
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLogEventType::QUIC_HTTP_STREAM_ADOPTED_PUSH_STREAM,
      NetLogEventPhase::NONE);
  EXPECT_LT(0, pos);
}

// Regression test for http://crbug.com/719461 in which a promised stream
// is closed before the pushed headers arrive, but after the connection
// is closed and before the callbacks are executed.
TEST_P(QuicNetworkTransactionTest, CancelServerPushAfterConnectionClose) {
  client_maker_->set_max_allowed_push_id(quic::kMaxQuicStreamId);
  context_.params()->max_allowed_push_id = quic::kMaxQuicStreamId;
  context_.params()->retry_without_alt_svc_on_quic_errors = false;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  uint64_t client_packet_number = 1;
  // Initial SETTINGS frame.
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructInitialSettingsPacket(client_packet_number++));
  }
  // First request: GET https://mail.example.org/
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          client_packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, true, GetRequestHeaders("GET", "https", "/")));
  // Server promise for: https://mail.example.org/pushed.jpg
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerPushPromisePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0),
          GetNthServerInitiatedUnidirectionalStreamId(0), false,
          GetRequestHeaders("GET", "https", "/pushed.jpg"), &server_maker_));
  if ((client_headers_include_h2_stream_dependency_ &&
       version_.transport_version >= quic::QUIC_VERSION_43 &&
       !VersionUsesHttp3(version_.transport_version))) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientPriorityPacket(
            client_packet_number++, false,
            GetNthServerInitiatedUnidirectionalStreamId(0),
            GetNthClientInitiatedBidirectionalStreamId(0), DEFAULT_PRIORITY));
  }
  // Response headers for first request.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  // Client ACKs the response headers.
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 2, 1, 1));
  // Response body for first request.
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  // Write error for the third request.
  mock_quic_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  // Send a request which triggers a push promise from the server.
  SendRequestAndExpectQuicResponse("hello!");

  // Start a push transaction that will be cancelled after the connection
  // is closed, but before the callback is executed.
  request_.url = GURL("https://mail.example.org/pushed.jpg");
  auto trans2 = std::make_unique<HttpNetworkTransaction>(DEFAULT_PRIORITY,
                                                         session_.get());
  TestCompletionCallback callback2;
  int rv = trans2->Start(&request_, callback2.callback(), net_log_.bound());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();

  // Cause the connection to close on a write error.
  HttpRequestInfo request3;
  request3.method = "GET";
  request3.url = GURL("https://mail.example.org/");
  request3.load_flags = 0;
  request3.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  HttpNetworkTransaction trans3(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback3;
  EXPECT_THAT(trans3.Start(&request3, callback3.callback(), net_log_.bound()),
              IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  // When |trans2| is destroyed, the underlying stream will be closed.
  EXPECT_FALSE(callback2.have_result());
  trans2 = nullptr;

  EXPECT_THAT(callback3.WaitForResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_P(QuicNetworkTransactionTest, QuicForceHolBlocking) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);

  int write_packet_index = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructInitialSettingsPacket(write_packet_index++));
  }

  std::string header = ConstructDataHeader(1);
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientRequestHeadersAndDataFramesPacket(
            write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
            true, true, DEFAULT_PRIORITY,
            GetRequestHeaders("POST", "https", "/"), 0, nullptr, {"1"}));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientRequestHeadersAndDataFramesPacket(
            write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
            true, true, DEFAULT_PRIORITY,
            GetRequestHeaders("POST", "https", "/"), 0, nullptr,
            {header, "1"}));
  }

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));

  std::string header2 = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header2 + "hello!"));

  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(write_packet_index++, 2, 1, 1));

  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();
  request_.method = "POST";
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData("1", 1, true);

  request_.upload_data_stream = &upload_data;

  SendRequestAndExpectQuicResponse("hello!");
}

class QuicURLRequestContext : public URLRequestContext {
 public:
  QuicURLRequestContext(std::unique_ptr<HttpNetworkSession> session,
                        MockClientSocketFactory* socket_factory)
      : storage_(this) {
    socket_factory_ = socket_factory;
    storage_.set_host_resolver(std::make_unique<MockHostResolver>());
    storage_.set_cert_verifier(std::make_unique<MockCertVerifier>());
    storage_.set_transport_security_state(
        std::make_unique<TransportSecurityState>());
    storage_.set_proxy_resolution_service(
        ConfiguredProxyResolutionService::CreateDirect());
    storage_.set_ssl_config_service(
        std::make_unique<SSLConfigServiceDefaults>());
    storage_.set_http_auth_handler_factory(
        HttpAuthHandlerFactory::CreateDefault());
    storage_.set_http_server_properties(
        std::make_unique<HttpServerProperties>());
    storage_.set_job_factory(std::make_unique<URLRequestJobFactoryImpl>());
    storage_.set_http_network_session(std::move(session));
    storage_.set_http_transaction_factory(std::make_unique<HttpCache>(
        storage_.http_network_session(), HttpCache::DefaultBackend::InMemory(0),
        false));
  }

  ~QuicURLRequestContext() override { AssertNoURLRequests(); }

  MockClientSocketFactory& socket_factory() { return *socket_factory_; }

 private:
  MockClientSocketFactory* socket_factory_;
  URLRequestContextStorage storage_;
};

TEST_P(QuicNetworkTransactionTest, RawHeaderSizeSuccessfullRequest) {
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  spdy::SpdyHeaderBlock headers(GetRequestHeaders("GET", "https", "/"));
  headers["user-agent"] = "";
  headers["accept-encoding"] = "gzip, deflate";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, std::move(headers)));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  quic::QuicStreamOffset expected_raw_header_response_size =
      server_maker_.stream_offset(
          quic::VersionUsesHttp3(version_.transport_version)
              ? GetNthClientInitiatedBidirectionalStreamId(0)
              : quic::QuicUtils::GetHeadersStreamId(
                    version_.transport_version));

  std::string header = ConstructDataHeader(18);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 "Main Resource Data"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  mock_quic_data.AddRead(ASYNC, 0);  // EOF

  CreateSession();

  TestDelegate delegate;
  QuicURLRequestContext quic_url_request_context(std::move(session_),
                                                 &socket_factory_);

  mock_quic_data.AddSocketDataToFactory(
      &quic_url_request_context.socket_factory());
  TestNetworkDelegate network_delegate;
  quic_url_request_context.set_network_delegate(&network_delegate);

  std::unique_ptr<URLRequest> request(quic_url_request_context.CreateRequest(
      GURL("https://mail.example.org/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  quic_url_request_context.socket_factory().AddSSLSocketDataProvider(
      &ssl_data_);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_LT(0, request->GetTotalSentBytes());
  EXPECT_LT(0, request->GetTotalReceivedBytes());
  EXPECT_EQ(static_cast<int>(expected_raw_header_response_size),
            request->raw_header_size());

  // Pump the message loop to allow all data to be consumed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, RawHeaderSizeSuccessfullPushHeadersFirst) {
  client_maker_->set_max_allowed_push_id(quic::kMaxQuicStreamId);
  context_.params()->max_allowed_push_id = quic::kMaxQuicStreamId;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  uint64_t client_packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructInitialSettingsPacket(client_packet_number++));
  }
  spdy::SpdyHeaderBlock headers(GetRequestHeaders("GET", "https", "/"));
  headers["user-agent"] = "";
  headers["accept-encoding"] = "gzip, deflate";
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          client_packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, true, std::move(headers)));

  const quic::QuicStreamOffset initial = server_maker_.stream_offset(
      GetNthClientInitiatedBidirectionalStreamId(0));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerPushPromisePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0),
          GetNthServerInitiatedUnidirectionalStreamId(0), false,
          GetRequestHeaders("GET", "https", "/pushed.jpg"), &server_maker_));
  quic::QuicStreamOffset push_promise_offset = 0;
  if (VersionUsesHttp3(version_.transport_version)) {
    push_promise_offset = server_maker_.stream_offset(
                              GetNthClientInitiatedBidirectionalStreamId(0)) -
                          initial;
  }

  if ((client_headers_include_h2_stream_dependency_ &&
       version_.transport_version >= quic::QUIC_VERSION_43 &&
       !VersionUsesHttp3(version_.transport_version))) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientPriorityPacket(
            client_packet_number++, false,
            GetNthServerInitiatedUnidirectionalStreamId(0),
            GetNthClientInitiatedBidirectionalStreamId(0), DEFAULT_PRIORITY));
  }

  const quic::QuicStreamOffset initial_offset = server_maker_.stream_offset(
      quic::VersionUsesHttp3(version_.transport_version)
          ? GetNthClientInitiatedBidirectionalStreamId(0)
          : quic::QuicUtils::GetHeadersStreamId(version_.transport_version));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  const quic::QuicStreamOffset final_offset = server_maker_.stream_offset(
      quic::VersionUsesHttp3(version_.transport_version)
          ? GetNthClientInitiatedBidirectionalStreamId(0)
          : quic::QuicUtils::GetHeadersStreamId(version_.transport_version));
  quic::QuicStreamOffset expected_raw_header_response_size =
      final_offset - initial_offset;

  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 2, 1, 1));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthServerInitiatedUnidirectionalStreamId(0), false,
                 false, GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(20);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthServerInitiatedUnidirectionalStreamId(0), false, true,
                 header + "Pushed Resource Data"));

  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 4, 3, 1));
  std::string header2 = ConstructDataHeader(18);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 5, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header2 + "Main Resource Data"));

  mock_quic_data.AddRead(ASYNC, ConstructServerConnectionClosePacket(6));

  CreateSession();

  TestDelegate delegate;
  QuicURLRequestContext quic_url_request_context(std::move(session_),
                                                 &socket_factory_);

  mock_quic_data.AddSocketDataToFactory(
      &quic_url_request_context.socket_factory());
  TestNetworkDelegate network_delegate;
  quic_url_request_context.set_network_delegate(&network_delegate);

  std::unique_ptr<URLRequest> request(quic_url_request_context.CreateRequest(
      GURL("https://mail.example.org/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  quic_url_request_context.socket_factory().AddSSLSocketDataProvider(
      &ssl_data_);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_LT(0, request->GetTotalSentBytes());
  EXPECT_LT(0, request->GetTotalReceivedBytes());
  EXPECT_EQ(
      static_cast<int>(expected_raw_header_response_size + push_promise_offset),
      request->raw_header_size());

  // Pump the message loop to allow all data to be consumed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

TEST_P(QuicNetworkTransactionTest, HostInAllowlist) {
  session_params_.quic_host_allowlist.insert("mail.example.org");

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectQuicResponse("hello!");
}

TEST_P(QuicNetworkTransactionTest, HostNotInAllowlist) {
  session_params_.quic_host_allowlist.insert("mail.example.com");

  MockRead http_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"), MockRead(kQuicAlternativeServiceHeader),
      MockRead("hello world"),
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(ASYNC, OK)};

  StaticSocketDataProvider http_data(http_reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  socket_factory_.AddSocketDataProvider(&http_data);
  AddCertificate(&ssl_data_);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  AddHangingNonAlternateProtocolSocketData();
  CreateSession();

  SendRequestAndExpectHttpResponse("hello world");
  SendRequestAndExpectHttpResponse("hello world");
}

class QuicNetworkTransactionWithDestinationTest
    : public PlatformTest,
      public ::testing::WithParamInterface<PoolingTestParams>,
      public WithTaskEnvironment {
 protected:
  QuicNetworkTransactionWithDestinationTest()
      : version_(GetParam().version),
        client_headers_include_h2_stream_dependency_(
            GetParam().client_headers_include_h2_stream_dependency),
        supported_versions_(quic::test::SupportedVersions(version_)),
        destination_type_(GetParam().destination_type),
        cert_transparency_verifier_(new MultiLogCTVerifier()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        ssl_data_(ASYNC, OK) {
    FLAGS_quic_enable_http3_grease_randomness = false;
  }

  void SetUp() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();

    HttpNetworkSession::Params session_params;
    session_params.enable_quic = true;
    context_.params()->allow_remote_alt_svc = true;
    context_.params()->supported_versions = supported_versions_;
    context_.params()->headers_include_h2_stream_dependency =
        client_headers_include_h2_stream_dependency_;

    HttpNetworkSession::Context session_context;

    context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));

    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);
    session_context.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;

    session_context.quic_context = &context_;
    session_context.client_socket_factory = &socket_factory_;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = &cert_verifier_;
    session_context.transport_security_state = &transport_security_state_;
    session_context.cert_transparency_verifier =
        cert_transparency_verifier_.get();
    session_context.ct_policy_enforcer = &ct_policy_enforcer_;
    session_context.socket_performance_watcher_factory =
        &test_socket_performance_watcher_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.http_auth_handler_factory = auth_handler_factory_.get();
    session_context.http_server_properties = &http_server_properties_;

    session_.reset(new HttpNetworkSession(session_params, session_context));
    session_->quic_stream_factory()
        ->set_is_quic_known_to_work_on_current_network(false);
  }

  void TearDown() override {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    base::RunLoop().RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();
    session_.reset();
  }

  void SetQuicAlternativeService(const std::string& origin) {
    HostPortPair destination;
    switch (destination_type_) {
      case SAME_AS_FIRST:
        destination = HostPortPair(origin1_, 443);
        break;
      case SAME_AS_SECOND:
        destination = HostPortPair(origin2_, 443);
        break;
      case DIFFERENT:
        destination = HostPortPair(kDifferentHostname, 443);
        break;
    }
    AlternativeService alternative_service(kProtoQUIC, destination);
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    http_server_properties_.SetQuicAlternativeService(
        url::SchemeHostPort("https", origin, 443), NetworkIsolationKey(),
        alternative_service, expiration, supported_versions_);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(uint64_t packet_number,
                                      quic::QuicStreamId stream_id,
                                      bool should_include_version,
                                      quic::QuicStreamId parent_stream_id,
                                      QuicTestPacketMaker* maker) {
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    spdy::SpdyHeaderBlock headers(
        maker->GetRequestHeaders("GET", "https", "/"));
    return maker->MakeRequestHeadersPacket(
        packet_number, stream_id, should_include_version, true, priority,
        std::move(headers), parent_stream_id, nullptr);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructClientRequestHeadersPacket(uint64_t packet_number,
                                      quic::QuicStreamId stream_id,
                                      bool should_include_version,
                                      QuicTestPacketMaker* maker) {
    return ConstructClientRequestHeadersPacket(
        packet_number, stream_id, should_include_version, 0, maker);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructServerResponseHeadersPacket(uint64_t packet_number,
                                       quic::QuicStreamId stream_id,
                                       QuicTestPacketMaker* maker) {
    spdy::SpdyHeaderBlock headers(maker->GetResponseHeaders("200 OK"));
    return maker->MakeResponseHeadersPacket(packet_number, stream_id, false,
                                            false, std::move(headers), nullptr);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      QuicTestPacketMaker* maker) {
    std::string header = "";
    if (version_.HasIetfQuicFrames()) {
      std::unique_ptr<char[]> buffer;
      auto header_length =
          quic::HttpEncoder::SerializeDataFrameHeader(5, &buffer);
      header = std::string(buffer.get(), header_length);
    }
    return maker->MakeDataPacket(packet_number, stream_id, false, true,
                                 header + "hello");
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t least_unacked,
      QuicTestPacketMaker* maker) {
    return maker->MakeAckPacket(packet_number, largest_received,
                                smallest_received, least_unacked);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructInitialSettingsPacket(
      uint64_t packet_number,
      QuicTestPacketMaker* maker) {
    return maker->MakeInitialSettingsPacket(packet_number);
  }

  void AddRefusedSocketData() {
    std::unique_ptr<StaticSocketDataProvider> refused_data(
        new StaticSocketDataProvider());
    MockConnect refused_connect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
    refused_data->set_connect_data(refused_connect);
    socket_factory_.AddSocketDataProvider(refused_data.get());
    static_socket_data_provider_vector_.push_back(std::move(refused_data));
  }

  void AddHangingSocketData() {
    std::unique_ptr<StaticSocketDataProvider> hanging_data(
        new StaticSocketDataProvider());
    MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
    hanging_data->set_connect_data(hanging_connect);
    socket_factory_.AddSocketDataProvider(hanging_data.get());
    static_socket_data_provider_vector_.push_back(std::move(hanging_data));
    socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  }

  bool AllDataConsumed() {
    for (const auto& socket_data_ptr : static_socket_data_provider_vector_) {
      if (!socket_data_ptr->AllReadDataConsumed() ||
          !socket_data_ptr->AllWriteDataConsumed()) {
        return false;
      }
    }
    return true;
  }

  void SendRequestAndExpectQuicResponse(const std::string& host) {
    HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
    HttpRequestInfo request;
    std::string url("https://");
    url.append(host);
    request.url = GURL(url);
    request.load_flags = 0;
    request.method = "GET";
    request.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    TestCompletionCallback callback;
    int rv = trans.Start(&request, callback.callback(), net_log_.bound());
    EXPECT_THAT(callback.GetResult(rv), IsOk());

    std::string response_data;
    ASSERT_THAT(ReadTransaction(&trans, &response_data), IsOk());
    EXPECT_EQ("hello", response_data);

    const HttpResponseInfo* response = trans.GetResponseInfo();
    ASSERT_TRUE(response != nullptr);
    ASSERT_TRUE(response->headers.get() != nullptr);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
    EXPECT_TRUE(response->was_fetched_via_spdy);
    EXPECT_TRUE(response->was_alpn_negotiated);
    EXPECT_EQ(QuicHttpStream::ConnectionInfoFromQuicVersion(version_),
              response->connection_info);
    EXPECT_EQ(443, response->remote_endpoint.port());
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  const quic::ParsedQuicVersion version_;
  const bool client_headers_include_h2_stream_dependency_;
  quic::ParsedQuicVersionVector supported_versions_;
  DestinationType destination_type_;
  std::string origin1_;
  std::string origin2_;
  MockQuicContext context_;
  std::unique_ptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<CTVerifier> cert_transparency_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  TestSocketPerformanceWatcherFactory test_socket_performance_watcher_factory_;
  std::unique_ptr<SSLConfigServiceDefaults> ssl_config_service_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  HttpServerProperties http_server_properties_;
  RecordingBoundTestNetLog net_log_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  std::vector<std::unique_ptr<StaticSocketDataProvider>>
      static_socket_data_provider_vector_;
  SSLSocketDataProvider ssl_data_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicNetworkTransactionWithDestinationTest,
                         ::testing::ValuesIn(GetPoolingTestParams()),
                         ::testing::PrintToStringParamName());

// A single QUIC request fails because the certificate does not match the origin
// hostname, regardless of whether it matches the alternative service hostname.
TEST_P(QuicNetworkTransactionWithDestinationTest, InvalidCertificate) {
  if (destination_type_ == DIFFERENT)
    return;

  GURL url("https://mail.example.com/");
  origin1_ = url.host();

  // Not used for requests, but this provides a test case where the certificate
  // is valid for the hostname of the alternative service.
  origin2_ = "mail.example.org";

  SetQuicAlternativeService(origin1_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_FALSE(cert->VerifyNameMatch(origin1_));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData mock_quic_data(version_);
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data.AddRead(ASYNC, 0);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddRefusedSocketData();

  HttpRequestInfo request;
  request.url = url;
  request.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request, callback.callback(), net_log_.bound());
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(AllDataConsumed());
}

// First request opens QUIC session to alternative service.  Second request
// pools to it, because destination matches and certificate is valid, even
// though quic::QuicServerId is different.
TEST_P(QuicNetworkTransactionWithDestinationTest, PoolIfCertificateValid) {
  origin1_ = "mail.example.org";
  origin2_ = "news.example.org";

  SetQuicAlternativeService(origin1_);
  SetQuicAlternativeService(origin2_);

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert->VerifyNameMatch(origin1_));
  ASSERT_TRUE(cert->VerifyNameMatch(origin2_));
  ASSERT_FALSE(cert->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details;
  verify_details.cert_verify_result.verified_cert = cert;
  verify_details.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  QuicTestPacketMaker client_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_);
  QuicTestPacketMaker server_maker(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_SERVER, false);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                             packet_num++, &client_maker));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          &client_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(packet_num++, 2, 1, 1, &client_maker));

  client_maker.set_hostname(origin2_);
  server_maker.set_hostname(origin2_);

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          GetNthClientInitiatedBidirectionalStreamId(0), &client_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          3, GetNthClientInitiatedBidirectionalStreamId(1), &server_maker));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          4, GetNthClientInitiatedBidirectionalStreamId(1), &server_maker));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(packet_num++, 4, 3, 1, &client_maker));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  AddHangingSocketData();
  AddHangingSocketData();

  scoped_refptr<TestTaskRunner> quic_task_runner(
      new TestTaskRunner(context_.mock_clock()));
  QuicStreamFactoryPeer::SetAlarmFactory(
      session_->quic_stream_factory(),
      std::make_unique<QuicChromiumAlarmFactory>(quic_task_runner.get(),
                                                 context_.clock()));

  SendRequestAndExpectQuicResponse(origin1_);
  SendRequestAndExpectQuicResponse(origin2_);

  EXPECT_TRUE(AllDataConsumed());
}

// First request opens QUIC session to alternative service.  Second request does
// not pool to it, even though destination matches, because certificate is not
// valid.  Instead, a new QUIC session is opened to the same destination with a
// different quic::QuicServerId.
TEST_P(QuicNetworkTransactionWithDestinationTest,
       DoNotPoolIfCertificateInvalid) {
  origin1_ = "news.example.org";
  origin2_ = "mail.example.com";

  SetQuicAlternativeService(origin1_);
  SetQuicAlternativeService(origin2_);

  scoped_refptr<X509Certificate> cert1(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  ASSERT_TRUE(cert1->VerifyNameMatch(origin1_));
  ASSERT_FALSE(cert1->VerifyNameMatch(origin2_));
  ASSERT_FALSE(cert1->VerifyNameMatch(kDifferentHostname));

  scoped_refptr<X509Certificate> cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
  ASSERT_TRUE(cert2->VerifyNameMatch(origin2_));
  ASSERT_FALSE(cert2->VerifyNameMatch(kDifferentHostname));

  ProofVerifyDetailsChromium verify_details1;
  verify_details1.cert_verify_result.verified_cert = cert1;
  verify_details1.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details1);

  ProofVerifyDetailsChromium verify_details2;
  verify_details2.cert_verify_result.verified_cert = cert2;
  verify_details2.cert_verify_result.is_issued_by_known_root = true;
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details2);

  QuicTestPacketMaker client_maker1(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_);
  QuicTestPacketMaker server_maker1(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin1_, quic::Perspective::IS_SERVER, false);

  MockQuicData mock_quic_data1(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data1.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                              packet_num++, &client_maker1));
  }
  mock_quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          &client_maker1));
  mock_quic_data1.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker1));
  mock_quic_data1.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker1));
  mock_quic_data1.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(packet_num++, 2, 1, 1, &client_maker1));
  mock_quic_data1.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data1.AddRead(ASYNC, 0);               // EOF

  mock_quic_data1.AddSocketDataToFactory(&socket_factory_);

  QuicTestPacketMaker client_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2_, quic::Perspective::IS_CLIENT,
      client_headers_include_h2_stream_dependency_);
  QuicTestPacketMaker server_maker2(
      version_,
      quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
      context_.clock(), origin2_, quic::Perspective::IS_SERVER, false);

  MockQuicData mock_quic_data2(version_);
  int packet_num2 = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                              packet_num2++, &client_maker2));
  }
  mock_quic_data2.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num2++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          &client_maker2));
  mock_quic_data2.AddRead(
      ASYNC,
      ConstructServerResponseHeadersPacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker2));
  mock_quic_data2.AddRead(
      ASYNC,
      ConstructServerDataPacket(
          2, GetNthClientInitiatedBidirectionalStreamId(0), &server_maker2));
  mock_quic_data2.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckPacket(packet_num2++, 2, 1, 1, &client_maker2));
  mock_quic_data2.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data2.AddRead(ASYNC, 0);               // EOF

  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  SendRequestAndExpectQuicResponse(origin1_);
  SendRequestAndExpectQuicResponse(origin2_);

  EXPECT_TRUE(AllDataConsumed());
}

// crbug.com/705109 - this confirms that matching request with a body
// triggers a crash (pre-fix).
TEST_P(QuicNetworkTransactionTest, QuicServerPushMatchesRequestWithBody) {
  client_maker_->set_max_allowed_push_id(quic::kMaxQuicStreamId);
  context_.params()->max_allowed_push_id = quic::kMaxQuicStreamId;
  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);
  uint64_t client_packet_number = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructInitialSettingsPacket(client_packet_number++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          client_packet_number++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, true, GetRequestHeaders("GET", "https", "/")));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerPushPromisePacket(
          1, GetNthClientInitiatedBidirectionalStreamId(0),
          GetNthServerInitiatedUnidirectionalStreamId(0), false,
          GetRequestHeaders("GET", "https", "/pushed.jpg"), &server_maker_));

  if ((client_headers_include_h2_stream_dependency_ &&
       version_.transport_version >= quic::QUIC_VERSION_43 &&
       !VersionUsesHttp3(version_.transport_version))) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientPriorityPacket(
            client_packet_number++, false,
            GetNthServerInitiatedUnidirectionalStreamId(0),
            GetNthClientInitiatedBidirectionalStreamId(0), DEFAULT_PRIORITY));
  }
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 2, 1, 1));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthServerInitiatedUnidirectionalStreamId(0), false,
                 false, GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 4, 3, 1));

  std::string header2 = ConstructDataHeader(10);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 5, GetNthServerInitiatedUnidirectionalStreamId(0), false, true,
                 header2 + "and hello!"));

  // Because the matching request has a body, we will see the push
  // stream get cancelled, and the matching request go out on the
  // wire.
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckAndRstPacket(
                              client_packet_number++,
                              GetNthServerInitiatedUnidirectionalStreamId(0),
                              quic::QUIC_STREAM_CANCELLED, 5, 5, 1));
  const char kBody[] = "1";
  std::string header3 = ConstructDataHeader(1);
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientRequestHeadersAndDataFramesPacket(
            client_packet_number++,
            GetNthClientInitiatedBidirectionalStreamId(1), false, true,
            DEFAULT_PRIORITY, GetRequestHeaders("GET", "https", "/pushed.jpg"),
            GetNthServerInitiatedUnidirectionalStreamId(0), nullptr, {kBody}));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientRequestHeadersAndDataFramesPacket(
            client_packet_number++,
            GetNthClientInitiatedBidirectionalStreamId(1), false, true,
            DEFAULT_PRIORITY, GetRequestHeaders("GET", "https", "/pushed.jpg"),
            GetNthServerInitiatedUnidirectionalStreamId(0), nullptr,
            {header3, kBody}));
  }

  // We see the same response as for the earlier pushed and cancelled
  // stream.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 6, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 7, GetNthClientInitiatedBidirectionalStreamId(1), false, true,
                 header2 + "and hello!"));

  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(client_packet_number++, 7, 6, 1));
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  // PUSH_PROMISE handling in the http layer gets exercised here.
  SendRequestAndExpectQuicResponse("hello!");

  request_.url = GURL("https://mail.example.org/pushed.jpg");
  ChunkedUploadDataStream upload_data(0);
  upload_data.AppendData("1", 1, true);
  request_.upload_data_stream = &upload_data;
  SendRequestAndExpectQuicResponse("and hello!");
}

// Regression test for https://crbug.com/797825: If pushed headers describe a
// valid URL with empty hostname, then X509Certificate::VerifyHostname() must
// not be called (otherwise a DCHECK fails).
TEST_P(QuicNetworkTransactionTest, QuicServerPushWithEmptyHostname) {
  client_maker_->set_max_allowed_push_id(quic::kMaxQuicStreamId);
  context_.params()->max_allowed_push_id = quic::kMaxQuicStreamId;

  spdy::SpdyHeaderBlock pushed_request_headers;
  pushed_request_headers[":authority"] = "";
  pushed_request_headers[":method"] = "GET";
  pushed_request_headers[":path"] = "/";
  pushed_request_headers[":scheme"] = "nosuchscheme";

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  MockQuicData mock_quic_data(version_);

  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          true, GetRequestHeaders("GET", "https", "/")));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerPushPromisePacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0),
                 GetNthServerInitiatedUnidirectionalStreamId(0), false,
                 std::move(pushed_request_headers), &server_maker_));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_INVALID_PROMISE_URL));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));

  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 3, GetNthServerInitiatedUnidirectionalStreamId(0), false,
                 false, GetResponseHeaders("200 OK")));
  std::string header = ConstructDataHeader(6);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 header + "hello!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3, 1));

  mock_quic_data.AddRead(ASYNC, 0);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  // PUSH_PROMISE handling in the http layer gets exercised here.
  SendRequestAndExpectQuicResponse("hello!");

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Performs an HTTPS/1.1 request over QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectHttpsServer) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));

  const char get_request[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  std::string header = ConstructDataHeader(strlen(get_request));
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            1, 1, 1, false, quiche::QuicheStringPiece(get_request)));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            1, 1, 1, false, {header + std::string(get_request)}));
  }

  const char get_response[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  std::string header2 = ConstructDataHeader(strlen(get_response));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 header2 + std::string(get_response)));
  std::string header3 = ConstructDataHeader(10);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       false, header3 + std::string("0123456789")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans);
  CheckWasHttpResponse(&trans);
  CheckResponsePort(&trans, 70);
  CheckResponseData(&trans, "0123456789");
  EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_quic());

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Performs an HTTP/2 request over QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectSpdyServer) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));

  SpdyTestUtil spdy_util;

  spdy::SpdySerializedFrame get_frame =
      spdy_util.ConstructSpdyGet("https://mail.example.org/", 1, LOWEST);
  std::string header = ConstructDataHeader(get_frame.size());
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            1, 1, 1, false,
            quiche::QuicheStringPiece(get_frame.data(), get_frame.size())));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            1, 1, 1, false,
            {header + std::string(get_frame.data(), get_frame.size())}));
  }
  spdy::SpdySerializedFrame resp_frame =
      spdy_util.ConstructSpdyGetReply(nullptr, 0, 1);
  std::string header2 = ConstructDataHeader(resp_frame.size());
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 header2 + std::string(resp_frame.data(), resp_frame.size())));

  spdy::SpdySerializedFrame data_frame =
      spdy_util.ConstructSpdyDataFrame(1, "0123456789", true);
  std::string header3 = ConstructDataHeader(resp_frame.size());
  mock_quic_data.AddRead(
      SYNCHRONOUS,
      ConstructServerDataPacket(
          3, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
          header3 + std::string(data_frame.data(), data_frame.size())));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans);
  CheckWasSpdyResponse(&trans);
  CheckResponsePort(&trans, 70);
  CheckResponseData(&trans, "0123456789");
  EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_quic());

  // Causes MockSSLClientSocket to disconproxyconnecthttpnect, which causes the
  // underlying QUIC proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Make two HTTP/1.1 requests to the same host over a QUIC proxy tunnel and
// check that the proxy socket is reused for the second request.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectReuseTransportSocket) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int write_packet_index = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructInitialSettingsPacket(write_packet_index++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         write_packet_index++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
          true, false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));

  quic::QuicStreamOffset server_data_offset = 0;
  const char get_request_1[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  std::string header = ConstructDataHeader(strlen(get_request_1));
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientAckAndDataPacket(
                         write_packet_index++, false,
                         GetNthClientInitiatedBidirectionalStreamId(0), 1, 1, 1,
                         false, quiche::QuicheStringPiece(get_request_1)));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientAckAndDataPacket(
                         write_packet_index++, false,
                         GetNthClientInitiatedBidirectionalStreamId(0), 1, 1, 1,
                         false, {header + std::string(get_request_1)}));
  }

  const char get_response_1[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  std::string header2 = ConstructDataHeader(strlen(get_response_1));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 header2 + std::string(get_response_1)));
  server_data_offset += strlen(get_response_1) + header2.length();

  std::string header3 = ConstructDataHeader(10);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       false, header3 + std::string("0123456789")));
  server_data_offset += 10 + header3.length();

  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(write_packet_index++, 3, 2, 1));

  const char get_request_2[] =
      "GET /2 HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  std::string header4 = ConstructDataHeader(strlen(get_request_2));
  if (version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientDataPacket(
            write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
            false, false, {header4 + std::string(get_request_2)}));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientDataPacket(
            write_packet_index++, GetNthClientInitiatedBidirectionalStreamId(0),
            false, false, quiche::QuicheStringPiece(get_request_2)));
  }

  const char get_response_2[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  std::string header5 = ConstructDataHeader(strlen(get_response_2));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 header5 + std::string(get_response_2)));
  server_data_offset += strlen(get_response_2) + header5.length();

  std::string header6 = ConstructDataHeader(7);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       5, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       false, header6 + std::string("0123456")));
  server_data_offset += 7 + header6.length();

  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientAckPacket(write_packet_index++, 5, 4, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         write_packet_index++, GetQpackDecoderStreamId(), true,
                         false, StreamCancellationQpackDecoderInstruction(0)));
  }

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(write_packet_index++,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans_1(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans_1);
  CheckWasHttpResponse(&trans_1);
  CheckResponsePort(&trans_1, 70);
  CheckResponseData(&trans_1, "0123456789");
  EXPECT_TRUE(trans_1.GetResponseInfo()->proxy_server.is_quic());

  request_.url = GURL("https://mail.example.org/2");
  HttpNetworkTransaction trans_2(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans_2);
  CheckWasHttpResponse(&trans_2);
  CheckResponsePort(&trans_2, 70);
  CheckResponseData(&trans_2, "0123456");
  EXPECT_TRUE(trans_2.GetResponseInfo()->proxy_server.is_quic());

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Make an HTTP/1.1 request to one host and an HTTP/2 request to a different
// host over a QUIC proxy tunnel. Check that the QUIC session to the proxy
// server is reused for the second request.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectReuseQuicSession) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }

  // CONNECT request and response for first request
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));

  // GET request, response, and data over QUIC tunnel for first request
  const char get_request[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  std::string header = ConstructDataHeader(strlen(get_request));
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            1, 1, 1, false, quiche::QuicheStringPiece(get_request)));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            1, 1, 1, false, {header + std::string(get_request)}));
  }

  const char get_response[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  std::string header2 = ConstructDataHeader(strlen(get_response));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 header2 + std::string(get_response)));
  std::string header3 = ConstructDataHeader(10);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       3, GetNthClientInitiatedBidirectionalStreamId(0), false,
                       false, header3 + std::string("0123456789")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 3, 2, 1));

  // CONNECT request and response for second request
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(1), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("different.example.org:443"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 4, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));

  // GET request, response, and data over QUIC tunnel for second request
  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame get_frame =
      spdy_util.ConstructSpdyGet("https://different.example.org/", 1, LOWEST);
  std::string header4 = ConstructDataHeader(get_frame.size());
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(1),
            4, 4, 1, false,
            quiche::QuicheStringPiece(get_frame.data(), get_frame.size())));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(1),
            4, 4, 1, false,
            {header4 + std::string(get_frame.data(), get_frame.size())}));
  }

  spdy::SpdySerializedFrame resp_frame =
      spdy_util.ConstructSpdyGetReply(nullptr, 0, 1);
  std::string header5 = ConstructDataHeader(resp_frame.size());
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 5, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 header5 + std::string(resp_frame.data(), resp_frame.size())));

  spdy::SpdySerializedFrame data_frame =
      spdy_util.ConstructSpdyDataFrame(1, "0123456", true);
  std::string header6 = ConstructDataHeader(data_frame.size());
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 6, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 header6 + std::string(data_frame.data(), data_frame.size())));

  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 6, 5, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(0)));
  }

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED));

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(1, false)));
  }

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthClientInitiatedBidirectionalStreamId(1),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans_1(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans_1);
  CheckWasHttpResponse(&trans_1);
  CheckResponsePort(&trans_1, 70);
  CheckResponseData(&trans_1, "0123456789");
  EXPECT_TRUE(trans_1.GetResponseInfo()->proxy_server.is_quic());

  request_.url = GURL("https://different.example.org/");
  HttpNetworkTransaction trans_2(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans_2);
  CheckWasSpdyResponse(&trans_2);
  CheckResponsePort(&trans_2, 70);
  CheckResponseData(&trans_2, "0123456");
  EXPECT_TRUE(trans_2.GetResponseInfo()->proxy_server.is_quic());

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Sends a CONNECT request to a QUIC proxy and receive a 500 response.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectFailure) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, true,
                 GetResponseHeaders("500")));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndRstPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          quic::QUIC_STREAM_CANCELLED, 1, 1, 1));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Sends a CONNECT request to a QUIC proxy and get a UDP socket read error.
TEST_P(QuicNetworkTransactionTest, QuicProxyQuicConnectionError) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Sends an HTTP/1.1 request over QUIC proxy tunnel and gets a bad cert from the
// host. Retries request and succeeds.
TEST_P(QuicNetworkTransactionTest, QuicProxyConnectBadCertificate) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
                 GetResponseHeaders("200 OK")));
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetQpackDecoderStreamId(), 1, 1, 1, false,
            StreamCancellationQpackDecoderInstruction(0)));
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientRstPacket(packet_num++,
                                 GetNthClientInitiatedBidirectionalStreamId(0),
                                 quic::QUIC_STREAM_CANCELLED));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndRstPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED, 1, 1, 1));
  }

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(1), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }

  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"),
          GetNthClientInitiatedBidirectionalStreamId(0)));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 2, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 GetResponseHeaders("200 OK")));

  const char get_request[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  std::string header = ConstructDataHeader(strlen(get_request));
  if (!version_.HasIetfQuicFrames()) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(1),
            2, 2, 1, false, quiche::QuicheStringPiece(get_request)));
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        ConstructClientAckAndDataPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(1),
            2, 2, 1, false, {header + std::string(get_request)}));
  }
  const char get_response[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";
  std::string header2 = ConstructDataHeader(strlen(get_response));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(
                 3, GetNthClientInitiatedBidirectionalStreamId(1), false, false,
                 header2 + std::string(get_response)));

  std::string header3 = ConstructDataHeader(10);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(
                       4, GetNthClientInitiatedBidirectionalStreamId(1), false,
                       false, header3 + std::string("0123456789")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 4, 3, 1));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more data to read

  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientDataPacket(
                         packet_num++, GetQpackDecoderStreamId(), true, false,
                         StreamCancellationQpackDecoderInstruction(1, false)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRstPacket(packet_num++,
                               GetNthClientInitiatedBidirectionalStreamId(1),
                               quic::QUIC_STREAM_CANCELLED));

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  SSLSocketDataProvider ssl_data_bad_cert(ASYNC, ERR_CERT_AUTHORITY_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data_bad_cert);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, callback.WaitForResult());

  rv = trans.RestartIgnoringLastError(callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  CheckWasHttpResponse(&trans);
  CheckResponsePort(&trans, 70);
  CheckResponseData(&trans, "0123456789");
  EXPECT_TRUE(trans.GetResponseInfo()->proxy_server.is_quic());

  // Causes MockSSLClientSocket to disconnect, which causes the underlying QUIC
  // proxy socket to disconnect.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Checks if a request's specified "user-agent" header shows up correctly in the
// CONNECT request to a QUIC proxy.
TEST_P(QuicNetworkTransactionTest, QuicProxyUserAgent) {
  const char kConfiguredUserAgent[] = "Configured User-Agent";
  const char kRequestUserAgent[] = "Request User-Agent";
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }

  spdy::SpdyHeaderBlock headers = ConnectRequestHeaders("mail.example.org:443");
  headers["user-agent"] = kConfiguredUserAgent;
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          std::move(headers), 0));
  // Return an error, so the transaction stops here (this test isn't interested
  // in the rest).
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  StaticHttpUserAgentSettings http_user_agent_settings(
      std::string() /* accept_language */, kConfiguredUserAgent);
  session_context_.http_user_agent_settings = &http_user_agent_settings;
  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  request_.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                   kRequestUserAgent);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Makes sure the CONNECT request packet for a QUIC proxy contains the correct
// HTTP/2 stream dependency and weights given the request priority.
TEST_P(QuicNetworkTransactionTest, QuicProxyRequestPriority) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  const RequestPriority request_priority = MEDIUM;

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
    mock_quic_data.AddWrite(
        SYNCHRONOUS, client_maker_->MakePriorityPacket(
                         packet_num++, true,
                         GetNthClientInitiatedBidirectionalStreamId(0), 0,
                         ConvertRequestPriorityToQuicPriority(
                             HttpProxyConnectJob::kH2QuicTunnelPriority)));
  }
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
          false,
          VersionUsesHttp3(version_.transport_version)
              ? MEDIUM
              : HttpProxyConnectJob::kH2QuicTunnelPriority,
          ConnectRequestHeaders("mail.example.org:443"), 0));
  // Return an error, so the transaction stops here (this test isn't interested
  // in the rest).
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(request_priority, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());

  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Makes sure the CONNECT request packet for a QUIC proxy contains the correct
// HTTP/2 stream dependency and weights given the request priority.
TEST_P(QuicNetworkTransactionTest, QuicProxyMultipleRequestsError) {
  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  const RequestPriority kRequestPriority = MEDIUM;
  const RequestPriority kRequestPriority2 = LOWEST;

  MockQuicData mock_quic_data(version_);
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(ASYNC, ConstructInitialSettingsPacket(1));
    mock_quic_data.AddWrite(SYNCHRONOUS, ERR_FAILED);
  } else {
    mock_quic_data.AddWrite(
        SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                         1, GetNthClientInitiatedBidirectionalStreamId(0), true,
                         false, HttpProxyConnectJob::kH2QuicTunnelPriority,
                         ConnectRequestHeaders("mail.example.org:443"), 0));
  }
  // This should never be reached.
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_FAILED);
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // Second connection attempt just fails - result doesn't really matter.
  MockQuicData mock_quic_data2(version_);
  mock_quic_data2.AddConnect(SYNCHRONOUS, ERR_FAILED);
  mock_quic_data2.AddSocketDataToFactory(&socket_factory_);

  int original_max_sockets_per_group =
      ClientSocketPoolManager::max_sockets_per_group(
          HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL, 1);
  int original_max_sockets_per_pool =
      ClientSocketPoolManager::max_sockets_per_pool(
          HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL, 1);
  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  HttpNetworkTransaction trans(kRequestPriority, session_.get());
  TestCompletionCallback callback;
  int rv = trans.Start(&request_, callback.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  HttpRequestInfo request2;
  request2.url = GURL("https://mail.example.org/some/other/path/");
  request2.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  HttpNetworkTransaction trans2(kRequestPriority2, session_.get());
  TestCompletionCallback callback2;
  int rv2 = trans2.Start(&request2, callback2.callback(), net_log_.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv2);

  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, callback.WaitForResult());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());

  EXPECT_EQ(ERR_FAILED, callback2.WaitForResult());

  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
      original_max_sockets_per_pool);
  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::SocketPoolType::NORMAL_SOCKET_POOL,
      original_max_sockets_per_group);
}

// Test the request-challenge-retry sequence for basic auth, over a QUIC
// connection when setting up a QUIC proxy tunnel.
TEST_P(QuicNetworkTransactionTest, QuicProxyAuth) {
  const base::string16 kBaz(base::ASCIIToUTF16("baz"));
  const base::string16 kFoo(base::ASCIIToUTF16("foo"));

  // On the second pass, the body read of the auth challenge is synchronous, so
  // IsConnectedAndIdle returns false.  The socket should still be drained and
  // reused. See http://crbug.com/544255.
  for (int i = 0; i < 2; ++i) {
    QuicTestPacketMaker client_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        client_headers_include_h2_stream_dependency_);
    QuicTestPacketMaker server_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        false);

    session_params_.enable_quic = true;
    session_params_.enable_quic_proxies_for_https_urls = true;
    proxy_resolution_service_ =
        ConfiguredProxyResolutionService::CreateFixedFromPacResult(
            "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

    MockQuicData mock_quic_data(version_);
    quic::QuicStreamOffset server_data_offset = 0;

    int packet_num = 1;
    if (VersionUsesHttp3(version_.transport_version)) {
      mock_quic_data.AddWrite(
          SYNCHRONOUS, client_maker.MakeInitialSettingsPacket(packet_num++));
      mock_quic_data.AddWrite(
          SYNCHRONOUS, client_maker.MakePriorityPacket(
                           packet_num++, true,
                           GetNthClientInitiatedBidirectionalStreamId(0), 0,
                           ConvertRequestPriorityToQuicPriority(
                               HttpProxyConnectJob::kH2QuicTunnelPriority)));
    }

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
            false,
            VersionUsesHttp3(version_.transport_version)
                ? 1
                : ConvertRequestPriorityToQuicPriority(
                      HttpProxyConnectJob::kH2QuicTunnelPriority),
            client_maker.ConnectRequestHeaders("mail.example.org:443"), 0,
            nullptr));

    spdy::SpdyHeaderBlock headers =
        server_maker.GetResponseHeaders("407 Proxy Authentication Required");
    headers["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
    headers["content-length"] = "10";
    mock_quic_data.AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   false, std::move(headers), nullptr));

    if (i == 0) {
      mock_quic_data.AddRead(
          ASYNC, server_maker.MakeDataPacket(
                     2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     false, "0123456789"));
    } else {
      mock_quic_data.AddRead(
          SYNCHRONOUS, server_maker.MakeDataPacket(
                           2, GetNthClientInitiatedBidirectionalStreamId(0),
                           false, false, "0123456789"));
    }
    server_data_offset += 10;

    mock_quic_data.AddWrite(SYNCHRONOUS,
                            client_maker.MakeAckPacket(packet_num++, 2, 1, 1));

    if (VersionUsesHttp3(version_.transport_version)) {
      mock_quic_data.AddWrite(
          SYNCHRONOUS,
          client_maker.MakeDataPacket(
              packet_num++, GetQpackDecoderStreamId(),
              /* should_include_version = */ true,
              /* fin = */ false, StreamCancellationQpackDecoderInstruction(0)));
    }

    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRstPacket(
            packet_num++, false, GetNthClientInitiatedBidirectionalStreamId(0),
            quic::QUIC_STREAM_CANCELLED,
            /*include_stop_sending_if_v99=*/true));

    headers = client_maker.ConnectRequestHeaders("mail.example.org:443");
    headers["proxy-authorization"] = "Basic Zm9vOmJheg==";
    if (VersionUsesHttp3(version_.transport_version)) {
      mock_quic_data.AddWrite(
          SYNCHRONOUS, client_maker.MakePriorityPacket(
                           packet_num++, true,
                           GetNthClientInitiatedBidirectionalStreamId(1), 0,
                           ConvertRequestPriorityToQuicPriority(
                               HttpProxyConnectJob::kH2QuicTunnelPriority)));
    }
    mock_quic_data.AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(1), false,
            false,
            VersionUsesHttp3(version_.transport_version)
                ? 1
                : ConvertRequestPriorityToQuicPriority(
                      HttpProxyConnectJob::kH2QuicTunnelPriority),
            std::move(headers), GetNthClientInitiatedBidirectionalStreamId(0),
            nullptr));

    // Response to wrong password
    headers =
        server_maker.GetResponseHeaders("407 Proxy Authentication Required");
    headers["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
    headers["content-length"] = "10";
    mock_quic_data.AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                   false, std::move(headers), nullptr));
    mock_quic_data.AddRead(SYNCHRONOUS,
                           ERR_IO_PENDING);  // No more data to read

    if (VersionUsesHttp3(version_.transport_version)) {
      mock_quic_data.AddWrite(
          SYNCHRONOUS,
          client_maker.MakeAckAndDataPacket(
              packet_num++, false, GetQpackDecoderStreamId(), 3, 3, 1, false,
              StreamCancellationQpackDecoderInstruction(1, false)));
      mock_quic_data.AddWrite(SYNCHRONOUS,
                              client_maker.MakeRstPacket(
                                  packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED));
    } else {
      mock_quic_data.AddWrite(SYNCHRONOUS,
                              client_maker.MakeAckAndRstPacket(
                                  packet_num++, false,
                                  GetNthClientInitiatedBidirectionalStreamId(1),
                                  quic::QUIC_STREAM_CANCELLED, 3, 3, 1));
    }

    mock_quic_data.AddSocketDataToFactory(&socket_factory_);
    mock_quic_data.GetSequencedSocketData()->set_busy_before_sync_reads(true);

    CreateSession();

    request_.url = GURL("https://mail.example.org/");
    // Ensure that proxy authentication is attempted even
    // when the no authentication data flag is set.
    request_.load_flags = LOAD_DO_NOT_SEND_AUTH_DATA;
    {
      HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
      RunTransaction(&trans);

      const HttpResponseInfo* response = trans.GetResponseInfo();
      ASSERT_TRUE(response != nullptr);
      ASSERT_TRUE(response->headers.get() != nullptr);
      EXPECT_EQ("HTTP/1.1 407 Proxy Authentication Required",
                response->headers->GetStatusLine());
      EXPECT_TRUE(response->headers->IsKeepAlive());
      EXPECT_EQ(407, response->headers->response_code());
      EXPECT_EQ(10, response->headers->GetContentLength());
      EXPECT_EQ(HttpVersion(1, 1), response->headers->GetHttpVersion());
      base::Optional<AuthChallengeInfo> auth_challenge =
          response->auth_challenge;
      ASSERT_TRUE(auth_challenge.has_value());
      EXPECT_TRUE(auth_challenge->is_proxy);
      EXPECT_EQ("https://proxy.example.org:70",
                auth_challenge->challenger.Serialize());
      EXPECT_EQ("MyRealm1", auth_challenge->realm);
      EXPECT_EQ("basic", auth_challenge->scheme);

      TestCompletionCallback callback;
      int rv = trans.RestartWithAuth(AuthCredentials(kFoo, kBaz),
                                     callback.callback());
      EXPECT_EQ(ERR_IO_PENDING, rv);
      EXPECT_EQ(OK, callback.WaitForResult());

      response = trans.GetResponseInfo();
      ASSERT_TRUE(response != nullptr);
      ASSERT_TRUE(response->headers.get() != nullptr);
      EXPECT_EQ("HTTP/1.1 407 Proxy Authentication Required",
                response->headers->GetStatusLine());
      EXPECT_TRUE(response->headers->IsKeepAlive());
      EXPECT_EQ(407, response->headers->response_code());
      EXPECT_EQ(10, response->headers->GetContentLength());
      EXPECT_EQ(HttpVersion(1, 1), response->headers->GetHttpVersion());
      auth_challenge = response->auth_challenge;
      ASSERT_TRUE(auth_challenge.has_value());
      EXPECT_TRUE(auth_challenge->is_proxy);
      EXPECT_EQ("https://proxy.example.org:70",
                auth_challenge->challenger.Serialize());
      EXPECT_EQ("MyRealm1", auth_challenge->realm);
      EXPECT_EQ("basic", auth_challenge->scheme);
    }
    // HttpNetworkTransaction is torn down now that it's out of scope, causing
    // the QUIC stream to be cleaned up (since the proxy socket cannot be
    // reused because it's not connected).
    EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
  }
}

TEST_P(QuicNetworkTransactionTest, QuicServerPushUpdatesPriority) {
  client_maker_->set_max_allowed_push_id(quic::kMaxQuicStreamId);
  context_.params()->max_allowed_push_id = quic::kMaxQuicStreamId;

  // Only run this test if HTTP/2 stream dependency info is sent by client (sent
  // in HEADERS frames for requests and PRIORITY frames).
  if (version_.transport_version < quic::QUIC_VERSION_43 ||
      !client_headers_include_h2_stream_dependency_) {
    return;
  }

  if (quic::VersionUsesHttp3(version_.transport_version)) {
    // HTTP/3 currently doesn't support PRIORITY.
    return;
  }

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  const quic::QuicStreamId client_stream_0 =
      GetNthClientInitiatedBidirectionalStreamId(0);
  const quic::QuicStreamId client_stream_1 =
      GetNthClientInitiatedBidirectionalStreamId(1);
  const quic::QuicStreamId client_stream_2 =
      GetNthClientInitiatedBidirectionalStreamId(2);
  const quic::QuicStreamId push_stream_0 =
      GetNthServerInitiatedUnidirectionalStreamId(0);
  const quic::QuicStreamId push_stream_1 =
      GetNthServerInitiatedUnidirectionalStreamId(1);

  MockQuicData mock_quic_data(version_);
  int packet_num = 1;
  if (VersionUsesHttp3(version_.transport_version)) {
    mock_quic_data.AddWrite(SYNCHRONOUS,
                            ConstructInitialSettingsPacket(packet_num++));
  }

  // Client sends "GET" requests for "/0.png", "/1.png", "/2.png".
  mock_quic_data.AddWrite(
      SYNCHRONOUS, ConstructClientRequestHeadersPacket(
                       packet_num++, client_stream_0, true, true, HIGHEST,
                       GetRequestHeaders("GET", "https", "/0.jpg"), 0));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, client_stream_1, true, true, MEDIUM,
          GetRequestHeaders("GET", "https", "/1.jpg"), client_stream_0));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientRequestHeadersPacket(
          packet_num++, client_stream_2, true, true, MEDIUM,
          GetRequestHeaders("GET", "https", "/2.jpg"), client_stream_1));

  // Server replies "OK" for the three requests.
  mock_quic_data.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                                    1, client_stream_0, false, false,
                                    GetResponseHeaders("200 OK")));
  mock_quic_data.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                                    2, client_stream_1, false, false,
                                    GetResponseHeaders("200 OK")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 2, 1, 1));
  mock_quic_data.AddRead(ASYNC, ConstructServerResponseHeadersPacket(
                                    3, client_stream_2, false, false,
                                    GetResponseHeaders("200 OK")));

  // Server sends two push promises associated with |client_stream_0|; client
  // responds with a PRIORITY frame after each to notify server of HTTP/2 stream
  // dependency info for each push promise stream.
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerPushPromisePacket(
          4, client_stream_0, push_stream_0, false,
          GetRequestHeaders("GET", "https", "/pushed_0.jpg"), &server_maker_));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndPriorityFramesPacket(
          packet_num++, false, 4, 3, 1,
          {{push_stream_0, client_stream_2,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY)}}));
  mock_quic_data.AddRead(
      ASYNC,
      ConstructServerPushPromisePacket(
          5, client_stream_0, push_stream_1, false,
          GetRequestHeaders("GET", "https", "/pushed_1.jpg"), &server_maker_));
  mock_quic_data.AddWrite(SYNCHRONOUS, ConstructClientPriorityPacket(
                                           packet_num++, false, push_stream_1,
                                           push_stream_0, DEFAULT_PRIORITY));

  // Server sends the response headers for the two push promises.
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 6, push_stream_0, false, false, GetResponseHeaders("200 OK")));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 6, 5, 1));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerResponseHeadersPacket(
                 7, push_stream_1, false, false, GetResponseHeaders("200 OK")));

  // Request for "pushed_0.jpg" matches |push_stream_0|. |push_stream_0|'s
  // priority updates to match the request's priority. Client sends PRIORITY
  // frames to inform server of new HTTP/2 stream dependencies.
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructClientAckAndPriorityFramesPacket(
          packet_num++, false, 7, 7, 1,
          {{push_stream_1, client_stream_2,
            ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY)},
           {push_stream_0, client_stream_0,
            ConvertRequestPriorityToQuicPriority(HIGHEST)}}));

  // Server sends data for the three requests and the two push promises.
  std::string header = ConstructDataHeader(8);
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(8, client_stream_0, false, true,
                                       header + "hello 0!"));
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(9, client_stream_1, false, true,
                                             header + "hello 1!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 9, 8, 1));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(10, client_stream_2, false, true,
                                       header + "hello 2!"));
  std::string header2 = ConstructDataHeader(12);
  mock_quic_data.AddRead(
      SYNCHRONOUS, ConstructServerDataPacket(11, push_stream_0, false, true,
                                             header2 + "and hello 0!"));
  mock_quic_data.AddWrite(SYNCHRONOUS,
                          ConstructClientAckPacket(packet_num++, 11, 10, 1));
  mock_quic_data.AddRead(
      ASYNC, ConstructServerDataPacket(12, push_stream_1, false, true,
                                       header2 + "and hello 1!"));

  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // No more data to read
  mock_quic_data.AddRead(ASYNC, 0);               // EOF
  mock_quic_data.AddSocketDataToFactory(&socket_factory_);

  // The non-alternate protocol job needs to hang in order to guarantee that
  // the alternate-protocol job will "win".
  AddHangingNonAlternateProtocolSocketData();

  CreateSession();

  request_.url = GURL("https://mail.example.org/0.jpg");
  HttpNetworkTransaction trans_0(HIGHEST, session_.get());
  TestCompletionCallback callback_0;
  EXPECT_EQ(ERR_IO_PENDING,
            trans_0.Start(&request_, callback_0.callback(), net_log_.bound()));
  base::RunLoop().RunUntilIdle();

  request_.url = GURL("https://mail.example.org/1.jpg");
  HttpNetworkTransaction trans_1(MEDIUM, session_.get());
  TestCompletionCallback callback_1;
  EXPECT_EQ(ERR_IO_PENDING,
            trans_1.Start(&request_, callback_1.callback(), net_log_.bound()));
  base::RunLoop().RunUntilIdle();

  request_.url = GURL("https://mail.example.org/2.jpg");
  HttpNetworkTransaction trans_2(MEDIUM, session_.get());
  TestCompletionCallback callback_2;
  EXPECT_EQ(ERR_IO_PENDING,
            trans_2.Start(&request_, callback_2.callback(), net_log_.bound()));
  base::RunLoop().RunUntilIdle();

  // Client makes request that matches resource pushed in |pushed_stream_0|.
  request_.url = GURL("https://mail.example.org/pushed_0.jpg");
  HttpNetworkTransaction trans_3(HIGHEST, session_.get());
  TestCompletionCallback callback_3;
  EXPECT_EQ(ERR_IO_PENDING,
            trans_3.Start(&request_, callback_3.callback(), net_log_.bound()));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_0.have_result());
  EXPECT_EQ(OK, callback_0.WaitForResult());
  EXPECT_TRUE(callback_1.have_result());
  EXPECT_EQ(OK, callback_1.WaitForResult());
  EXPECT_TRUE(callback_2.have_result());
  EXPECT_EQ(OK, callback_2.WaitForResult());

  CheckResponseData(&trans_0, "hello 0!");      // Closes stream 5
  CheckResponseData(&trans_1, "hello 1!");      // Closes stream 7
  CheckResponseData(&trans_2, "hello 2!");      // Closes strema 9
  CheckResponseData(&trans_3, "and hello 0!");  // Closes stream 2, sends RST

  mock_quic_data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

// Test that NetworkIsolationKey is respected by QUIC connections, when
// kPartitionConnectionsByNetworkIsolationKey is enabled.
TEST_P(QuicNetworkTransactionTest, NetworkIsolation) {
  const auto kOrigin1 = url::Origin::Create(GURL("http://origin1/"));
  const auto kOrigin2 = url::Origin::Create(GURL("http://origin2/"));
  NetworkIsolationKey network_isolation_key1(kOrigin1, kOrigin1);
  NetworkIsolationKey network_isolation_key2(kOrigin2, kOrigin2);

  context_.params()->origins_to_force_quic_on.insert(
      HostPortPair::FromString("mail.example.org:443"));

  // Whether to use an H2 proxy. When false, uses HTTPS H2 requests without a
  // proxy, when true, uses HTTP requests over an H2 proxy. It's unnecessary to
  // test tunneled HTTPS over an H2 proxy, since that path sets up H2 sessions
  // the same way as the HTTP over H2 proxy case.
  for (bool use_proxy : {false, true}) {
    SCOPED_TRACE(use_proxy);

    if (use_proxy) {
      proxy_resolution_service_ =
          ConfiguredProxyResolutionService::CreateFixedFromPacResult(
              "QUIC mail.example.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);
    } else {
      proxy_resolution_service_ =
          ConfiguredProxyResolutionService::CreateDirect();
    }

    GURL url1;
    GURL url2;
    GURL url3;
    if (use_proxy) {
      url1 = GURL("http://mail.example.org/1");
      url2 = GURL("http://mail.example.org/2");
      url3 = GURL("http://mail.example.org/3");
    } else {
      url1 = GURL("https://mail.example.org/1");
      url2 = GURL("https://mail.example.org/2");
      url3 = GURL("https://mail.example.org/3");
    }

    for (bool partition_connections : {false, true}) {
      SCOPED_TRACE(partition_connections);

      base::test::ScopedFeatureList feature_list;
      if (partition_connections) {
        feature_list.InitAndEnableFeature(
            features::kPartitionConnectionsByNetworkIsolationKey);
      } else {
        feature_list.InitAndDisableFeature(
            features::kPartitionConnectionsByNetworkIsolationKey);
      }

      // Reads and writes for the unpartitioned case, where only one socket is
      // used.

      context_.params()->origins_to_force_quic_on.insert(
          HostPortPair::FromString("mail.example.org:443"));

      MockQuicData unpartitioned_mock_quic_data(version_);
      QuicTestPacketMaker client_maker1(
          version_,
          quic::QuicUtils::CreateRandomConnectionId(
              context_.random_generator()),
          context_.clock(), kDefaultServerHostName,
          quic::Perspective::IS_CLIENT,
          client_headers_include_h2_stream_dependency_);
      QuicTestPacketMaker server_maker1(
          version_,
          quic::QuicUtils::CreateRandomConnectionId(
              context_.random_generator()),
          context_.clock(), kDefaultServerHostName,
          quic::Perspective::IS_SERVER, false);

      int packet_num = 1;
      if (VersionUsesHttp3(version_.transport_version)) {
        unpartitioned_mock_quic_data.AddWrite(
            SYNCHRONOUS, client_maker1.MakeInitialSettingsPacket(packet_num++));
      }

      unpartitioned_mock_quic_data.AddWrite(
          SYNCHRONOUS,
          client_maker1.MakeRequestHeadersPacket(
              packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
              true, ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
              GetRequestHeaders("GET", url1.scheme(), "/1"), 0, nullptr));
      unpartitioned_mock_quic_data.AddRead(
          ASYNC, server_maker1.MakeResponseHeadersPacket(
                     1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     false, GetResponseHeaders("200 OK"), nullptr));
      unpartitioned_mock_quic_data.AddRead(
          ASYNC, server_maker1.MakeDataPacket(
                     2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     true, ConstructDataHeader(1) + "1"));
      unpartitioned_mock_quic_data.AddWrite(
          SYNCHRONOUS, ConstructClientAckPacket(packet_num++, 2, 1, 1));

      unpartitioned_mock_quic_data.AddWrite(
          SYNCHRONOUS,
          client_maker1.MakeRequestHeadersPacket(
              packet_num++, GetNthClientInitiatedBidirectionalStreamId(1),
              false, true,
              ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
              GetRequestHeaders("GET", url2.scheme(), "/2"), 0, nullptr));
      unpartitioned_mock_quic_data.AddRead(
          ASYNC, server_maker1.MakeResponseHeadersPacket(
                     3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                     false, GetResponseHeaders("200 OK"), nullptr));
      unpartitioned_mock_quic_data.AddRead(
          ASYNC, server_maker1.MakeDataPacket(
                     4, GetNthClientInitiatedBidirectionalStreamId(1), false,
                     true, ConstructDataHeader(1) + "2"));
      unpartitioned_mock_quic_data.AddWrite(
          SYNCHRONOUS,
          ConstructClientAckAndConnectionClosePacket(packet_num++, 4, 3, 1));

      unpartitioned_mock_quic_data.AddWrite(
          SYNCHRONOUS,
          client_maker1.MakeRequestHeadersPacket(
              packet_num++, GetNthClientInitiatedBidirectionalStreamId(2),
              false, true,
              ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
              GetRequestHeaders("GET", url3.scheme(), "/3"), 0, nullptr));
      unpartitioned_mock_quic_data.AddRead(
          ASYNC, server_maker1.MakeResponseHeadersPacket(
                     5, GetNthClientInitiatedBidirectionalStreamId(2), false,
                     false, GetResponseHeaders("200 OK"), nullptr));
      unpartitioned_mock_quic_data.AddRead(
          ASYNC, server_maker1.MakeDataPacket(
                     6, GetNthClientInitiatedBidirectionalStreamId(2), false,
                     true, ConstructDataHeader(1) + "3"));
      unpartitioned_mock_quic_data.AddWrite(
          SYNCHRONOUS,
          ConstructClientAckAndConnectionClosePacket(packet_num++, 6, 5, 1));

      unpartitioned_mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

      // Reads and writes for the partitioned case, where two sockets are used.

      MockQuicData partitioned_mock_quic_data1(version_);
      QuicTestPacketMaker client_maker2(
          version_,
          quic::QuicUtils::CreateRandomConnectionId(
              context_.random_generator()),
          context_.clock(), kDefaultServerHostName,
          quic::Perspective::IS_CLIENT,
          client_headers_include_h2_stream_dependency_);
      QuicTestPacketMaker server_maker2(
          version_,
          quic::QuicUtils::CreateRandomConnectionId(
              context_.random_generator()),
          context_.clock(), kDefaultServerHostName,
          quic::Perspective::IS_SERVER, false);

      int packet_num2 = 1;
      if (VersionUsesHttp3(version_.transport_version)) {
        partitioned_mock_quic_data1.AddWrite(
            SYNCHRONOUS,
            client_maker2.MakeInitialSettingsPacket(packet_num2++));
      }

      partitioned_mock_quic_data1.AddWrite(
          SYNCHRONOUS,
          client_maker2.MakeRequestHeadersPacket(
              packet_num2++, GetNthClientInitiatedBidirectionalStreamId(0),
              true, true,
              ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
              GetRequestHeaders("GET", url1.scheme(), "/1"), 0, nullptr));
      partitioned_mock_quic_data1.AddRead(
          ASYNC, server_maker2.MakeResponseHeadersPacket(
                     1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     false, GetResponseHeaders("200 OK"), nullptr));
      partitioned_mock_quic_data1.AddRead(
          ASYNC, server_maker2.MakeDataPacket(
                     2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     true, ConstructDataHeader(1) + "1"));
      partitioned_mock_quic_data1.AddWrite(
          SYNCHRONOUS, client_maker2.MakeAckPacket(packet_num2++, 2, 1, 1));

      partitioned_mock_quic_data1.AddWrite(
          SYNCHRONOUS,
          client_maker2.MakeRequestHeadersPacket(
              packet_num2++, GetNthClientInitiatedBidirectionalStreamId(1),
              false, true,
              ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
              GetRequestHeaders("GET", url3.scheme(), "/3"), 0, nullptr));
      partitioned_mock_quic_data1.AddRead(
          ASYNC, server_maker2.MakeResponseHeadersPacket(
                     3, GetNthClientInitiatedBidirectionalStreamId(1), false,
                     false, GetResponseHeaders("200 OK"), nullptr));
      partitioned_mock_quic_data1.AddRead(
          ASYNC, server_maker2.MakeDataPacket(
                     4, GetNthClientInitiatedBidirectionalStreamId(1), false,
                     true, ConstructDataHeader(1) + "3"));
      partitioned_mock_quic_data1.AddWrite(
          SYNCHRONOUS, client_maker2.MakeAckPacket(packet_num2++, 4, 3, 1));

      partitioned_mock_quic_data1.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

      MockQuicData partitioned_mock_quic_data2(version_);
      QuicTestPacketMaker client_maker3(
          version_,
          quic::QuicUtils::CreateRandomConnectionId(
              context_.random_generator()),
          context_.clock(), kDefaultServerHostName,
          quic::Perspective::IS_CLIENT,
          client_headers_include_h2_stream_dependency_);
      QuicTestPacketMaker server_maker3(
          version_,
          quic::QuicUtils::CreateRandomConnectionId(
              context_.random_generator()),
          context_.clock(), kDefaultServerHostName,
          quic::Perspective::IS_SERVER, false);

      int packet_num3 = 1;
      if (VersionUsesHttp3(version_.transport_version)) {
        partitioned_mock_quic_data2.AddWrite(
            SYNCHRONOUS,
            client_maker3.MakeInitialSettingsPacket(packet_num3++));
      }

      partitioned_mock_quic_data2.AddWrite(
          SYNCHRONOUS,
          client_maker3.MakeRequestHeadersPacket(
              packet_num3++, GetNthClientInitiatedBidirectionalStreamId(0),
              true, true,
              ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY),
              GetRequestHeaders("GET", url2.scheme(), "/2"), 0, nullptr));
      partitioned_mock_quic_data2.AddRead(
          ASYNC, server_maker3.MakeResponseHeadersPacket(
                     1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     false, GetResponseHeaders("200 OK"), nullptr));
      partitioned_mock_quic_data2.AddRead(
          ASYNC, server_maker3.MakeDataPacket(
                     2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                     true, ConstructDataHeader(1) + "2"));
      partitioned_mock_quic_data2.AddWrite(
          SYNCHRONOUS, client_maker3.MakeAckPacket(packet_num3++, 2, 1, 1));

      partitioned_mock_quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

      if (partition_connections) {
        partitioned_mock_quic_data1.AddSocketDataToFactory(&socket_factory_);
        partitioned_mock_quic_data2.AddSocketDataToFactory(&socket_factory_);
      } else {
        unpartitioned_mock_quic_data.AddSocketDataToFactory(&socket_factory_);
      }

      CreateSession();

      TestCompletionCallback callback;
      HttpRequestInfo request1;
      request1.method = "GET";
      request1.url = GURL(url1);
      request1.traffic_annotation =
          net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
      request1.network_isolation_key = network_isolation_key1;
      HttpNetworkTransaction trans1(LOWEST, session_.get());
      int rv = trans1.Start(&request1, callback.callback(), NetLogWithSource());
      EXPECT_THAT(callback.GetResult(rv), IsOk());
      std::string response_data1;
      EXPECT_THAT(ReadTransaction(&trans1, &response_data1), IsOk());
      EXPECT_EQ("1", response_data1);

      HttpRequestInfo request2;
      request2.method = "GET";
      request2.url = GURL(url2);
      request2.traffic_annotation =
          net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
      request2.network_isolation_key = network_isolation_key2;
      HttpNetworkTransaction trans2(LOWEST, session_.get());
      rv = trans2.Start(&request2, callback.callback(), NetLogWithSource());
      EXPECT_THAT(callback.GetResult(rv), IsOk());
      std::string response_data2;
      EXPECT_THAT(ReadTransaction(&trans2, &response_data2), IsOk());
      EXPECT_EQ("2", response_data2);

      HttpRequestInfo request3;
      request3.method = "GET";
      request3.url = GURL(url3);
      request3.traffic_annotation =
          net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
      request3.network_isolation_key = network_isolation_key1;
      HttpNetworkTransaction trans3(LOWEST, session_.get());
      rv = trans3.Start(&request3, callback.callback(), NetLogWithSource());
      EXPECT_THAT(callback.GetResult(rv), IsOk());
      std::string response_data3;
      EXPECT_THAT(ReadTransaction(&trans3, &response_data3), IsOk());
      EXPECT_EQ("3", response_data3);

      if (partition_connections) {
        EXPECT_TRUE(partitioned_mock_quic_data1.AllReadDataConsumed());
        EXPECT_TRUE(partitioned_mock_quic_data1.AllWriteDataConsumed());
        EXPECT_TRUE(partitioned_mock_quic_data2.AllReadDataConsumed());
        EXPECT_TRUE(partitioned_mock_quic_data2.AllWriteDataConsumed());
      } else {
        EXPECT_TRUE(unpartitioned_mock_quic_data.AllReadDataConsumed());
        EXPECT_TRUE(unpartitioned_mock_quic_data.AllWriteDataConsumed());
      }
    }
  }
}

// Test that two requests to the same origin over QUIC tunnels use different
// QUIC sessions if their NetworkIsolationKeys don't match, and
// kPartitionConnectionsByNetworkIsolationKey is enabled.
TEST_P(QuicNetworkTransactionTest, NetworkIsolationTunnel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  session_params_.enable_quic = true;
  session_params_.enable_quic_proxies_for_https_urls = true;
  proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateFixedFromPacResult(
          "QUIC proxy.example.org:70", TRAFFIC_ANNOTATION_FOR_TESTS);

  const char kGetRequest[] =
      "GET / HTTP/1.1\r\n"
      "Host: mail.example.org\r\n"
      "Connection: keep-alive\r\n\r\n";
  const char kGetResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 10\r\n\r\n";

  std::unique_ptr<MockQuicData> mock_quic_data[2] = {
      std::make_unique<MockQuicData>(version_),
      std::make_unique<MockQuicData>(version_)};

  for (int index : {0, 1}) {
    QuicTestPacketMaker client_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_CLIENT,
        client_headers_include_h2_stream_dependency_);
    QuicTestPacketMaker server_maker(
        version_,
        quic::QuicUtils::CreateRandomConnectionId(context_.random_generator()),
        context_.clock(), kDefaultServerHostName, quic::Perspective::IS_SERVER,
        false);

    int packet_num = 1;
    if (VersionUsesHttp3(version_.transport_version)) {
      mock_quic_data[index]->AddWrite(
          SYNCHRONOUS, client_maker.MakeInitialSettingsPacket(packet_num++));
      mock_quic_data[index]->AddWrite(
          SYNCHRONOUS, client_maker.MakePriorityPacket(
                           packet_num++, true,
                           GetNthClientInitiatedBidirectionalStreamId(0), 0,
                           ConvertRequestPriorityToQuicPriority(
                               HttpProxyConnectJob::kH2QuicTunnelPriority)));
    }

    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS,
        client_maker.MakeRequestHeadersPacket(
            packet_num++, GetNthClientInitiatedBidirectionalStreamId(0), true,
            false,
            VersionUsesHttp3(version_.transport_version)
                ? 1
                : ConvertRequestPriorityToQuicPriority(
                      HttpProxyConnectJob::kH2QuicTunnelPriority),
            ConnectRequestHeaders("mail.example.org:443"), 0, nullptr));
    mock_quic_data[index]->AddRead(
        ASYNC, server_maker.MakeResponseHeadersPacket(
                   1, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   false, GetResponseHeaders("200 OK"), nullptr));

    std::string header = ConstructDataHeader(strlen(kGetRequest));
    if (!version_.HasIetfQuicFrames()) {
      mock_quic_data[index]->AddWrite(
          SYNCHRONOUS, client_maker.MakeAckAndDataPacket(
                           packet_num++, false,
                           GetNthClientInitiatedBidirectionalStreamId(0), 1, 1,
                           1, false, quiche::QuicheStringPiece(kGetRequest)));
    } else {
      mock_quic_data[index]->AddWrite(
          SYNCHRONOUS, client_maker.MakeAckAndDataPacket(
                           packet_num++, false,
                           GetNthClientInitiatedBidirectionalStreamId(0), 1, 1,
                           1, false, {header + std::string(kGetRequest)}));
    }

    std::string header2 = ConstructDataHeader(strlen(kGetResponse));
    mock_quic_data[index]->AddRead(
        ASYNC, server_maker.MakeDataPacket(
                   2, GetNthClientInitiatedBidirectionalStreamId(0), false,
                   false, header2 + std::string(kGetResponse)));
    mock_quic_data[index]->AddRead(
        SYNCHRONOUS,
        server_maker.MakeDataPacket(
            3, GetNthClientInitiatedBidirectionalStreamId(0), false, false,
            ConstructDataHeader(10) + std::string("0123456789")));
    mock_quic_data[index]->AddWrite(
        SYNCHRONOUS, client_maker.MakeAckPacket(packet_num++, 3, 2, 1));
    mock_quic_data[index]->AddRead(SYNCHRONOUS,
                                   ERR_IO_PENDING);  // No more data to read

    mock_quic_data[index]->AddSocketDataToFactory(&socket_factory_);
  }

  socket_factory_.AddSSLSocketDataProvider(&ssl_data_);
  SSLSocketDataProvider ssl_data2(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl_data2);

  CreateSession();

  request_.url = GURL("https://mail.example.org/");
  AddQuicAlternateProtocolMapping(MockCryptoClientStream::CONFIRM_HANDSHAKE);
  HttpNetworkTransaction trans(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans);
  CheckResponseData(&trans, "0123456789");

  HttpRequestInfo request2;
  const auto kOrigin1 = url::Origin::Create(GURL("http://origin1/"));
  request_.network_isolation_key = NetworkIsolationKey(kOrigin1, kOrigin1);
  HttpNetworkTransaction trans2(DEFAULT_PRIORITY, session_.get());
  RunTransaction(&trans2);
  CheckResponseData(&trans2, "0123456789");

  EXPECT_TRUE(mock_quic_data[0]->AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data[0]->AllWriteDataConsumed());
  EXPECT_TRUE(mock_quic_data[1]->AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data[1]->AllWriteDataConsumed());
}

}  // namespace test
}  // namespace net

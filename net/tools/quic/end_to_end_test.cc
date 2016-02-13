// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <sys/epoll.h>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/quic_client_session_base.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/quic_session.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_flow_controller_peer.h"
#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_spdy_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/reliable_quic_stream_peer.h"
#include "net/test/gtest_util.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"
#include "net/tools/quic/quic_server.h"
#include "net/tools/quic/quic_simple_server_stream.h"
#include "net/tools/quic/quic_socket_utils.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "net/tools/quic/test_tools/http_message.h"
#include "net/tools/quic/test_tools/packet_dropping_test_writer.h"
#include "net/tools/quic/test_tools/quic_client_peer.h"
#include "net/tools/quic/test_tools/quic_dispatcher_peer.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "net/tools/quic/test_tools/quic_server_peer.h"
#include "net/tools/quic/test_tools/quic_test_client.h"
#include "net/tools/quic/test_tools/quic_test_server.h"
#include "net/tools/quic/test_tools/server_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::IntToString;
using base::StringPiece;
using base::WaitableEvent;
using net::EpollServer;
using net::IPAddressNumber;
using net::test::ConstructEncryptedPacket;
using net::test::CryptoTestUtils;
using net::test::GenerateBody;
using net::test::Loopback4;
using net::test::MockQuicConnectionDebugVisitor;
using net::test::QuicConnectionPeer;
using net::test::QuicFlowControllerPeer;
using net::test::QuicSentPacketManagerPeer;
using net::test::QuicSessionPeer;
using net::test::QuicSpdySessionPeer;
using net::test::ReliableQuicStreamPeer;
using net::test::ValueRestore;
using net::test::kClientDataStreamId1;
using net::test::kInitialSessionFlowControlWindowForTest;
using net::test::kInitialStreamFlowControlWindowForTest;
using net::test::PacketDroppingTestWriter;
using net::test::QuicDispatcherPeer;
using net::test::QuicServerPeer;
using std::ostream;
using std::string;
using std::vector;

namespace net {
namespace test {
namespace {

const char kFooResponseBody[] = "Artichoke hearts make me happy.";
const char kBarResponseBody[] = "Palm hearts are pretty delicious, also.";
const float kSessionToStreamRatio = 1.5;

// Run all tests with the cross products of all versions.
struct TestParams {
  TestParams(const QuicVersionVector& client_supported_versions,
             const QuicVersionVector& server_supported_versions,
             QuicVersion negotiated_version,
             bool use_fec,
             bool client_supports_stateless_rejects,
             bool server_uses_stateless_rejects_if_peer_supported,
             QuicTag congestion_control_tag,
             bool auto_tune_flow_control_window)
      : client_supported_versions(client_supported_versions),
        server_supported_versions(server_supported_versions),
        negotiated_version(negotiated_version),
        use_fec(use_fec),
        client_supports_stateless_rejects(client_supports_stateless_rejects),
        server_uses_stateless_rejects_if_peer_supported(
            server_uses_stateless_rejects_if_peer_supported),
        congestion_control_tag(congestion_control_tag),
        auto_tune_flow_control_window(auto_tune_flow_control_window) {}

  friend ostream& operator<<(ostream& os, const TestParams& p) {
    os << "{ server_supported_versions: "
       << QuicVersionVectorToString(p.server_supported_versions);
    os << " client_supported_versions: "
       << QuicVersionVectorToString(p.client_supported_versions);
    os << " negotiated_version: " << QuicVersionToString(p.negotiated_version);
    os << " client_supports_stateless_rejects: "
       << p.client_supports_stateless_rejects;
    os << " server_uses_stateless_rejects_if_peer_supported: "
       << p.server_uses_stateless_rejects_if_peer_supported;
    os << " use_fec: " << p.use_fec;
    os << " congestion_control_tag: "
       << QuicUtils::TagToString(p.congestion_control_tag);
    os << " auto_tune_flow_control_window: " << p.auto_tune_flow_control_window
       << " }";
    return os;
  }

  QuicVersionVector client_supported_versions;
  QuicVersionVector server_supported_versions;
  QuicVersion negotiated_version;
  bool use_fec;
  bool client_supports_stateless_rejects;
  bool server_uses_stateless_rejects_if_peer_supported;
  QuicTag congestion_control_tag;
  bool auto_tune_flow_control_window;
};

// Constructs various test permutations.
vector<TestParams> GetTestParams() {
  // Divide the versions into buckets in which the intra-frame format
  // is compatible. When clients encounter QUIC version negotiation
  // they simply retransmit all packets using the new version's
  // QUIC framing. However, they are unable to change the intra-frame
  // layout (for example to change SPDY/4 headers to SPDY/3). So
  // these tests need to ensure that clients are never attempting
  // to do 0-RTT across incompatible versions. Chromium only supports
  // a single version at a time anyway. :)
  QuicVersionVector all_supported_versions = QuicSupportedVersions();
  QuicVersionVector version_buckets[2];
  for (const QuicVersion version : all_supported_versions) {
    if (version <= QUIC_VERSION_25) {
      // SPDY/4
      version_buckets[0].push_back(version);
    } else {
      // QUIC_VERSION_26 changes the kdf in a way that is incompatible with
      // version negotiation across the version 26 boundary.
      version_buckets[1].push_back(version);
    }
  }

  vector<TestParams> params;
  for (bool server_uses_stateless_rejects_if_peer_supported : {true, false}) {
    for (bool client_supports_stateless_rejects : {true, false}) {
      // TODO(rtenneti): Add kTBBR after BBR code is checked in.
      for (const QuicTag congestion_control_tag : {kRENO, kQBIC}) {
        for (bool auto_tune_flow_control_window : {true, false}) {
          for (const bool use_fec : {false, true}) {
            const int kMaxEnabledOptions = 5;
            int enabled_options = 0;
            if (congestion_control_tag != kQBIC) {
              ++enabled_options;
            }
            if (use_fec) {
              ++enabled_options;
            }
            if (auto_tune_flow_control_window) {
              ++enabled_options;
            }
            if (client_supports_stateless_rejects) {
              ++enabled_options;
            }
            if (server_uses_stateless_rejects_if_peer_supported) {
              ++enabled_options;
            }
            CHECK_GE(kMaxEnabledOptions, enabled_options);

            // Run tests with no options, a single option, or all the options
            // enabled to avoid a combinatorial explosion.
            if (enabled_options > 1 && enabled_options < kMaxEnabledOptions) {
              continue;
            }

            for (const QuicVersionVector& client_versions : version_buckets) {
              CHECK(!client_versions.empty());
              // Add an entry for server and client supporting all versions.
              params.push_back(TestParams(
                  client_versions, all_supported_versions,
                  client_versions.front(), use_fec,
                  client_supports_stateless_rejects,
                  server_uses_stateless_rejects_if_peer_supported,
                  congestion_control_tag, auto_tune_flow_control_window));

              // Run version negotiation tests tests with no options, or all
              // the options enabled to avoid a combinatorial explosion.
              if (enabled_options > 0 && enabled_options < kMaxEnabledOptions) {
                continue;
              }

              // Test client supporting all versions and server supporting 1
              // version. Simulate an old server and exercise version downgrade
              // in the client. Protocol negotiation should occur. Skip the i =
              // 0 case because it is essentially the same as the default case.
              for (size_t i = 1; i < client_versions.size(); ++i) {
                QuicVersionVector server_supported_versions;
                server_supported_versions.push_back(client_versions[i]);
                params.push_back(TestParams(
                    client_versions, server_supported_versions,
                    server_supported_versions.front(), use_fec,
                    client_supports_stateless_rejects,
                    server_uses_stateless_rejects_if_peer_supported,
                    congestion_control_tag, auto_tune_flow_control_window));
              }
            }
          }
        }
      }
    }
  }
  return params;
}

class ServerDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ServerDelegate(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}
  ~ServerDelegate() override {}
  void OnCanWrite() override { dispatcher_->OnCanWrite(); }

 private:
  QuicDispatcher* dispatcher_;
};

class ClientDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ClientDelegate(QuicClient* client) : client_(client) {}
  ~ClientDelegate() override {}
  void OnCanWrite() override {
    EpollEvent event(EPOLLOUT, false);
    client_->OnEvent(client_->GetLatestFD(), &event);
  }

 private:
  QuicClient* client_;
};

class EndToEndTest : public ::testing::TestWithParam<TestParams> {
 protected:
  EndToEndTest()
      : initialized_(false),
        server_address_(IPEndPoint(Loopback4(), 0)),
        server_hostname_("example.com"),
        server_started_(false),
        strike_register_no_startup_period_(false),
        chlo_multiplier_(0),
        stream_factory_(nullptr) {
    client_supported_versions_ = GetParam().client_supported_versions;
    server_supported_versions_ = GetParam().server_supported_versions;
    negotiated_version_ = GetParam().negotiated_version;
    FLAGS_enable_quic_fec = GetParam().use_fec;

    VLOG(1) << "Using Configuration: " << GetParam();

    // Use different flow control windows for client/server.
    client_config_.SetInitialStreamFlowControlWindowToSend(
        2 * kInitialStreamFlowControlWindowForTest);
    client_config_.SetInitialSessionFlowControlWindowToSend(
        2 * kInitialSessionFlowControlWindowForTest);
    server_config_.SetInitialStreamFlowControlWindowToSend(
        3 * kInitialStreamFlowControlWindowForTest);
    server_config_.SetInitialSessionFlowControlWindowToSend(
        3 * kInitialSessionFlowControlWindowForTest);

    QuicInMemoryCachePeer::ResetForTests();
    AddToCache("/foo", 200, kFooResponseBody);
    AddToCache("/bar", 200, kBarResponseBody);
  }

  ~EndToEndTest() override {
    // TODO(rtenneti): port RecycleUnusedPort if needed.
    // RecycleUnusedPort(server_address_.port());
    QuicInMemoryCachePeer::ResetForTests();
  }

  QuicTestClient* CreateQuicClient(QuicPacketWriterWrapper* writer) {
    QuicTestClient* client =
        new QuicTestClient(server_address_, server_hostname_, client_config_,
                           client_supported_versions_);
    client->UseWriter(writer);
    client->Connect();
    return client;
  }

  void set_smaller_flow_control_receive_window() {
    const uint32_t kClientIFCW = 64 * 1024;
    const uint32_t kServerIFCW = 1024 * 1024;
    set_client_initial_stream_flow_control_receive_window(kClientIFCW);
    set_client_initial_session_flow_control_receive_window(
        kSessionToStreamRatio * kClientIFCW);
    set_server_initial_stream_flow_control_receive_window(kServerIFCW);
    set_server_initial_session_flow_control_receive_window(
        kSessionToStreamRatio * kServerIFCW);
  }

  void set_client_initial_stream_flow_control_receive_window(uint32_t window) {
    CHECK(client_.get() == nullptr);
    DVLOG(1) << "Setting client initial stream flow control window: " << window;
    client_config_.SetInitialStreamFlowControlWindowToSend(window);
  }

  void set_client_initial_session_flow_control_receive_window(uint32_t window) {
    CHECK(client_.get() == nullptr);
    DVLOG(1) << "Setting client initial session flow control window: "
             << window;
    client_config_.SetInitialSessionFlowControlWindowToSend(window);
  }

  void set_server_initial_stream_flow_control_receive_window(uint32_t window) {
    CHECK(server_thread_.get() == nullptr);
    DVLOG(1) << "Setting server initial stream flow control window: " << window;
    server_config_.SetInitialStreamFlowControlWindowToSend(window);
  }

  void set_server_initial_session_flow_control_receive_window(uint32_t window) {
    CHECK(server_thread_.get() == nullptr);
    DVLOG(1) << "Setting server initial session flow control window: "
             << window;
    server_config_.SetInitialSessionFlowControlWindowToSend(window);
  }

  const QuicSentPacketManager* GetSentPacketManagerFromFirstServerSession()
      const {
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    QuicSession* session = dispatcher->session_map().begin()->second;
    return &session->connection()->sent_packet_manager();
  }

  bool Initialize() {
    QuicTagVector copt;
    server_config_.SetConnectionOptionsToSend(copt);

    // TODO(nimia): Consider setting the congestion control algorithm for the
    // client as well according to the test parameter.
    copt.push_back(GetParam().congestion_control_tag);

    if (GetParam().use_fec) {
      // Set FEC config in client's connection options and in client session.
      copt.push_back(kFHDR);
    }
    if (GetParam().client_supports_stateless_rejects) {
      copt.push_back(kSREJ);
    }
    if (GetParam().auto_tune_flow_control_window) {
      copt.push_back(kAFCW);
      copt.push_back(kIFW5);
    }
    client_config_.SetConnectionOptionsToSend(copt);

    // Start the server first, because CreateQuicClient() attempts
    // to connect to the server.
    StartServer();

    client_.reset(CreateQuicClient(client_writer_));
    if (GetParam().use_fec) {
      // Set FecPolicy to always protect data on all streams.
      client_->SetFecPolicy(FEC_PROTECT_ALWAYS);
    }
    static EpollEvent event(EPOLLOUT, false);
    client_writer_->Initialize(
        reinterpret_cast<QuicEpollConnectionHelper*>(
            QuicConnectionPeer::GetHelper(
                client_->client()->session()->connection())),
        new ClientDelegate(client_->client()));
    initialized_ = true;
    return client_->client()->connected();
  }

  void SetUp() override {
    // The ownership of these gets transferred to the QuicPacketWriterWrapper
    // when Initialize() is executed.
    client_writer_ = new PacketDroppingTestWriter();
    server_writer_ = new PacketDroppingTestWriter();
  }

  void TearDown() override {
    ASSERT_TRUE(initialized_) << "You must call Initialize() in every test "
                              << "case. Otherwise, your test will leak memory.";
    StopServer();
  }

  void StartServer() {
    server_thread_.reset(new ServerThread(
        new QuicTestServer(CryptoTestUtils::ProofSourceForTesting(),
                           server_config_, server_supported_versions_),
        server_address_, strike_register_no_startup_period_));
    if (chlo_multiplier_ != 0) {
      server_thread_->server()->SetChloMultiplier(chlo_multiplier_);
    }
    server_thread_->Initialize();
    server_address_ = IPEndPoint(server_address_.address().bytes(),
                                 server_thread_->GetPort());
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    QuicDispatcherPeer::UseWriter(dispatcher, server_writer_);

    FLAGS_enable_quic_stateless_reject_support =
        GetParam().server_uses_stateless_rejects_if_peer_supported;

    server_writer_->Initialize(QuicDispatcherPeer::GetHelper(dispatcher),
                               new ServerDelegate(dispatcher));
    if (stream_factory_ != nullptr) {
      static_cast<QuicTestServer*>(server_thread_->server())
          ->SetSpdyStreamFactory(stream_factory_);
    }

    server_thread_->Start();
    server_started_ = true;
  }

  void StopServer() {
    if (!server_started_)
      return;
    if (server_thread_.get()) {
      server_thread_->Quit();
      server_thread_->Join();
    }
  }

  void AddToCache(StringPiece path, int response_code, StringPiece body) {
    QuicInMemoryCache::GetInstance()->AddSimpleResponse("www.google.com", path,
                                                        response_code, body);
  }

  void SetPacketLossPercentage(int32_t loss) {
    // TODO(rtenneti): enable when we can do random packet loss tests in
    // chrome's tree.
    if (loss != 0 && loss != 100)
      return;
    client_writer_->set_fake_packet_loss_percentage(loss);
    server_writer_->set_fake_packet_loss_percentage(loss);
  }

  void SetPacketSendDelay(QuicTime::Delta delay) {
    // TODO(rtenneti): enable when we can do random packet send delay tests in
    // chrome's tree.
    // client_writer_->set_fake_packet_delay(delay);
    // server_writer_->set_fake_packet_delay(delay);
  }

  void SetReorderPercentage(int32_t reorder) {
    // TODO(rtenneti): enable when we can do random packet reorder tests in
    // chrome's tree.
    // client_writer_->set_fake_reorder_percentage(reorder);
    // server_writer_->set_fake_reorder_percentage(reorder);
  }

  // Verifies that the client and server connections were both free of packets
  // being discarded, based on connection stats.
  // Calls server_thread_ Pause() and Resume(), which may only be called once
  // per test.
  void VerifyCleanConnection(bool /*had_packet_loss*/) {
    QuicConnectionStats client_stats =
        client_->client()->session()->connection()->GetStats();
    // TODO(ianswett): Re-enable this check once b/19572432 is fixed.
    // if (!had_packet_loss) {
    //   EXPECT_EQ(0u, client_stats.packets_lost);
    // }
    EXPECT_EQ(0u, client_stats.packets_discarded);
    // When doing 0-RTT with stateless rejects, the encrypted requests cause
    // a retranmission of the SREJ packets which are dropped by the client.
    if (!BothSidesSupportStatelessRejects()) {
      EXPECT_EQ(0u, client_stats.packets_dropped);
    }
    EXPECT_EQ(client_stats.packets_received, client_stats.packets_processed);

    const int num_expected_stateless_rejects =
        (BothSidesSupportStatelessRejects() &&
         client_->client()->session()->GetNumSentClientHellos() > 0)
            ? 1
            : 0;
    EXPECT_EQ(num_expected_stateless_rejects,
              client_->client()->num_stateless_rejects_received());

    server_thread_->Pause();
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    ASSERT_EQ(1u, dispatcher->session_map().size());
    QuicSession* session = dispatcher->session_map().begin()->second;
    QuicConnectionStats server_stats = session->connection()->GetStats();
    // TODO(ianswett): Re-enable this check once b/19572432 is fixed.
    // if (!had_packet_loss) {
    //   EXPECT_EQ(0u, server_stats.packets_lost);
    // }
    EXPECT_EQ(0u, server_stats.packets_discarded);
    // TODO(ianswett): Restore the check for packets_dropped equals 0.
    // The expect for packets received is equal to packets processed fails
    // due to version negotiation packets.
    server_thread_->Resume();
  }

  bool BothSidesSupportStatelessRejects() {
    return (GetParam().server_uses_stateless_rejects_if_peer_supported &&
            GetParam().client_supports_stateless_rejects);
  }

  void ExpectFlowControlsSynced(QuicFlowController* client,
                                QuicFlowController* server) {
    EXPECT_EQ(QuicFlowControllerPeer::SendWindowSize(client),
              QuicFlowControllerPeer::ReceiveWindowSize(server));
    EXPECT_EQ(QuicFlowControllerPeer::ReceiveWindowSize(client),
              QuicFlowControllerPeer::SendWindowSize(server));
  }

  // Must be called before Initialize to have effect.
  void SetSpdyStreamFactory(QuicTestServer::StreamFactory* factory) {
    stream_factory_ = factory;
  }

  bool initialized_;
  IPEndPoint server_address_;
  string server_hostname_;
  scoped_ptr<ServerThread> server_thread_;
  scoped_ptr<QuicTestClient> client_;
  PacketDroppingTestWriter* client_writer_;
  PacketDroppingTestWriter* server_writer_;
  bool server_started_;
  QuicConfig client_config_;
  QuicConfig server_config_;
  QuicVersionVector client_supported_versions_;
  QuicVersionVector server_supported_versions_;
  QuicVersion negotiated_version_;
  bool strike_register_no_startup_period_;
  size_t chlo_multiplier_;
  QuicTestServer::StreamFactory* stream_factory_;
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_CASE_P(EndToEndTests,
                        EndToEndTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(EndToEndTest, SimpleRequestResponse) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());
}

TEST_P(EndToEndTest, SimpleRequestResponseWithLargeReject) {
  chlo_multiplier_ = 1;
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  EXPECT_EQ(3, client_->client()->GetNumSentClientHellos());
}

// TODO(rch): figure out how to detect missing v6 supprt (like on the linux
// try bots) and selectively disable this test.
TEST_P(EndToEndTest, DISABLED_SimpleRequestResponsev6) {
  IPAddressNumber ip;
  CHECK(net::ParseIPLiteralToNumber("::1", &ip));
  server_address_ = IPEndPoint(ip, server_address_.port());
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, SeparateFinPacket) {
  ASSERT_TRUE(Initialize());

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.set_has_complete_message(false);

  // Send a request in two parts: the request and then an empty packet with FIN.
  client_->SendMessage(request);
  client_->SendData("", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Now do the same thing but with a content length.
  request.AddBody("foo", true);
  client_->SendMessage(request);
  client_->SendData("", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, MultipleRequestResponse) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, MultipleClients) {
  ASSERT_TRUE(Initialize());
  scoped_ptr<QuicTestClient> client2(CreateQuicClient(nullptr));

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_has_complete_message(false);

  client_->SendMessage(request);
  client2->SendMessage(request);

  client_->SendData("bar", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  client2->SendData("eep", true);
  client2->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client2->response_body());
  EXPECT_EQ(200u, client2->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, RequestOverMultiplePackets) {
  // Send a large enough request to guarantee fragmentation.
  string huge_request = "/some/path?query=" + string(kMaxPacketSize, '.');
  AddToCache(huge_request, 200, kBarResponseBody);

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest(huge_request));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, MultiplePacketsRandomOrder) {
  // Send a large enough request to guarantee fragmentation.
  string huge_request = "/some/path?query=" + string(kMaxPacketSize, '.');
  AddToCache(huge_request, 200, kBarResponseBody);

  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(50);

  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest(huge_request));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, PostMissingBytes) {
  ASSERT_TRUE(Initialize());

  // Add a content length header with no body.
  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_skip_message_validation(true);

  // This should be detected as stream fin without complete request,
  // triggering an error response.
  client_->SendCustomSynchronousRequest(request);
  EXPECT_EQ(QuicSimpleServerStream::kErrorResponseBody,
            client_->response_body());
  EXPECT_EQ(500u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, LargePostNoPacketLoss) {
  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // 1 MB body.
  string body;
  GenerateBody(&body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostNoPacketLoss1sRTT) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(1000));

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // 100 KB body.
  string body;
  GenerateBody(&body, 100 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostWithPacketLoss) {
  if (!BothSidesSupportStatelessRejects()) {
    // Connect with lower fake packet loss than we'd like to test.
    // Until b/10126687 is fixed, losing handshake packets is pretty
    // brutal.
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(5);
  }
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  SetPacketLossPercentage(30);

  // 10 KB body.
  string body;
  GenerateBody(&body, 1024 * 10);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTest, LargePostWithPacketLossAndBlockedSocket) {
  if (!BothSidesSupportStatelessRejects()) {
    // Connect with lower fake packet loss than we'd like to test.  Until
    // b/10126687 is fixed, losing handshake packets is pretty brutal.
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(5);
  }
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  SetPacketLossPercentage(10);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // 10 KB body.
  string body;
  GenerateBody(&body, 1024 * 10);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, LargePostNoPacketLossWithDelayAndReordering) {
  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();
  // Both of these must be called when the writer is not actively used.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // 1 MB body.
  string body;
  GenerateBody(&body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, LargePostZeroRTTFailure) {
  // Have the server accept 0-RTT without waiting a startup period.
  strike_register_no_startup_period_ = true;

  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  string body;
  GenerateBody(&body, 20480);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos on the latest
  // session is 1.
  const int expected_num_hellos_latest_session =
      BothSidesSupportStatelessRejects() ? 1 : 2;
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  client_->WaitForResponseForMs(-1);
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  if (FLAGS_require_strike_register_or_server_nonce) {
    EXPECT_EQ(expected_num_hellos_latest_session,
              client_->client()->session()->GetNumSentClientHellos());
    EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());
  } else {
    EXPECT_EQ(1, client_->client()->session()->GetNumSentClientHellos());
    EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());
  }

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos sent on the
  // latest session is 1.
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, SynchronousRequestZeroRTTFailure) {
  // Have the server accept 0-RTT without waiting a startup period.
  strike_register_no_startup_period_ = true;

  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos on that second
  // latest session is 1.
  const int expected_num_hellos_latest_session =
      BothSidesSupportStatelessRejects() ? 1 : 2;
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  client_->WaitForInitialResponse();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  if (FLAGS_require_strike_register_or_server_nonce) {
    EXPECT_EQ(expected_num_hellos_latest_session,
              client_->client()->session()->GetNumSentClientHellos());
    EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());
  } else {
    EXPECT_EQ(1, client_->client()->session()->GetNumSentClientHellos());
    EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());
  }

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos sent on the
  // latest session is 1.
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostSynchronousRequest) {
  // Have the server accept 0-RTT without waiting a startup period.
  strike_register_no_startup_period_ = true;

  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  string body;
  GenerateBody(&body, 20480);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos on the latest
  // session is 1.
  const int expected_num_hellos_latest_session =
      BothSidesSupportStatelessRejects() ? 1 : 2;
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  client_->WaitForInitialResponse();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  if (FLAGS_require_strike_register_or_server_nonce) {
    EXPECT_EQ(expected_num_hellos_latest_session,
              client_->client()->session()->GetNumSentClientHellos());
    EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());
  } else {
    EXPECT_EQ(1, client_->client()->session()->GetNumSentClientHellos());
    EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());
  }

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos sent on the
  // latest session is 1.
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, StatelessRejectWithPacketLoss) {
  // In this test, we intentionally drop the first packet from the
  // server, which corresponds with the initial REJ/SREJ response from
  // the server.
  server_writer_->set_fake_drop_first_n_packets(1);
  ASSERT_TRUE(Initialize());
}

TEST_P(EndToEndTest, SetInitialReceivedConnectionOptions) {
  QuicTagVector initial_received_options;
  initial_received_options.push_back(kTBBR);
  initial_received_options.push_back(kIW10);
  initial_received_options.push_back(kPRST);
  EXPECT_TRUE(server_config_.SetInitialReceivedConnectionOptions(
      initial_received_options));

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  EXPECT_FALSE(server_config_.SetInitialReceivedConnectionOptions(
      initial_received_options));

  // Verify that server's configuration is correct.
  server_thread_->Pause();
  EXPECT_TRUE(server_config_.HasReceivedConnectionOptions());
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kTBBR));
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kIW10));
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kPRST));
}

TEST_P(EndToEndTest, CorrectlyConfiguredFec) {
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  FecPolicy expected_policy =
      GetParam().use_fec ? FEC_PROTECT_ALWAYS : FEC_PROTECT_OPTIONAL;

  // Verify that server's FEC configuration is correct.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  QuicSpdySession* session = dispatcher->session_map().begin()->second;
  EXPECT_EQ(expected_policy,
            QuicSpdySessionPeer::GetHeadersStream(session)->fec_policy());
  server_thread_->Resume();

  // Verify that client's FEC configuration is correct.
  EXPECT_EQ(expected_policy,
            QuicSpdySessionPeer::GetHeadersStream(client_->client()->session())
                ->fec_policy());
  EXPECT_EQ(expected_policy, client_->GetOrCreateStream()->fec_policy());
}

TEST_P(EndToEndTest, LargePostSmallBandwidthLargeBuffer) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMicroseconds(1));
  // 256KB per second with a 256KB buffer from server to client.  Wireless
  // clients commonly have larger buffers, but our max CWND is 200.
  server_writer_->set_max_bandwidth_and_buffer_size(
      QuicBandwidth::FromBytesPerSecond(256 * 1024), 256 * 1024);

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // 1 MB body.
  string body;
  GenerateBody(&body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  // This connection will not drop packets, because the buffer size is larger
  // than the default receive window.
  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, DoNotSetResumeWriteAlarmIfConnectionFlowControlBlocked) {
  // Regression test for b/14677858.
  // Test that the resume write alarm is not set in QuicConnection::OnCanWrite
  // if currently connection level flow control blocked. If set, this results in
  // an infinite loop in the EpollServer, as the alarm fires and is immediately
  // rescheduled.
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  // Ensure both stream and connection level are flow control blocked by setting
  // the send window offset to 0.
  const uint64_t flow_control_window =
      server_config_.GetInitialStreamFlowControlWindowToSend();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  QuicSession* session = client_->client()->session();
  QuicFlowControllerPeer::SetSendWindowOffset(stream->flow_controller(), 0);
  QuicFlowControllerPeer::SetSendWindowOffset(session->flow_controller(), 0);
  EXPECT_TRUE(stream->flow_controller()->IsBlocked());
  EXPECT_TRUE(session->flow_controller()->IsBlocked());

  // Make sure that the stream has data pending so that it will be marked as
  // write blocked when it receives a stream level WINDOW_UPDATE.
  stream->SendBody("hello", false);

  // The stream now attempts to write, fails because it is still connection
  // level flow control blocked, and is added to the write blocked list.
  QuicWindowUpdateFrame window_update(stream->id(), 2 * flow_control_window);
  stream->OnWindowUpdateFrame(window_update);

  // Prior to fixing b/14677858 this call would result in an infinite loop in
  // Chromium. As a proxy for detecting this, we now check whether the
  // resume_writes_alarm is set after OnCanWrite. It should not be, as the
  // connection is still flow control blocked.
  session->connection()->OnCanWrite();

  QuicAlarm* resume_writes_alarm =
      QuicConnectionPeer::GetResumeWritesAlarm(session->connection());
  EXPECT_FALSE(resume_writes_alarm->IsSet());
}

TEST_P(EndToEndTest, InvalidStream) {
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  string body;
  GenerateBody(&body, kMaxPacketSize);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(body, true);
  // Force the client to write with a stream ID belonging to a nonexistent
  // server-side stream.
  QuicSessionPeer::SetNextOutgoingStreamId(client_->client()->session(), 2);

  client_->SendCustomSynchronousRequest(request);
  // EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_INVALID_STREAM_ID, client_->connection_error());
}

TEST_P(EndToEndTest, EarlyResponseWithQuicStreamNoError) {
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  string large_body;
  GenerateBody(&large_body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddBody(large_body, false);

  // Insert an invalid content_length field in request to trigger an early
  // response from server.
  request.AddHeader("content-length", "-3");

  request.set_skip_message_validation(true);
  client_->SendCustomSynchronousRequest(request);
  EXPECT_EQ("bad", client_->response_body());
  EXPECT_EQ(500u, client_->response_headers()->parsed_response_code());
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

// TODO(rch): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, DISABLED_MultipleTermination) {
  ASSERT_TRUE(Initialize());

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_has_complete_message(false);

  // Set the offset so we won't frame.  Otherwise when we pick up termination
  // before HTTP framing is complete, we send an error and close the stream,
  // and the second write is picked up as writing on a closed stream.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream != nullptr);
  ReliableQuicStreamPeer::SetStreamBytesWritten(3, stream);

  client_->SendData("bar", true);
  client_->WaitForWriteToFlush();

  // By default the stream protects itself from writes after terminte is set.
  // Override this to test the server handling buggy clients.
  ReliableQuicStreamPeer::SetWriteSideClosed(false,
                                             client_->GetOrCreateStream());

  EXPECT_DFATAL(client_->SendData("eep", true), "Fin already buffered");
}

TEST_P(EndToEndTest, Timeout) {
  client_config_.SetIdleConnectionStateLifetime(
      QuicTime::Delta::FromMicroseconds(500),
      QuicTime::Delta::FromMicroseconds(500));
  // Note: we do NOT ASSERT_TRUE: we may time out during initial handshake:
  // that's enough to validate timeout in this case.
  Initialize();
  while (client_->client()->connected()) {
    client_->client()->WaitForEvents();
  }
}

TEST_P(EndToEndTest, NegotiateMaxOpenStreams) {
  // Negotiate 1 max open stream.
  client_config_.SetMaxStreamsPerConnection(1, 1);
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  // Make the client misbehave after negotiation.
  const int kServerMaxStreams = kMaxStreamsMinimumIncrement + 1;
  QuicSessionPeer::SetMaxOpenOutgoingStreams(client_->client()->session(),
                                             kServerMaxStreams + 1);

  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_has_complete_message(false);

  // The server supports a small number of additional streams beyond the
  // negotiated limit. Open enough streams to go beyond that limit.
  for (int i = 0; i < kServerMaxStreams + 1; ++i) {
    client_->SendMessage(request);
  }
  client_->WaitForResponse();

  if (negotiated_version_ <= QUIC_VERSION_27) {
    EXPECT_FALSE(client_->connected());
    EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
    EXPECT_EQ(QUIC_TOO_MANY_OPEN_STREAMS, client_->connection_error());
  } else {
    EXPECT_TRUE(client_->connected());
    EXPECT_EQ(QUIC_REFUSED_STREAM, client_->stream_error());
    EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
  }
}

TEST_P(EndToEndTest, NegotiateCongestionControl) {
  ValueRestore<bool> old_flag(&FLAGS_quic_allow_bbr, true);
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  CongestionControlType expected_congestion_control_type = kReno;
  switch (GetParam().congestion_control_tag) {
    case kRENO:
      expected_congestion_control_type = kReno;
      break;
    case kTBBR:
      expected_congestion_control_type = kBBR;
      break;
    case kQBIC:
      expected_congestion_control_type = kCubic;
      break;
    default:
      DLOG(FATAL) << "Unexpected congestion control tag";
  }

  EXPECT_EQ(expected_congestion_control_type,
            QuicSentPacketManagerPeer::GetSendAlgorithm(
                *GetSentPacketManagerFromFirstServerSession())
                ->GetCongestionControlType());
}

TEST_P(EndToEndTest, LimitMaxOpenStreams) {
  // Server limits the number of max streams to 2.
  server_config_.SetMaxStreamsPerConnection(2, 2);
  // Client tries to negotiate for 10.
  client_config_.SetMaxStreamsPerConnection(10, 5);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  QuicConfig* client_negotiated_config = client_->client()->session()->config();
  EXPECT_EQ(2u, client_negotiated_config->MaxStreamsPerConnection());
}

TEST_P(EndToEndTest, ClientSuggestsRTT) {
  // Client suggests initial RTT, verify it is used.
  const uint32_t kInitialRTT = 20000;
  client_config_.SetInitialRoundTripTimeUsToSend(kInitialRTT);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->session()->connection()->sent_packet_manager();
  const QuicSentPacketManager& server_sent_packet_manager =
      *GetSentPacketManagerFromFirstServerSession();

  EXPECT_EQ(kInitialRTT,
            client_sent_packet_manager.GetRttStats()->initial_rtt_us());
  EXPECT_EQ(kInitialRTT,
            server_sent_packet_manager.GetRttStats()->initial_rtt_us());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, MaxInitialRTT) {
  // Client tries to suggest twice the server's max initial rtt and the server
  // uses the max.
  client_config_.SetInitialRoundTripTimeUsToSend(2 *
                                                 kMaxInitialRoundTripTimeUs);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  QuicSession* session = dispatcher->session_map().begin()->second;
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->session()->connection()->sent_packet_manager();

  // Now that acks have been exchanged, the RTT estimate has decreased on the
  // server and is not infinite on the client.
  EXPECT_FALSE(
      client_sent_packet_manager.GetRttStats()->smoothed_rtt().IsInfinite());
  const RttStats& server_rtt_stats =
      *session->connection()->sent_packet_manager().GetRttStats();
  EXPECT_EQ(static_cast<int64_t>(kMaxInitialRoundTripTimeUs),
            server_rtt_stats.initial_rtt_us());
  EXPECT_GE(static_cast<int64_t>(kMaxInitialRoundTripTimeUs),
            server_rtt_stats.smoothed_rtt().ToMicroseconds());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, MinInitialRTT) {
  // Client tries to suggest 0 and the server uses the default.
  client_config_.SetInitialRoundTripTimeUsToSend(0);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  QuicSession* session = dispatcher->session_map().begin()->second;
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->session()->connection()->sent_packet_manager();
  const QuicSentPacketManager& server_sent_packet_manager =
      session->connection()->sent_packet_manager();

  // Now that acks have been exchanged, the RTT estimate has decreased on the
  // server and is not infinite on the client.
  EXPECT_FALSE(
      client_sent_packet_manager.GetRttStats()->smoothed_rtt().IsInfinite());
  // Expect the default rtt of 100ms.
  EXPECT_EQ(static_cast<int64_t>(100 * kNumMicrosPerMilli),
            server_sent_packet_manager.GetRttStats()->initial_rtt_us());
  // Ensure the bandwidth is valid.
  client_sent_packet_manager.BandwidthEstimate();
  server_sent_packet_manager.BandwidthEstimate();
  server_thread_->Resume();
}

TEST_P(EndToEndTest, 0ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(0);
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  QuicPacketHeader* header = QuicConnectionPeer::GetLastHeader(
      client_->client()->session()->connection());
  EXPECT_EQ(PACKET_0BYTE_CONNECTION_ID,
            header->public_header.connection_id_length);
}

TEST_P(EndToEndTest, 1ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(1);
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  QuicPacketHeader* header = QuicConnectionPeer::GetLastHeader(
      client_->client()->session()->connection());
  EXPECT_EQ(PACKET_1BYTE_CONNECTION_ID,
            header->public_header.connection_id_length);
}

TEST_P(EndToEndTest, 4ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(4);
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  QuicPacketHeader* header = QuicConnectionPeer::GetLastHeader(
      client_->client()->session()->connection());
  EXPECT_EQ(PACKET_4BYTE_CONNECTION_ID,
            header->public_header.connection_id_length);
}

TEST_P(EndToEndTest, 8ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(8);
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  QuicPacketHeader* header = QuicConnectionPeer::GetLastHeader(
      client_->client()->session()->connection());
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID,
            header->public_header.connection_id_length);
}

TEST_P(EndToEndTest, 15ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(15);
  ASSERT_TRUE(Initialize());

  // Our server is permissive and allows for out of bounds values.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  QuicPacketHeader* header = QuicConnectionPeer::GetLastHeader(
      client_->client()->session()->connection());
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID,
            header->public_header.connection_id_length);
}

TEST_P(EndToEndTest, ResetConnection) {
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  client_->ResetConnection();
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, MaxStreamsUberTest) {
  if (!BothSidesSupportStatelessRejects()) {
    // Connect with lower fake packet loss than we'd like to test.  Until
    // b/10126687 is fixed, losing handshake packets is pretty brutal.
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(1);
  }
  ASSERT_TRUE(Initialize());
  string large_body;
  GenerateBody(&large_body, 10240);
  int max_streams = 100;

  AddToCache("/large_response", 200, large_body);

  client_->client()->WaitForCryptoHandshakeConfirmed();
  SetPacketLossPercentage(10);

  for (int i = 0; i < max_streams; ++i) {
    EXPECT_LT(0, client_->SendRequest("/large_response"));
  }

  // WaitForEvents waits 50ms and returns true if there are outstanding
  // requests.
  while (client_->client()->WaitForEvents() == true) {
  }
}

TEST_P(EndToEndTest, StreamCancelErrorTest) {
  ASSERT_TRUE(Initialize());
  string small_body;
  GenerateBody(&small_body, 256);

  AddToCache("/small_response", 200, small_body);

  client_->client()->WaitForCryptoHandshakeConfirmed();

  QuicSession* session = client_->client()->session();
  // Lose the request.
  SetPacketLossPercentage(100);
  EXPECT_LT(0, client_->SendRequest("/small_response"));
  client_->client()->WaitForEvents();
  // Transmit the cancel, and ensure the connection is torn down properly.
  SetPacketLossPercentage(0);
  QuicStreamId stream_id = kClientDataStreamId1;
  session->SendRstStream(stream_id, QUIC_STREAM_CANCELLED, 0);

  // WaitForEvents waits 50ms and returns true if there are outstanding
  // requests.
  while (client_->client()->WaitForEvents() == true) {
  }
  // It should be completely fine to RST a stream before any data has been
  // received for that stream.
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

class WrongAddressWriter : public QuicPacketWriterWrapper {
 public:
  WrongAddressWriter() {
    IPAddressNumber ip;
    CHECK(net::ParseIPLiteralToNumber("127.0.0.2", &ip));
    self_address_ = IPEndPoint(ip, 0);
  }

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const IPAddressNumber& /*real_self_address*/,
                          const IPEndPoint& peer_address,
                          PerPacketOptions* options) override {
    // Use wrong address!
    return QuicPacketWriterWrapper::WritePacket(buffer, buf_len,
                                                self_address_.address().bytes(),
                                                peer_address, options);
  }

  bool IsWriteBlockedDataBuffered() const override { return false; }

  IPEndPoint self_address_;
};

TEST_P(EndToEndTest, ConnectionMigrationClientIPChanged) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Store the client IP address which was used to send the first request.
  IPAddressNumber old_host =
      client_->client()->GetLatestClientAddress().address().bytes();

  // Migrate socket to the new IP address.
  IPAddressNumber new_host;
  CHECK(net::ParseIPLiteralToNumber("127.0.0.2", &new_host));
  EXPECT_NE(old_host, new_host);
  ASSERT_TRUE(client_->client()->MigrateSocket(new_host));

  // Send a request using the new socket.
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, ConnectionMigrationClientPortChanged) {
  // Tests that the client's port can change during an established QUIC
  // connection, and that doing so does not result in the connection being
  // closed by the server.
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Store the client address which was used to send the first request.
  IPEndPoint old_address = client_->client()->GetLatestClientAddress();

  // Stop listening and close the old FD.
  QuicClientPeer::CleanUpUDPSocket(client_->client(),
                                   client_->client()->GetLatestFD());

  // Create a new socket before closing the old one, which will result in a new
  // ephemeral port.
  QuicClientPeer::CreateUDPSocket(client_->client());

  // The packet writer needs to be updated to use the new FD.
  client_->client()->CreateQuicPacketWriter();

  // Change the internal state of the client and connection to use the new port,
  // this is done because in a real NAT rebinding the client wouldn't see any
  // port change, and so expects no change to incoming port.
  // This is kind of ugly, but needed as we are simply swapping out the client
  // FD rather than any more complex NAT rebinding simulation.
  int new_port = client_->client()->GetLatestClientAddress().port();
  QuicClientPeer::SetClientPort(client_->client(), new_port);
  QuicConnectionPeer::SetSelfAddress(
      client_->client()->session()->connection(),
      IPEndPoint(
          client_->client()->session()->connection()->self_address().address(),
          new_port));

  // Register the new FD for epoll events.
  int new_fd = client_->client()->GetLatestFD();
  EpollServer* eps = client_->epoll_server();
  eps->RegisterFD(new_fd, client_->client(), EPOLLIN | EPOLLOUT | EPOLLET);

  // Send a second request, using the new FD.
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Verify that the client's ephemeral port is different.
  IPEndPoint new_address = client_->client()->GetLatestClientAddress();
  EXPECT_EQ(old_address.address(), new_address.address());
  EXPECT_NE(old_address.port(), new_address.port());
}

TEST_P(EndToEndTest, DifferentFlowControlWindows) {
  // Client and server can set different initial flow control receive windows.
  // These are sent in CHLO/SHLO. Tests that these values are exchanged properly
  // in the crypto handshake.
  const uint32_t kClientStreamIFCW = 123456;
  const uint32_t kClientSessionIFCW = 234567;
  set_client_initial_stream_flow_control_receive_window(kClientStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kClientSessionIFCW);

  uint32_t kServerStreamIFCW =
      GetParam().auto_tune_flow_control_window ? 32 * 1024 : 654321;
  uint32_t kServerSessionIFCW =
      GetParam().auto_tune_flow_control_window ? 48 * 1024 : 765432;
  set_server_initial_stream_flow_control_receive_window(kServerStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kServerSessionIFCW);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Open a data stream to make sure the stream level flow control is updated.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  stream->SendBody("hello", false);

  // Client should have the right values for server's receive window.
  EXPECT_EQ(kServerStreamIFCW,
            client_->client()
                ->session()
                ->config()
                ->ReceivedInitialStreamFlowControlWindowBytes());
  EXPECT_EQ(kServerSessionIFCW,
            client_->client()
                ->session()
                ->config()
                ->ReceivedInitialSessionFlowControlWindowBytes());
  EXPECT_EQ(kServerStreamIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                   stream->flow_controller()));
  EXPECT_EQ(kServerSessionIFCW,
            QuicFlowControllerPeer::SendWindowOffset(
                client_->client()->session()->flow_controller()));

  // Server should have the right values for client's receive window.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicSession* session = dispatcher->session_map().begin()->second;
  EXPECT_EQ(kClientStreamIFCW,
            session->config()->ReceivedInitialStreamFlowControlWindowBytes());
  EXPECT_EQ(kClientSessionIFCW,
            session->config()->ReceivedInitialSessionFlowControlWindowBytes());
  EXPECT_EQ(kClientSessionIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                    session->flow_controller()));
  server_thread_->Resume();
}

TEST_P(EndToEndTest, HeadersAndCryptoStreamsNoConnectionFlowControl) {
  // The special headers and crypto streams should be subject to per-stream flow
  // control limits, but should not be subject to connection level flow control.
  const uint32_t kStreamIFCW =
      GetParam().auto_tune_flow_control_window ? 32 * 1024 : 123456;
  const uint32_t kSessionIFCW =
      GetParam().auto_tune_flow_control_window ? 48 * 1024 : 234567;
  set_client_initial_stream_flow_control_receive_window(kStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kSessionIFCW);
  set_server_initial_stream_flow_control_receive_window(kStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kSessionIFCW);

  ASSERT_TRUE(Initialize());

  // Wait for crypto handshake to finish. This should have contributed to the
  // crypto stream flow control window, but not affected the session flow
  // control window.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  QuicCryptoStream* crypto_stream =
      QuicSessionPeer::GetCryptoStream(client_->client()->session());
  EXPECT_LT(
      QuicFlowControllerPeer::SendWindowSize(crypto_stream->flow_controller()),
      kStreamIFCW);
  EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::SendWindowSize(
                              client_->client()->session()->flow_controller()));

  // Send a request with no body, and verify that the connection level window
  // has not been affected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(client_->client()->session());
  EXPECT_LT(
      QuicFlowControllerPeer::SendWindowSize(headers_stream->flow_controller()),
      kStreamIFCW);
  EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::SendWindowSize(
                              client_->client()->session()->flow_controller()));

  // Server should be in a similar state: connection flow control window should
  // not have any bytes marked as received.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicSession* session = dispatcher->session_map().begin()->second;
  QuicFlowController* server_connection_flow_controller =
      session->flow_controller();
  EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::ReceiveWindowSize(
                              server_connection_flow_controller));
  server_thread_->Resume();
}

TEST_P(EndToEndTest, FlowControlsSynced) {
  set_smaller_flow_control_receive_window();

  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  server_thread_->Pause();
  QuicSpdySession* const client_session = client_->client()->session();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicSpdySession* server_session = dispatcher->session_map().begin()->second;

  ExpectFlowControlsSynced(client_session->flow_controller(),
                           server_session->flow_controller());
  ExpectFlowControlsSynced(
      QuicSessionPeer::GetCryptoStream(client_session)->flow_controller(),
      QuicSessionPeer::GetCryptoStream(server_session)->flow_controller());
  ExpectFlowControlsSynced(
      QuicSpdySessionPeer::GetHeadersStream(client_session)->flow_controller(),
      QuicSpdySessionPeer::GetHeadersStream(server_session)->flow_controller());

  EXPECT_EQ(static_cast<float>(QuicFlowControllerPeer::ReceiveWindowSize(
                client_session->flow_controller())) /
                QuicFlowControllerPeer::ReceiveWindowSize(
                    QuicSpdySessionPeer::GetHeadersStream(client_session)
                        ->flow_controller()),
            kSessionToStreamRatio);

  server_thread_->Resume();
}

TEST_P(EndToEndTest, RequestWithNoBodyWillNeverSendStreamFrameWithFIN) {
  // A stream created on receipt of a simple request with no body will never get
  // a stream frame with a FIN. Verify that we don't keep track of the stream in
  // the locally closed streams map: it will never be removed if so.
  ASSERT_TRUE(Initialize());

  // Send a simple headers only request, and receive response.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Now verify that the server is not waiting for a final FIN or RST.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicSession* session = dispatcher->session_map().begin()->second;
  EXPECT_EQ(
      0u,
      QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(session).size());
  server_thread_->Resume();
}

// A TestAckListener verifies that its OnAckNotification method has been
// called exactly once on destruction.
class TestAckListener : public QuicAckListenerInterface {
 public:
  explicit TestAckListener(int num_packets) : num_notifications_(num_packets) {}

  void OnPacketAcked(int /*acked_bytes*/,
                     QuicTime::Delta /*delta_largest_observed*/) override {
    ASSERT_LT(0, num_notifications_);
    num_notifications_--;
  }

  void OnPacketRetransmitted(int /*retransmitted_bytes*/) override {}

  bool has_been_notified() const { return num_notifications_ == 0; }

 protected:
  // Object is ref counted.
  ~TestAckListener() override { EXPECT_EQ(0, num_notifications_); }

 private:
  int num_notifications_;
};

class TestResponseListener : public QuicClient::ResponseListener {
 public:
  void OnCompleteResponse(QuicStreamId id,
                          const BalsaHeaders& response_headers,
                          const string& response_body) override {
    std::string debug_string;
    response_headers.DumpHeadersToString(&debug_string);
    DVLOG(1) << "response for stream " << id << " " << debug_string << "\n"
             << response_body;
  }
};

TEST_P(EndToEndTest, AckNotifierWithPacketLossAndBlockedSocket) {
  // Verify that even in the presence of packet loss and occasionally blocked
  // socket,  an AckNotifierDelegate will get informed that the data it is
  // interested in has been ACKed. This tests end-to-end ACK notification, and
  // demonstrates that retransmissions do not break this functionality.
  if (!BothSidesSupportStatelessRejects()) {
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(5);
  }
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  SetPacketLossPercentage(30);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // Create a POST request and send the headers only.
  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/foo");
  request.set_has_complete_message(false);
  client_->SendMessage(request);

  // The TestAckListener will cause a failure if not notified.
  scoped_refptr<TestAckListener> delegate(new TestAckListener(2));

  // Test the AckNotifier's ability to track multiple packets by making the
  // request body exceed the size of a single packet.
  string request_string =
      "a request body bigger than one packet" + string(kMaxPacketSize, '.');

  // Send the request, and register the delegate for ACKs.
  client_->SendData(request_string, true, delegate.get());
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Send another request to flush out any pending ACKs on the server.
  client_->SendSynchronousRequest("/bar");

  // Pause the server to avoid races.
  server_thread_->Pause();
  // Make sure the delegate does get the notification it expects.
  while (!delegate->has_been_notified()) {
    // Waits for up to 50 ms.
    client_->client()->WaitForEvents();
  }
  server_thread_->Resume();
}

// Send a public reset from the server for a different connection ID.
// It should be ignored.
TEST_P(EndToEndTest, ServerSendPublicResetWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  // Send the public reset.
  QuicConnectionId incorrect_connection_id =
      client_->client()->session()->connection()->connection_id() + 1;
  QuicPublicResetPacket header;
  header.public_header.connection_id = incorrect_connection_id;
  header.public_header.reset_flag = true;
  header.public_header.version_flag = false;
  header.rejected_packet_number = 10101;
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_SERVER);
  scoped_ptr<QuicEncryptedPacket> packet(framer.BuildPublicResetPacket(header));
  testing::NiceMock<MockQuicConnectionDebugVisitor> visitor;
  client_->client()->session()->connection()->set_debug_visitor(&visitor);
  EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
      .Times(1);
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  server_writer_->WritePacket(
      packet->data(), packet->length(), server_address_.address().bytes(),
      client_->client()->GetLatestClientAddress(), nullptr);
  server_thread_->Resume();

  // The connection should be unaffected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  client_->client()->session()->connection()->set_debug_visitor(nullptr);
}

// Send a public reset from the client for a different connection ID.
// It should be ignored.
TEST_P(EndToEndTest, ClientSendPublicResetWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  // Send the public reset.
  QuicConnectionId incorrect_connection_id =
      client_->client()->session()->connection()->connection_id() + 1;
  QuicPublicResetPacket header;
  header.public_header.connection_id = incorrect_connection_id;
  header.public_header.reset_flag = true;
  header.public_header.version_flag = false;
  header.rejected_packet_number = 10101;
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_CLIENT);
  scoped_ptr<QuicEncryptedPacket> packet(framer.BuildPublicResetPacket(header));
  client_writer_->WritePacket(
      packet->data(), packet->length(),
      client_->client()->GetLatestClientAddress().address().bytes(),
      server_address_, nullptr);

  // The connection should be unaffected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

// Send a version negotiation packet from the server for a different
// connection ID.  It should be ignored.
TEST_P(EndToEndTest, ServerSendVersionNegotiationWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  // Send the version negotiation packet.
  QuicConnectionId incorrect_connection_id =
      client_->client()->session()->connection()->connection_id() + 1;
  scoped_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(incorrect_connection_id,
                                                server_supported_versions_));
  testing::NiceMock<MockQuicConnectionDebugVisitor> visitor;
  client_->client()->session()->connection()->set_debug_visitor(&visitor);
  EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
      .Times(1);
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  server_writer_->WritePacket(
      packet->data(), packet->length(), server_address_.address().bytes(),
      client_->client()->GetLatestClientAddress(), nullptr);
  server_thread_->Resume();

  // The connection should be unaffected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  client_->client()->session()->connection()->set_debug_visitor(nullptr);
}

// A bad header shouldn't tear down the connection, because the receiver can't
// tell the connection ID.
TEST_P(EndToEndTest, BadPacketHeaderTruncated) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Packet with invalid public flags.
  char packet[] = {// public flags (8 byte connection_id)
                   0x3C,
                   // truncated connection ID
                   0x11};
  client_writer_->WritePacket(
      &packet[0], sizeof(packet),
      client_->client()->GetLatestClientAddress().address().bytes(),
      server_address_, nullptr);
  // Give the server time to process the packet.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER,
            QuicDispatcherPeer::GetAndClearLastError(dispatcher));
  server_thread_->Resume();

  // The connection should not be terminated.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

// A bad header shouldn't tear down the connection, because the receiver can't
// tell the connection ID.
TEST_P(EndToEndTest, BadPacketHeaderFlags) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  // Packet with invalid public flags.
  char packet[] = {
      // invalid public flags
      0xFF,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags
      0x00,
  };
  client_writer_->WritePacket(
      &packet[0], sizeof(packet),
      client_->client()->GetLatestClientAddress().address().bytes(),
      server_address_, nullptr);
  // Give the server time to process the packet.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER,
            QuicDispatcherPeer::GetAndClearLastError(dispatcher));
  server_thread_->Resume();

  // The connection should not be terminated.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

// Send a packet from the client with bad encrypted data.  The server should not
// tear down the connection.
TEST_P(EndToEndTest, BadEncryptedData) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  scoped_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      client_->client()->session()->connection()->connection_id(), false, false,
      false, kDefaultPathId, 1, "At least 20 characters.",
      PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_PACKET_NUMBER));
  // Damage the encrypted data.
  string damaged_packet(packet->data(), packet->length());
  damaged_packet[30] ^= 0x01;
  DVLOG(1) << "Sending bad packet.";
  client_writer_->WritePacket(
      damaged_packet.data(), damaged_packet.length(),
      client_->client()->GetLatestClientAddress().address().bytes(),
      server_address_, nullptr);
  // Give the server time to process the packet.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  // This error is sent to the connection's OnError (which ignores it), so the
  // dispatcher doesn't see it.
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicDispatcherPeer::GetAndClearLastError(dispatcher));
  server_thread_->Resume();

  // The connection should not be terminated.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

// A test stream that gives |response_body_| as an error response body.
class ServerStreamWithErrorResponseBody : public QuicSimpleServerStream {
 public:
  ServerStreamWithErrorResponseBody(QuicStreamId id,
                                    QuicSpdySession* session,
                                    string response_body)
      : QuicSimpleServerStream(id, session), response_body_(response_body) {}

  ~ServerStreamWithErrorResponseBody() override {}

 protected:
  void SendErrorResponse() override {
    DVLOG(1) << "Sending error response for stream " << id();
    SpdyHeaderBlock headers;
    headers[":status"] = "500";
    headers["content-length"] = base::UintToString(response_body_.size());
    // This method must call CloseReadSide to cause the test case, StopReading
    // is not sufficient.
    ReliableQuicStreamPeer::CloseReadSide(this);
    SendHeadersAndBody(headers, response_body_);
  }

  string response_body_;
};

class StreamWithErrorFactory : public QuicTestServer::StreamFactory {
 public:
  explicit StreamWithErrorFactory(string response_body)
      : response_body_(response_body) {}

  ~StreamWithErrorFactory() override {}

  QuicSimpleServerStream* CreateStream(QuicStreamId id,
                                       QuicSpdySession* session) override {
    return new ServerStreamWithErrorResponseBody(id, session, response_body_);
  }

 private:
  string response_body_;
};

TEST_P(EndToEndTest, EarlyResponseFinRecording) {
  set_smaller_flow_control_receive_window();

  // Verify that an incoming FIN is recorded in a stream object even if the read
  // side has been closed.  This prevents an entry from being made in
  // locally_close_streams_highest_offset_ (which will never be deleted).
  // To set up the test condition, the server must do the following in order:
  // start sending the response and call CloseReadSide
  // receive the FIN of the request
  // send the FIN of the response

  string response_body;
  // The response body must be larger than the flow control window so the server
  // must receive a window update from the client before it can finish sending
  // it.
  uint32_t response_body_size =
      2 * client_config_.GetInitialStreamFlowControlWindowToSend();
  GenerateBody(&response_body, response_body_size);

  StreamWithErrorFactory stream_factory(response_body);
  SetSpdyStreamFactory(&stream_factory);

  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // A POST that gets an early error response, after the headers are received
  // and before the body is received, due to invalid content-length.
  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/garbage");
  // The body must be large enough that the FIN will be in a different packet
  // than the end of the headers, but short enough to not require a flow control
  // update.  This allows headers processing to trigger the error response
  // before the request FIN is processed but receive the request FIN before the
  // response is sent completely.
  const uint32_t kRequestBodySize = kMaxPacketSize + 10;
  string request_body;
  GenerateBody(&request_body, kRequestBodySize);
  request.AddBody(request_body, false);
  // Set an invalid content-length, so the request will receive an early 500
  // response.  Must be done after AddBody, which also sets content-length.
  request.AddHeader("content-length", "-1");
  request.set_skip_message_validation(true);

  // Send the request.
  client_->SendMessage(request);
  client_->WaitForResponse();
  EXPECT_EQ(500u, client_->response_headers()->parsed_response_code());

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();

  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicDispatcher::SessionMap const& map =
      QuicDispatcherPeer::session_map(dispatcher);
  QuicDispatcher::SessionMap::const_iterator it = map.begin();
  EXPECT_TRUE(it != map.end());
  QuicServerSessionBase* server_session = it->second;

  // The stream is not waiting for the arrival of the peer's final offset.
  EXPECT_EQ(
      0u, QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(server_session)
              .size());

  server_thread_->Resume();
}

TEST_P(EndToEndTest, LargePostEarlyResponse) {
  const uint32_t kWindowSize = 65536;
  set_client_initial_stream_flow_control_receive_window(kWindowSize);
  set_client_initial_session_flow_control_receive_window(kWindowSize);
  set_server_initial_stream_flow_control_receive_window(kWindowSize);
  set_server_initial_session_flow_control_receive_window(kWindowSize);

  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // POST to a URL that gets an early error response, after the headers are
  // received and before the body is received.
  HTTPMessage request(HttpConstants::HTTP_1_1, HttpConstants::POST, "/garbage");
  const uint32_t kBodySize = 2 * kWindowSize;
  // Invalid content-length so the request will receive an early 500 response.
  request.AddHeader("content-length", "-1");
  request.set_skip_message_validation(true);
  request.set_has_complete_message(false);

  // Tell the client to not close the stream if it receives an early response.
  client_->set_allow_bidirectional_data(true);
  // Send the headers.
  client_->SendMessage(request);
  // Receive the response and let the server close writing.
  client_->WaitForInitialResponse();
  EXPECT_EQ(500u, client_->response_headers()->parsed_response_code());

  if (negotiated_version_ > QUIC_VERSION_28) {
    // Receive the reset stream from server on early response.
    client_->WaitForResponseForMs(100);
    ReliableQuicStream* stream =
        client_->client()->session()->GetStream(kClientDataStreamId1);
    // The stream is reset by server's reset stream.
    EXPECT_EQ(stream, nullptr);
    return;
  }

  // Send a body larger than the stream flow control window.
  string body;
  GenerateBody(&body, kBodySize);
  client_->SendData(body, true);

  // Run the client to let any buffered data be sent.
  // (This is OK despite already waiting for a response.)
  client_->WaitForResponse();
  // There should be no buffered data to write in the client's stream.
  ReliableQuicStream* stream =
      client_->client()->session()->GetStream(kClientDataStreamId1);
  EXPECT_FALSE(stream != nullptr && stream->HasBufferedData());
}

TEST_P(EndToEndTest, Trailers) {
  // Test sending and receiving HTTP/2 Trailers (trailing HEADERS frames).
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_trailers, true);
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  // Set reordering to ensure that Trailers arriving before body is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and trailers.
  const string kBody = "body content";

  SpdyHeaderBlock headers;
  headers[":status"] = "200";
  headers[":version"] = "HTTP/1.1";
  headers["content-length"] = IntToString(kBody.size());

  SpdyHeaderBlock trailers;
  trailers["some-trailing-header"] = "trailing-header-value";

  QuicInMemoryCache::GetInstance()->AddResponse(
      "www.google.com", "/trailer_url", headers, kBody, trailers);

  EXPECT_EQ(kBody, client_->SendSynchronousRequest("/trailer_url"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
  EXPECT_EQ(trailers, client_->response_trailers());
}

TEST_P(EndToEndTest, ServerPush) {
  FLAGS_quic_supports_push_promise = true;
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  // Set reordering to ensure that body arriving before PUSH_PROMISE is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and trailers.
  const string kBody = "body content";

  list<QuicInMemoryCache::ServerPushInfo> push_resources;

  string push_urls[] = {
      "https://google.com/font.woff", "https://google.com/script.js",
      "https://fonts.google.com/font.woff", "https://google.com/logo-hires.jpg",
  };
  for (const string& url : push_urls) {
    GURL resource_url(url);
    string body = "This is server push response body for " + url;
    SpdyHeaderBlock response_headers;
    response_headers[":version"] = "HTTP/1.1";
    response_headers[":status"] = "200";
    response_headers["content-length"] = IntToString(body.size());
    push_resources.push_back(QuicInMemoryCache::ServerPushInfo(
        resource_url, response_headers, kV3LowestPriority, body));
  }

  QuicInMemoryCache::GetInstance()->AddSimpleResponseWithServerPushResources(
      "google.com", "/push_example", 200, kBody, push_resources);

  client_->client()->set_response_listener(new TestResponseListener);

  DVLOG(1) << "send request for /push_example";
  EXPECT_EQ(kBody,
            client_->SendSynchronousRequest("https://google.com/push_example"));
  for (const string& url : push_urls) {
    DVLOG(1) << "send request for pushed stream on url " << url;
    string expected_body = "This is server push response body for " + url;
    string response_body = client_->SendSynchronousRequest(url);
    DVLOG(1) << "response body " << response_body;
    EXPECT_EQ(expected_body, response_body);
  }
}

}  // namespace
}  // namespace test
}  // namespace net

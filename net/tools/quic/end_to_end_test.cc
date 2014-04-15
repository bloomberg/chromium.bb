// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>
#include <sys/epoll.h>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/congestion_control/tcp_cubic_sender.h"
#include "net/quic/crypto/aes_128_gcm_12_encrypter.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/reliable_quic_stream_peer.h"
#include "net/test/gtest_util.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"
#include "net/tools/quic/quic_server.h"
#include "net/tools/quic/quic_socket_utils.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "net/tools/quic/test_tools/http_message.h"
#include "net/tools/quic/test_tools/packet_dropping_test_writer.h"
#include "net/tools/quic/test_tools/quic_client_peer.h"
#include "net/tools/quic/test_tools/quic_dispatcher_peer.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "net/tools/quic/test_tools/quic_server_peer.h"
#include "net/tools/quic/test_tools/quic_test_client.h"
#include "net/tools/quic/test_tools/server_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using base::WaitableEvent;
using net::test::GenerateBody;
using net::test::QuicConnectionPeer;
using net::test::QuicSessionPeer;
using net::test::ReliableQuicStreamPeer;
using net::tools::test::PacketDroppingTestWriter;
using net::tools::test::QuicDispatcherPeer;
using net::tools::test::QuicServerPeer;
using std::ostream;
using std::string;
using std::vector;

namespace net {
namespace tools {
namespace test {
namespace {

const char* kFooResponseBody = "Artichoke hearts make me happy.";
const char* kBarResponseBody = "Palm hearts are pretty delicious, also.";

// Run all tests with the cross products of all versions.
struct TestParams {
  TestParams(const QuicVersionVector& client_supported_versions,
             const QuicVersionVector& server_supported_versions,
             QuicVersion negotiated_version,
             bool use_pacing)
      : client_supported_versions(client_supported_versions),
        server_supported_versions(server_supported_versions),
        negotiated_version(negotiated_version),
        use_pacing(use_pacing) {
  }

  friend ostream& operator<<(ostream& os, const TestParams& p) {
    os << "{ server_supported_versions: "
       << QuicVersionVectorToString(p.server_supported_versions);
    os << " client_supported_versions: "
       << QuicVersionVectorToString(p.client_supported_versions);
    os << " negotiated_version: " << QuicVersionToString(p.negotiated_version);
    os << " use_pacing: " << p.use_pacing << " }";
    return os;
  }

  QuicVersionVector client_supported_versions;
  QuicVersionVector server_supported_versions;
  QuicVersion negotiated_version;
  bool use_pacing;
};

// Constructs various test permutations.
vector<TestParams> GetTestParams() {
  vector<TestParams> params;
  QuicVersionVector all_supported_versions = QuicSupportedVersions();
  for (int use_pacing = 0; use_pacing < 2; ++use_pacing) {
    // Add an entry for server and client supporting all versions.
    params.push_back(TestParams(all_supported_versions,
                                all_supported_versions,
                                all_supported_versions[0],
                                use_pacing != 0));

    // Test client supporting all versions and server supporting 1 version.
    // Simulate an old server and exercise version downgrade in the client.
    // Protocol negotiation should occur. Skip the i = 0 case because it is
    // essentially the same as the default case.
    for (size_t i = 1; i < all_supported_versions.size(); ++i) {
      QuicVersionVector server_supported_versions;
      server_supported_versions.push_back(all_supported_versions[i]);
      params.push_back(TestParams(all_supported_versions,
                                  server_supported_versions,
                                  server_supported_versions[0],
                                  use_pacing != 0));
    }
  }
  return params;
}

class ServerDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ServerDelegate(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}
  virtual ~ServerDelegate() {}
  virtual void OnCanWrite() OVERRIDE { dispatcher_->OnCanWrite(); }
 private:
  QuicDispatcher* dispatcher_;
};

class ClientDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ClientDelegate(QuicClient* client) : client_(client) {}
  virtual ~ClientDelegate() {}
  virtual void OnCanWrite() OVERRIDE {
    EpollEvent event(EPOLLOUT, false);
    client_->OnEvent(client_->fd(), &event);
  }
 private:
  QuicClient* client_;
};

class EndToEndTest : public ::testing::TestWithParam<TestParams> {
 protected:
  EndToEndTest()
      : server_hostname_("example.com"),
        server_started_(false),
        strike_register_no_startup_period_(false) {
    net::IPAddressNumber ip;
    CHECK(net::ParseIPLiteralToNumber("127.0.0.1", &ip));
    server_address_ = IPEndPoint(ip, 0);

    client_supported_versions_ = GetParam().client_supported_versions;
    server_supported_versions_ = GetParam().server_supported_versions;
    negotiated_version_ = GetParam().negotiated_version;
    FLAGS_enable_quic_pacing = GetParam().use_pacing;

    if (negotiated_version_ >= QUIC_VERSION_17) {
      FLAGS_enable_quic_stream_flow_control = true;
    }
    VLOG(1) << "Using Configuration: " << GetParam();

    client_config_.SetDefaults();
    server_config_.SetDefaults();
    server_config_.set_initial_round_trip_time_us(kMaxInitialRoundTripTimeUs,
                                                  0);

    // Use different flow control windows for client/server.
    client_initial_flow_control_receive_window_ =
        2 * kInitialFlowControlWindowForTest;
    server_initial_flow_control_receive_window_ =
        3 * kInitialFlowControlWindowForTest;

    QuicInMemoryCachePeer::ResetForTests();
    AddToCache("GET", "https://www.google.com/foo",
               "HTTP/1.1", "200", "OK", kFooResponseBody);
    AddToCache("GET", "https://www.google.com/bar",
               "HTTP/1.1", "200", "OK", kBarResponseBody);
  }

  virtual ~EndToEndTest() {
    // TODO(rtenneti): port RecycleUnusedPort if needed.
    // RecycleUnusedPort(server_address_.port());
    QuicInMemoryCachePeer::ResetForTests();
  }

  QuicTestClient* CreateQuicClient(QuicPacketWriterWrapper* writer) {
    QuicTestClient* client = new QuicTestClient(
        server_address_,
        server_hostname_,
        false,  // not secure
        client_config_,
        client_supported_versions_,
        client_initial_flow_control_receive_window_);
    client->UseWriter(writer);
    client->Connect();
    return client;
  }

  void set_client_initial_flow_control_receive_window(uint32 window) {
    CHECK(client_.get() == NULL);
    DVLOG(1) << "Setting client initial flow control window: " << window;
    client_initial_flow_control_receive_window_ = window;
  }

  void set_server_initial_flow_control_receive_window(uint32 window) {
    CHECK(server_thread_.get() == NULL);
    DVLOG(1) << "Setting server initial flow control window: " << window;
    server_initial_flow_control_receive_window_ = window;
  }

  bool Initialize() {
    // Start the server first, because CreateQuicClient() attempts
    // to connect to the server.
    StartServer();
    client_.reset(CreateQuicClient(client_writer_));
    static EpollEvent event(EPOLLOUT, false);
    client_writer_->Initialize(
        reinterpret_cast<QuicEpollConnectionHelper*>(
            QuicConnectionPeer::GetHelper(
                client_->client()->session()->connection())),
        new ClientDelegate(client_->client()));
    return client_->client()->connected();
  }

  virtual void SetUp() OVERRIDE {
    // The ownership of these gets transferred to the QuicPacketWriterWrapper
    // and QuicDispatcher when Initialize() is executed.
    client_writer_ = new PacketDroppingTestWriter();
    server_writer_ = new PacketDroppingTestWriter();
  }

  virtual void TearDown() OVERRIDE {
    StopServer();
  }

  void StartServer() {
    server_thread_.reset(
        new ServerThread(server_address_,
                         server_config_,
                         server_supported_versions_,
                         strike_register_no_startup_period_,
                         server_initial_flow_control_receive_window_));
    server_thread_->Initialize();
    server_address_ = IPEndPoint(server_address_.address(),
                                 server_thread_->GetPort());
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    QuicDispatcherPeer::UseWriter(dispatcher, server_writer_);
    server_writer_->Initialize(
        QuicDispatcherPeer::GetHelper(dispatcher),
        new ServerDelegate(dispatcher));
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

  void AddToCache(StringPiece method,
                  StringPiece path,
                  StringPiece version,
                  StringPiece response_code,
                  StringPiece response_detail,
                  StringPiece body) {
    QuicInMemoryCache::GetInstance()->AddSimpleResponse(
        method, path, version, response_code, response_detail, body);
  }

  void SetPacketLossPercentage(int32 loss) {
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

  void SetReorderPercentage(int32 reorder) {
    // TODO(rtenneti): enable when we can do random packet reorder tests in
    // chrome's tree.
    // client_writer_->set_fake_reorder_percentage(reorder);
    // server_writer_->set_fake_reorder_percentage(reorder);
  }

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
  uint32 client_initial_flow_control_receive_window_;
  uint32 server_initial_flow_control_receive_window_;
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_CASE_P(EndToEndTests,
                        EndToEndTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(EndToEndTest, SimpleRequestResponse) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
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

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.set_has_complete_message(false);

  client_->SendMessage(request);

  client_->SendData(string(), true);

  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  request.AddBody("foo", true);

  client_->SendMessage(request);
  client_->SendData(string(), true);
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
  scoped_ptr<QuicTestClient> client2(CreateQuicClient(NULL));

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
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
  string huge_request =
      "https://www.google.com/some/path?query=" + string(kMaxPacketSize, '.');
  AddToCache("GET", huge_request, "HTTP/1.1", "200", "OK", kBarResponseBody);

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest(huge_request));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, MultiplePacketsRandomOrder) {
  // Send a large enough request to guarantee fragmentation.
  string huge_request =
      "https://www.google.com/some/path?query=" + string(kMaxPacketSize, '.');
  AddToCache("GET", huge_request, "HTTP/1.1", "200", "OK", kBarResponseBody);

  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(50);

  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest(huge_request));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, PostMissingBytes) {
  ASSERT_TRUE(Initialize());

  // Add a content length header with no body.
  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_skip_message_validation(true);

  // This should be detected as stream fin without complete request,
  // triggering an error response.
  client_->SendCustomSynchronousRequest(request);
  EXPECT_EQ("bad", client_->response_body());
  EXPECT_EQ(500u, client_->response_headers()->parsed_response_code());
}

TEST_P(EndToEndTest, LargePostNoPacketLoss) {
  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // 1 Mb body.
  string body;
  GenerateBody(&body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  QuicConnectionStats stats =
      client_->client()->session()->connection()->GetStats();
  // TODO(rtenneti): check for stats.packets_lost is flaky on valgrind.
  // EXPECT_EQ(0u, stats.packets_lost);
}

TEST_P(EndToEndTest, LargePostNoPacketLoss1sRTT) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(1000));

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // 1 Mb body.
  string body;
  GenerateBody(&body, 100 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  QuicConnectionStats stats =
      client_->client()->session()->connection()->GetStats();
  EXPECT_EQ(0u, stats.packets_lost);
}

TEST_P(EndToEndTest, LargePostWithPacketLoss) {
  // Connect with lower fake packet loss than we'd like to test.  Until
  // b/10126687 is fixed, losing handshake packets is pretty brutal.
  SetPacketLossPercentage(5);
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  SetPacketLossPercentage(30);

  // 10 Kb body.
  string body;
  GenerateBody(&body, 1024 * 10);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, LargePostNoPacketLossWithDelayAndReordering) {
  ASSERT_TRUE(Initialize());

  client_->client()->WaitForCryptoHandshakeConfirmed();
  // Both of these must be called when the writer is not actively used.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // 1 Mb body.
  string body;
  GenerateBody(&body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, LargePostWithPacketLossAndBlockedSocket) {
  // Connect with lower fake packet loss than we'd like to test.  Until
  // b/10126687 is fixed, losing handshake packets is pretty brutal.
  SetPacketLossPercentage(5);
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  SetPacketLossPercentage(10);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // 10 Kb body.
  string body;
  GenerateBody(&body, 1024 * 10);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

// TODO(rtenneti): rch is investigating the root cause. Will enable after we
// find the bug.
TEST_P(EndToEndTest, DISABLED_LargePostZeroRTTFailure) {
  // Have the server accept 0-RTT without waiting a startup period.
  strike_register_no_startup_period_ = true;

  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  string body;
  GenerateBody(&body, 20480);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  EXPECT_EQ(2, client_->client()->session()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  if (client_supported_versions_[0] >= QUIC_VERSION_17 &&
      negotiated_version_ < QUIC_VERSION_17) {
    // If the version negotiation has resulted in a downgrade, then the client
    // must wait for the handshake to complete before sending any data.
    // Otherwise it may have queued QUIC_VERSION_17 frames which will trigger a
    // DFATAL when they are serialized after the downgrade.
    client_->client()->WaitForCryptoHandshakeConfirmed();
  }
  client_->WaitForResponseForMs(-1);
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  EXPECT_EQ(1, client_->client()->session()->GetNumSentClientHellos());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  if (client_supported_versions_[0] >= QUIC_VERSION_17 &&
      negotiated_version_ < QUIC_VERSION_17) {
    // If the version negotiation has resulted in a downgrade, then the client
    // must wait for the handshake to complete before sending any data.
    // Otherwise it may have queued QUIC_VERSION_17 frames which will trigger a
    // DFATAL when they are serialized after the downgrade.
    client_->client()->WaitForCryptoHandshakeConfirmed();
  }
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
  EXPECT_EQ(2, client_->client()->session()->GetNumSentClientHellos());
}

// TODO(ianswett): Enable once b/9295090 is fixed.
TEST_P(EndToEndTest, DISABLED_LargePostFEC) {
  SetPacketLossPercentage(30);
  ASSERT_TRUE(Initialize());
  client_->options()->max_packets_per_fec_group = 6;

  string body;
  GenerateBody(&body, 10240);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, LargePostLargeBuffer) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMicroseconds(1));
  // 1Mbit per second with a 128k buffer from server to client.  Wireless
  // clients commonly have larger buffers, but our max CWND is 200.
  server_writer_->set_max_bandwidth_and_buffer_size(
      QuicBandwidth::FromBytesPerSecond(256 * 1024), 128 * 1024);

  client_->client()->WaitForCryptoHandshakeConfirmed();

  // 1 Mb body.
  string body;
  GenerateBody(&body, 1024 * 1024);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, InvalidStream) {
  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();

  string body;
  GenerateBody(&body, kMaxPacketSize);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);
  // Force the client to write with a stream ID belonging to a nonexistent
  // server-side stream.
  QuicSessionPeer::SetNextStreamId(client_->client()->session(), 2);

  client_->SendCustomSynchronousRequest(request);
  // EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_PACKET_FOR_NONEXISTENT_STREAM, client_->connection_error());
}

// TODO(rch): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, DISABLED_MultipleTermination) {
  ASSERT_TRUE(Initialize());

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_has_complete_message(false);

  // Set the offset so we won't frame.  Otherwise when we pick up termination
  // before HTTP framing is complete, we send an error and close the stream,
  // and the second write is picked up as writing on a closed stream.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream != NULL);
  ReliableQuicStreamPeer::SetStreamBytesWritten(3, stream);

  client_->SendData("bar", true);
  client_->WaitForWriteToFlush();

  // By default the stream protects itself from writes after terminte is set.
  // Override this to test the server handling buggy clients.
  ReliableQuicStreamPeer::SetWriteSideClosed(
      false, client_->GetOrCreateStream());

  EXPECT_DFATAL(client_->SendData("eep", true), "Fin already buffered");
}

TEST_P(EndToEndTest, Timeout) {
  client_config_.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromMicroseconds(500),
      QuicTime::Delta::FromMicroseconds(500));
  // Note: we do NOT ASSERT_TRUE: we may time out during initial handshake:
  // that's enough to validate timeout in this case.
  Initialize();
  while (client_->client()->connected()) {
    client_->client()->WaitForEvents();
  }
}

TEST_P(EndToEndTest, LimitMaxOpenStreams) {
  // Server limits the number of max streams to 2.
  server_config_.set_max_streams_per_connection(2, 2);
  // Client tries to negotiate for 10.
  client_config_.set_max_streams_per_connection(10, 5);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  QuicConfig* client_negotiated_config = client_->client()->session()->config();
  EXPECT_EQ(2u, client_negotiated_config->max_streams_per_connection());
}

// TODO(rtenneti): DISABLED_LimitCongestionWindowAndRTT seems to be flaky.
// http://crbug.com/321870.
TEST_P(EndToEndTest, DISABLED_LimitCongestionWindowAndRTT) {
  server_config_.set_server_initial_congestion_window(kMaxInitialWindow,
                                                      kDefaultInitialWindow);
  // Client tries to negotiate twice the server's max and negotiation settles
  // on the max.
  client_config_.set_server_initial_congestion_window(2 * kMaxInitialWindow,
                                                      kDefaultInitialWindow);
  client_config_.set_initial_round_trip_time_us(1, 1);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  QuicSession* session = dispatcher->session_map().begin()->second;
  QuicConfig* client_negotiated_config = client_->client()->session()->config();
  QuicConfig* server_negotiated_config = session->config();
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->session()->connection()->sent_packet_manager();
  const QuicSentPacketManager& server_sent_packet_manager =
      session->connection()->sent_packet_manager();

  EXPECT_EQ(kMaxInitialWindow,
            client_negotiated_config->server_initial_congestion_window());
  EXPECT_EQ(kMaxInitialWindow,
            server_negotiated_config->server_initial_congestion_window());
  // The client shouldn't set it's initial window based on the negotiated value.
  EXPECT_EQ(kDefaultInitialWindow * kDefaultTCPMSS,
            client_sent_packet_manager.GetCongestionWindow());
  EXPECT_EQ(kMaxInitialWindow * kDefaultTCPMSS,
            server_sent_packet_manager.GetCongestionWindow());

  EXPECT_EQ(FLAGS_enable_quic_pacing,
            server_sent_packet_manager.using_pacing());
  EXPECT_EQ(FLAGS_enable_quic_pacing,
            client_sent_packet_manager.using_pacing());

  EXPECT_EQ(1u, client_negotiated_config->initial_round_trip_time_us());
  EXPECT_EQ(1u, server_negotiated_config->initial_round_trip_time_us());

  // Now use the negotiated limits with packet loss.
  SetPacketLossPercentage(30);

  // 10 Kb body.
  string body;
  GenerateBody(&body, 1024 * 10);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  server_thread_->Resume();

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_P(EndToEndTest, InitialRTT) {
  // Client tries to negotiate twice the server's max and negotiation settles
  // on the max.
  client_config_.set_initial_round_trip_time_us(2 * kMaxInitialRoundTripTimeUs,
                                                0);

  ASSERT_TRUE(Initialize());
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  QuicSession* session = dispatcher->session_map().begin()->second;
  QuicConfig* client_negotiated_config = client_->client()->session()->config();
  QuicConfig* server_negotiated_config = session->config();
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->session()->connection()->sent_packet_manager();
  const QuicSentPacketManager& server_sent_packet_manager =
      session->connection()->sent_packet_manager();

  EXPECT_EQ(kMaxInitialRoundTripTimeUs,
            client_negotiated_config->initial_round_trip_time_us());
  EXPECT_EQ(kMaxInitialRoundTripTimeUs,
            server_negotiated_config->initial_round_trip_time_us());
  // Now that acks have been exchanged, the RTT estimate has decreased on the
  // server and is not infinite on the client.
  EXPECT_FALSE(
      client_sent_packet_manager.GetRttStats()->SmoothedRtt().IsInfinite());
  EXPECT_GE(
      static_cast<int64>(kMaxInitialRoundTripTimeUs),
      server_sent_packet_manager.GetRttStats()->SmoothedRtt().ToMicroseconds());
  server_thread_->Resume();
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
  SetPacketLossPercentage(1);
  ASSERT_TRUE(Initialize());
  string large_body;
  GenerateBody(&large_body, 10240);
  int max_streams = 100;

  AddToCache("GET", "/large_response", "HTTP/1.1", "200", "OK", large_body);;

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

  AddToCache("GET", "/small_response", "HTTP/1.1", "200", "OK", small_body);

  client_->client()->WaitForCryptoHandshakeConfirmed();

  QuicSession* session = client_->client()->session();
  // Lose the request.
  SetPacketLossPercentage(100);
  EXPECT_LT(0, client_->SendRequest("/small_response"));
  client_->client()->WaitForEvents();
  // Transmit the cancel, and ensure the connection is torn down properly.
  SetPacketLossPercentage(0);
  QuicStreamId stream_id = 5;
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

  virtual WriteResult WritePacket(
      const char* buffer,
      size_t buf_len,
      const IPAddressNumber& real_self_address,
      const IPEndPoint& peer_address) OVERRIDE {
    // Use wrong address!
    return QuicPacketWriterWrapper::WritePacket(
        buffer, buf_len, self_address_.address(), peer_address);
  }

  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE {
    return false;
  }

  IPEndPoint self_address_;
};

TEST_P(EndToEndTest, ConnectionMigration) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200u, client_->response_headers()->parsed_response_code());

  scoped_ptr<WrongAddressWriter> writer(new WrongAddressWriter());

  writer->set_writer(new QuicDefaultPacketWriter(
      QuicClientPeer::GetFd(client_->client())));
  QuicConnectionPeer::SetWriter(client_->client()->session()->connection(),
                                writer.get());

  client_->SendSynchronousRequest("/bar");

  EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_ERROR_MIGRATING_ADDRESS, client_->connection_error());
}

TEST_P(EndToEndTest, DifferentFlowControlWindows) {
  // Client and server can set different initial flow control receive windows.
  // These are sent in CHLO/SHLO. Tests that these values are exchanged properly
  // in the crypto handshake.

  const uint32 kClientIFCW = 123456;
  set_client_initial_flow_control_receive_window(kClientIFCW);

  const uint32 kServerIFCW = 654321;
  set_server_initial_flow_control_receive_window(kServerIFCW);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  client_->client()->WaitForCryptoHandshakeConfirmed();
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Client should have the right value for server's receive window.
  EXPECT_EQ(kServerIFCW, client_->client()
                             ->session()
                             ->config()
                             ->peer_initial_flow_control_window_bytes());

  // Server should have the right value for client's receive window.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicSession* session = dispatcher->session_map().begin()->second;
  EXPECT_EQ(kClientIFCW,
            session->config()->peer_initial_flow_control_window_bytes());
  server_thread_->Resume();
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net

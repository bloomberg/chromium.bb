// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "net/base/ip_endpoint.h"
// TODO(rtenneti): Delete this when NSS is supported.
#include "net/quic/crypto/aes_128_gcm_encrypter.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/reliable_quic_stream_peer.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_server.h"
#include "net/tools/quic/test_tools/http_message_test_utils.h"
#include "net/tools/quic/test_tools/quic_test_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using base::WaitableEvent;
using std::string;

namespace net {
namespace test {
namespace {

const char* kFooResponseBody = "Artichoke hearts make me happy.";
const char* kBarResponseBody = "Palm hearts are pretty delicious, also.";
const size_t kCongestionFeedbackFrameSize = 25;
// If kCongestionFeedbackFrameSize increase we need to expand this string
// accordingly.
const char* kLargeRequest =
    "https://www.google.com/foo/test/a/request/string/longer/than/25/bytes";

void GenerateBody(string* body, int length) {
  body->clear();
  body->reserve(length);
  for (int i = 0; i < length; ++i) {
    body->append(1, static_cast<char>(32 + i % (126 - 32)));
  }
}


// Simple wrapper class to run GFE in a thread.
class ServerThread : public base::SimpleThread {
 public:
  explicit ServerThread(IPEndPoint address)
      : SimpleThread("server_thread"),
        listening_(true, false),
        quit_(true, false),
        address_(address),
        port_(0) {
  }
  virtual ~ServerThread() {
  }

  virtual void Run() {
    server_.Listen(address_);

    port_lock_.Acquire();
    port_ = server_.port();
    port_lock_.Release();

    listening_.Signal();
    while (!quit_.IsSignaled()) {
      server_.WaitForEvents();
    }
    server_.Shutdown();
  }

  int GetPort() {
    port_lock_.Acquire();
    int rc = port_;
    port_lock_.Release();
    return rc;
  }

  WaitableEvent* listening() { return &listening_; }
  WaitableEvent* quit() { return &quit_; }

 private:
  WaitableEvent listening_;
  WaitableEvent quit_;
  base::Lock port_lock_;
  QuicServer server_;
  IPEndPoint address_;
  int port_;

  DISALLOW_COPY_AND_ASSIGN(ServerThread);
};

class EndToEndTest : public ::testing::Test {
 protected:
  EndToEndTest()
      : server_hostname_("localhost"),
        server_started_(false) {
    net::IPAddressNumber ip;
    CHECK(net::ParseIPLiteralToNumber("127.0.0.1", &ip));
    server_address_ = IPEndPoint(ip, 0);

    AddToCache("GET", kLargeRequest, "HTTP/1.1", "200", "OK", kFooResponseBody);
    AddToCache("GET", "https://www.google.com/foo",
               "HTTP/1.1", "200", "OK", kFooResponseBody);
    AddToCache("GET", "https://www.google.com/bar",
               "HTTP/1.1", "200", "OK", kBarResponseBody);
  }

  virtual QuicTestClient* CreateQuicClient() {
    QuicTestClient* client = new QuicTestClient(server_address_,
                                                server_hostname_);
    client->Connect();
    return client;
  }

  virtual bool Initialize() {
    // Start the server first, because CreateQuicClient() attempts
    // to connect to the server.
    StartServer();
    client_.reset(CreateQuicClient());
    return client_->client()->connected();
  }

  virtual void TearDown() {
    StopServer();
  }

  void StartServer() {
    server_thread_.reset(new ServerThread(server_address_));
    server_thread_->Start();
    server_thread_->listening()->Wait();
    server_address_ = IPEndPoint(server_address_.address(),
                                 server_thread_->GetPort());
    server_started_ = true;
  }

  void StopServer() {
    if (!server_started_)
      return;
    server_thread_->quit()->Signal();
    server_thread_->Join();
  }

  void AddToCache(const StringPiece& method,
                  const StringPiece& path,
                  const StringPiece& version,
                  const StringPiece& response_code,
                  const StringPiece& response_detail,
                  const StringPiece& body) {
    BalsaHeaders request_headers, response_headers;
    request_headers.SetRequestFirstlineFromStringPieces(method,
                                                        path,
                                                        version);
    response_headers.SetRequestFirstlineFromStringPieces(version,
                                                         response_code,
                                                         response_detail);
    response_headers.AppendHeader("content-length",
                                  base::IntToString(body.length()));

    // Check if response already exists and matches.
    QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();
    const QuicInMemoryCache::Response* cached_response =
        cache->GetResponse(request_headers);
    if (cached_response != NULL) {
      string cached_response_headers_str, response_headers_str;
      cached_response->headers().DumpToString(&cached_response_headers_str);
      response_headers.DumpToString(&response_headers_str);
      CHECK_EQ(cached_response_headers_str, response_headers_str);
      CHECK_EQ(cached_response->body(), body);
      return;
    }
    cache->AddResponse(request_headers, response_headers, body);
  }

  IPEndPoint server_address_;
  string server_hostname_;
  scoped_ptr<ServerThread> server_thread_;
  scoped_ptr<QuicTestClient> client_;
  bool server_started_;
};

TEST_F(EndToEndTest, SimpleRequestResponse) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, SimpleRequestResponsev6) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  IPAddressNumber ip;
  CHECK(net::ParseIPLiteralToNumber("::", &ip));
  server_address_ = IPEndPoint(ip, server_address_.port());
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, SeparateFinPacket) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.set_has_complete_message(false);

  client_->SendMessage(request);

  client_->SendData("", true);

  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());

  request.AddBody("foo", true);

  client_->SendMessage(request);
  client_->SendData("", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, MultipleRequestResponse) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, MultipleClients) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());
  scoped_ptr<QuicTestClient> client2(CreateQuicClient());

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_has_complete_message(false);

  client_->SendMessage(request);
  client2->SendMessage(request);

  client_->SendData("bar", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());

  client2->SendData("eep", true);
  client2->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client2->response_body());
  EXPECT_EQ(200ul, client2->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, RequestOverMultiplePackets) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());
  // Set things up so we have a small payload, to guarantee fragmentation.
  // A congestion feedback frame can't be split into multiple packets, make sure
  // that our packet have room for at least this amount after the normal headers
  // are added.

  // TODO(rch) handle this better when we have different encryption options.
  size_t stream_data = 3;
  size_t stream_payload_size = QuicFramer::GetMinStreamFrameSize() +
      stream_data;
  size_t min_payload_size =
      std::max(kCongestionFeedbackFrameSize, stream_payload_size);
  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(min_payload_size);
  // TODO(satyashekhar): Fix this when versioning is implemented.
  client_->options()->max_packet_length =
      GetPacketHeaderSize(!kIncludeVersion) + ciphertext_size;

  // Make sure our request is too large to fit in one packet.
  EXPECT_GT(strlen(kLargeRequest), min_payload_size);
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest(kLargeRequest));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, MultipleFramesRandomOrder) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());
  // Set things up so we have a small payload, to guarantee fragmentation.
  // A congestion feedback frame can't be split into multiple packets, make sure
  // that our packet have room for at least this amount after the normal headers
  // are added.

  // TODO(rch) handle this better when we have different encryption options.
  size_t stream_data = 3;
  size_t stream_payload_size = QuicFramer::GetMinStreamFrameSize() +
      stream_data;
  size_t min_payload_size =
      std::max(kCongestionFeedbackFrameSize, stream_payload_size);
  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(min_payload_size);
  // TODO(satyashekhar): Fix this when versioning is implemented.
  client_->options()->max_packet_length =
      GetPacketHeaderSize(!kIncludeVersion) + ciphertext_size;
  client_->options()->random_reorder = true;

  // Make sure our request is too large to fit in one packet.
  EXPECT_GT(strlen(kLargeRequest), min_payload_size);
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest(kLargeRequest));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, PostMissingBytes) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

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
  EXPECT_EQ(500ul, client_->response_headers()->parsed_response_code());
}

TEST_F(EndToEndTest, LargePost) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  // FLAGS_fake_packet_loss_percentage = 30;
  ASSERT_TRUE(Initialize());

  string body;
  GenerateBody(&body, 10240);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

TEST_F(EndToEndTest, LargePostFEC) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  // FLAGS_fake_packet_loss_percentage = 30;
  ASSERT_TRUE(Initialize());
  client_->options()->max_packets_per_fec_group = 6;

  string body;
  GenerateBody(&body, 10240);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);

  EXPECT_EQ(kFooResponseBody, client_->SendCustomSynchronousRequest(request));
}

/*TEST_F(EndToEndTest, PacketTooLarge) {
  FLAGS_quic_allow_oversized_packets_for_test = true;
  ASSERT_TRUE(Initialize());

  string body;
  GenerateBody(&body, kMaxPacketSize);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);
  client_->options()->max_packet_length = 20480;

  EXPECT_EQ("", client_->SendCustomSynchronousRequest(request));
  EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, client_->connection_error());
}*/

TEST_F(EndToEndTest, InvalidStream) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());

  string body;
  GenerateBody(&body, kMaxPacketSize);

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddBody(body, true);
  // Force the client to write with a stream ID belonging to a nonexistant
  // server-side stream.
  QuicSessionPeer::SetNextStreamId(2, client_->client()->session());

  client_->SendCustomSynchronousRequest(request);
//  EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_PACKET_FOR_NONEXISTENT_STREAM, client_->connection_error());
}

TEST_F(EndToEndTest, MultipleTermination) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());
  scoped_ptr<QuicTestClient> client2(CreateQuicClient());

  HTTPMessage request(HttpConstants::HTTP_1_1,
                      HttpConstants::POST, "/foo");
  request.AddHeader("content-length", "3");
  request.set_has_complete_message(false);

  // Set the offset so we won't frame.  Otherwise when we pick up termination
  // before HTTP framing is complete, we send an error and close the stream,
  // and the second write is picked up as writing on a closed stream.
  QuicReliableClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream != NULL);
  ReliableQuicStreamPeer::SetStreamBytesWritten(3, stream);

  client_->SendData("bar", true);

  // By default the stream protects itself from writes after terminte is set.
  // Override this to test the server handling buggy clients.
  ReliableQuicStreamPeer::SetWriteSideClosed(
      false, client_->GetOrCreateStream());
  EXPECT_DEBUG_DEATH({
    client_->SendData("eep", true);
    client_->WaitForResponse();
    EXPECT_EQ(QUIC_MULTIPLE_TERMINATION_OFFSETS, client_->stream_error());
  },
  "Check failed: !fin_buffered_");
}

/*TEST_F(EndToEndTest, Timeout) {
  FLAGS_negotiated_timeout_us = 500;
  // Note: we do NOT ASSERT_TRUE: we may time out during initial handshake:
  // that's enough to validate timeout in this case.
  Initialize();

  while (client_->client()->connected()) {
    client_->client()->WaitForEvents();
  }
}*/

TEST_F(EndToEndTest, ResetConnection) {
  // TODO(rtenneti): Delete this when NSS is supported.
  if (!Aes128GcmEncrypter::IsSupported()) {
    LOG(INFO) << "AES GCM not supported. Test skipped.";
    return;
  }

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
  client_->ResetConnection();
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ(200ul, client_->response_headers()->parsed_response_code());
}

}  // namespace
}  // namespace test
}  // namespace net

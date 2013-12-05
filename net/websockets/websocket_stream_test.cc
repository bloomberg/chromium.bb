// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

// A sub-class of WebSocketHandshakeStreamCreateHelper which always sets a
// deterministic key to use in the WebSocket handshake.
class DeterministicKeyWebSocketHandshakeStreamCreateHelper
    : public WebSocketHandshakeStreamCreateHelper {
 public:
  DeterministicKeyWebSocketHandshakeStreamCreateHelper(
      const std::vector<std::string>& requested_subprotocols)
      : WebSocketHandshakeStreamCreateHelper(requested_subprotocols) {}

  virtual WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<ClientSocketHandle> connection,
      bool using_proxy) OVERRIDE {
    WebSocketHandshakeStreamCreateHelper::CreateBasicStream(connection.Pass(),
                                                            using_proxy);
    // This will break in an obvious way if the type created by
    // CreateBasicStream() changes.
    static_cast<WebSocketBasicHandshakeStream*>(stream())
        ->SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");
    return stream();
  }
};

class WebSocketStreamCreateTest : public ::testing::Test {
 protected:
  WebSocketStreamCreateTest() : websocket_error_(0) {}

  void CreateAndConnectCustomResponse(
      const std::string& socket_url,
      const std::string& socket_path,
      const std::vector<std::string>& sub_protocols,
      const std::string& origin,
      const std::string& extra_request_headers,
      const std::string& response_body) {
    url_request_context_host_.SetExpectations(
        WebSocketStandardRequest(socket_path, origin, extra_request_headers),
        response_body);
    CreateAndConnectStream(socket_url, sub_protocols, origin);
  }

  // |extra_request_headers| and |extra_response_headers| must end in "\r\n" or
  // errors like "Unable to perform synchronous IO while stopped" will occur.
  void CreateAndConnectStandard(const std::string& socket_url,
                                const std::string& socket_path,
                                const std::vector<std::string>& sub_protocols,
                                const std::string& origin,
                                const std::string& extra_request_headers,
                                const std::string& extra_response_headers) {
    CreateAndConnectCustomResponse(
        socket_url,
        socket_path,
        sub_protocols,
        origin,
        extra_request_headers,
        WebSocketStandardResponse(extra_response_headers));
  }

  void CreateAndConnectRawExpectations(
      const std::string& socket_url,
      const std::vector<std::string>& sub_protocols,
      const std::string& origin,
      scoped_ptr<DeterministicSocketData> socket_data) {
    url_request_context_host_.SetRawExpectations(socket_data.Pass());
    CreateAndConnectStream(socket_url, sub_protocols, origin);
  }

  // A wrapper for CreateAndConnectStreamForTesting that knows about our default
  // parameters.
  void CreateAndConnectStream(const std::string& socket_url,
                              const std::vector<std::string>& sub_protocols,
                              const std::string& origin) {
    stream_request_ = ::net::CreateAndConnectStreamForTesting(
        GURL(socket_url),
        scoped_ptr<WebSocketHandshakeStreamCreateHelper>(
            new DeterministicKeyWebSocketHandshakeStreamCreateHelper(
                sub_protocols)),
        GURL(origin),
        url_request_context_host_.GetURLRequestContext(),
        BoundNetLog(),
        scoped_ptr<WebSocketStream::ConnectDelegate>(
            new TestConnectDelegate(this)));
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // A simple function to make the tests more readable. Creates an empty vector.
  static std::vector<std::string> NoSubProtocols() {
    return std::vector<std::string>();
  }

  uint16 error() const { return websocket_error_; }

  class TestConnectDelegate : public WebSocketStream::ConnectDelegate {
   public:
    TestConnectDelegate(WebSocketStreamCreateTest* owner) : owner_(owner) {}

    virtual void OnSuccess(scoped_ptr<WebSocketStream> stream) OVERRIDE {
      stream.swap(owner_->stream_);
    }

    virtual void OnFailure(uint16 websocket_error) OVERRIDE {
      owner_->websocket_error_ = websocket_error;
    }

   private:
    WebSocketStreamCreateTest* owner_;
  };

  WebSocketTestURLRequestContextHost url_request_context_host_;
  scoped_ptr<WebSocketStreamRequest> stream_request_;
  // Only set if the connection succeeded.
  scoped_ptr<WebSocketStream> stream_;
  // Only set if the connection failed. 0 otherwise.
  uint16 websocket_error_;
};

// Confirm that the basic case works as expected.
TEST_F(WebSocketStreamCreateTest, SimpleSuccess) {
  CreateAndConnectStandard(
      "ws://localhost/", "/", NoSubProtocols(), "http://localhost/", "", "");
  RunUntilIdle();
  EXPECT_TRUE(stream_);
}

// Confirm that the stream isn't established until the message loop runs.
TEST_F(WebSocketStreamCreateTest, NeedsToRunLoop) {
  CreateAndConnectStandard(
      "ws://localhost/", "/", NoSubProtocols(), "http://localhost/", "", "");
  EXPECT_FALSE(stream_);
}

// Check the path is used.
TEST_F(WebSocketStreamCreateTest, PathIsUsed) {
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           NoSubProtocols(),
                           "http://localhost/",
                           "",
                           "");
  RunUntilIdle();
  EXPECT_TRUE(stream_);
}

// Check that the origin is used.
TEST_F(WebSocketStreamCreateTest, OriginIsUsed) {
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           NoSubProtocols(),
                           "http://google.com/",
                           "",
                           "");
  RunUntilIdle();
  EXPECT_TRUE(stream_);
}

// Check that sub-protocols are sent and parsed.
TEST_F(WebSocketStreamCreateTest, SubProtocolIsUsed) {
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           sub_protocols,
                           "http://google.com/",
                           "Sec-WebSocket-Protocol: chatv11.chromium.org, "
                           "chatv20.chromium.org\r\n",
                           "Sec-WebSocket-Protocol: chatv20.chromium.org\r\n");
  RunUntilIdle();
  EXPECT_TRUE(stream_);
  EXPECT_EQ("chatv20.chromium.org", stream_->GetSubProtocol());
}

// Unsolicited sub-protocols are rejected.
TEST_F(WebSocketStreamCreateTest, UnsolicitedSubProtocol) {
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           NoSubProtocols(),
                           "http://google.com/",
                           "",
                           "Sec-WebSocket-Protocol: chatv20.chromium.org\r\n");
  RunUntilIdle();
  EXPECT_FALSE(stream_);
  EXPECT_EQ(1006, error());
}

// Missing sub-protocol response is rejected.
TEST_F(WebSocketStreamCreateTest, UnacceptedSubProtocol) {
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           std::vector<std::string>(1, "chat.example.com"),
                           "http://localhost/",
                           "Sec-WebSocket-Protocol: chat.example.com\r\n",
                           "");
  RunUntilIdle();
  EXPECT_FALSE(stream_);
  EXPECT_EQ(1006, error());
}

// Only one sub-protocol can be accepted.
TEST_F(WebSocketStreamCreateTest, MultipleSubProtocolsInResponse) {
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           sub_protocols,
                           "http://google.com/",
                           "Sec-WebSocket-Protocol: chatv11.chromium.org, "
                           "chatv20.chromium.org\r\n",
                           "Sec-WebSocket-Protocol: chatv11.chromium.org, "
                           "chatv20.chromium.org\r\n");
  RunUntilIdle();
  EXPECT_FALSE(stream_);
  EXPECT_EQ(1006, error());
}

// Unknown extension in the response is rejected
TEST_F(WebSocketStreamCreateTest, UnknownExtension) {
  CreateAndConnectStandard("ws://localhost/testing_path",
                           "/testing_path",
                           NoSubProtocols(),
                           "http://localhost/",
                           "",
                           "Sec-WebSocket-Extensions: x-unknown-extension\r\n");
  RunUntilIdle();
  EXPECT_FALSE(stream_);
  EXPECT_EQ(1006, error());
}

// Additional Sec-WebSocket-Accept headers should be rejected.
TEST_F(WebSocketStreamCreateTest, DoubleAccept) {
  CreateAndConnectStandard(
      "ws://localhost/",
      "/",
      NoSubProtocols(),
      "http://localhost/",
      "",
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n");
  RunUntilIdle();
  EXPECT_FALSE(stream_);
  EXPECT_EQ(1006, error());
}

// Response code 200 must be rejected.
TEST_F(WebSocketStreamCreateTest, InvalidStatusCode) {
  static const char kInvalidStatusCodeResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kInvalidStatusCodeResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Redirects are not followed (according to the WHATWG WebSocket API, which
// overrides RFC6455 for browser applications).
TEST_F(WebSocketStreamCreateTest, RedirectsRejected) {
  static const char kRedirectResponse[] =
      "HTTP/1.1 302 Moved Temporarily\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: 34\r\n"
      "Connection: keep-alive\r\n"
      "Location: ws://localhost/other\r\n"
      "\r\n"
      "<title>Moved</title><h1>Moved</h1>";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kRedirectResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Malformed responses should be rejected. HttpStreamParser will accept just
// about any garbage in the middle of the headers. To make it give up, the junk
// has to be at the start of the response. Even then, it just gets treated as an
// HTTP/0.9 response.
TEST_F(WebSocketStreamCreateTest, MalformedResponse) {
  static const char kMalformedResponse[] =
      "220 mx.google.com ESMTP\r\n"
      "HTTP/1.1 101 OK\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kMalformedResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Upgrade header must be present.
TEST_F(WebSocketStreamCreateTest, MissingUpgradeHeader) {
  static const char kMissingUpgradeResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kMissingUpgradeResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// There must only be one upgrade header.
TEST_F(WebSocketStreamCreateTest, DoubleUpgradeHeader) {
  CreateAndConnectStandard(
      "ws://localhost/",
      "/",
      NoSubProtocols(),
      "http://localhost/",
      "", "Upgrade: HTTP/2.0\r\n");
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Connection header must be present.
TEST_F(WebSocketStreamCreateTest, MissingConnectionHeader) {
  static const char kMissingConnectionResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kMissingConnectionResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Connection header is permitted to contain other tokens.
TEST_F(WebSocketStreamCreateTest, AdditionalTokenInConnectionHeader) {
  static const char kAdditionalConnectionTokenResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade, Keep-Alive\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kAdditionalConnectionTokenResponse);
  RunUntilIdle();
  EXPECT_TRUE(stream_);
}

// Sec-WebSocket-Accept header must be present.
TEST_F(WebSocketStreamCreateTest, MissingSecWebSocketAccept) {
  static const char kMissingAcceptResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kMissingAcceptResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Sec-WebSocket-Accept header must match the key that was sent.
TEST_F(WebSocketStreamCreateTest, WrongSecWebSocketAccept) {
  static const char kIncorrectAcceptResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: x/byyPZ2tOFvJCGkkugcKvqhhPk=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://localhost/",
                                 "/",
                                 NoSubProtocols(),
                                 "http://localhost/",
                                 "",
                                 kIncorrectAcceptResponse);
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Cancellation works.
TEST_F(WebSocketStreamCreateTest, Cancellation) {
  CreateAndConnectStandard(
      "ws://localhost/", "/", NoSubProtocols(), "http://localhost/", "", "");
  stream_request_.reset();
  RunUntilIdle();
  EXPECT_FALSE(stream_);
}

// Connect failure must look just like negotiation failure.
TEST_F(WebSocketStreamCreateTest, ConnectionFailure) {
  scoped_ptr<DeterministicSocketData> socket_data(
      new DeterministicSocketData(NULL, 0, NULL, 0));
  socket_data->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  CreateAndConnectRawExpectations("ws://localhost/", NoSubProtocols(),
                                  "http://localhost/", socket_data.Pass());
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Connect timeout must look just like any other failure.
TEST_F(WebSocketStreamCreateTest, ConnectionTimeout) {
  scoped_ptr<DeterministicSocketData> socket_data(
      new DeterministicSocketData(NULL, 0, NULL, 0));
  socket_data->set_connect_data(
      MockConnect(ASYNC, ERR_CONNECTION_TIMED_OUT));
  CreateAndConnectRawExpectations("ws://localhost/", NoSubProtocols(),
                                  "http://localhost/", socket_data.Pass());
  RunUntilIdle();
  EXPECT_EQ(1006, error());
}

// Cancellation during connect works.
TEST_F(WebSocketStreamCreateTest, CancellationDuringConnect) {
  scoped_ptr<DeterministicSocketData> socket_data(
      new DeterministicSocketData(NULL, 0, NULL, 0));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  CreateAndConnectRawExpectations("ws://localhost/",
                                  NoSubProtocols(),
                                  "http://localhost/",
                                  socket_data.Pass());
  stream_request_.reset();
  RunUntilIdle();
  EXPECT_FALSE(stream_);
}

// Cancellation during write of the request headers works.
TEST_F(WebSocketStreamCreateTest, CancellationDuringWrite) {
  // We seem to need at least two operations in order to use SetStop().
  MockWrite writes[] = {MockWrite(ASYNC, 0, "GET / HTTP/"),
                        MockWrite(ASYNC, 1, "1.1\r\n")};
  // We keep a copy of the pointer so that we can call RunFor() on it later.
  DeterministicSocketData* socket_data(
      new DeterministicSocketData(NULL, 0, writes, arraysize(writes)));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_data->SetStop(1);
  CreateAndConnectRawExpectations("ws://localhost/",
                                  NoSubProtocols(),
                                  "http://localhost/",
                                  make_scoped_ptr(socket_data));
  socket_data->Run();
  stream_request_.reset();
  RunUntilIdle();
  EXPECT_FALSE(stream_);
}

// Cancellation during read of the response headers works.
TEST_F(WebSocketStreamCreateTest, CancellationDuringRead) {
  std::string request = WebSocketStandardRequest("/", "http://localhost/", "");
  MockWrite writes[] = {MockWrite(ASYNC, 0, request.c_str())};
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 101 Switching Protocols\r\nUpgr"),
  };
  DeterministicSocketData* socket_data(new DeterministicSocketData(
      reads, arraysize(reads), writes, arraysize(writes)));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_data->SetStop(1);
  CreateAndConnectRawExpectations("ws://localhost/",
                                  NoSubProtocols(),
                                  "http://localhost/",
                                  make_scoped_ptr(socket_data));
  socket_data->Run();
  stream_request_.reset();
  RunUntilIdle();
  EXPECT_FALSE(stream_);
}

}  // namespace
}  // namespace net

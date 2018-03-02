// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_stream_create_test_base.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test::IsOk;

namespace net {
namespace {

// Simple builder for a SequencedSocketData object to save repetitive code.
// It always sets the connect data to MockConnect(SYNCHRONOUS, OK), so it cannot
// be used in tests where the connect fails. In practice, those tests never have
// any read/write data and so can't benefit from it anyway.  The arrays are not
// copied. It is up to the caller to ensure they stay in scope until the test
// ends.
template <size_t reads_count, size_t writes_count>
std::unique_ptr<SequencedSocketData> BuildSocketData(
    MockRead (&reads)[reads_count],
    MockWrite (&writes)[writes_count]) {
  std::unique_ptr<SequencedSocketData> socket_data(
      new SequencedSocketData(reads, reads_count, writes, writes_count));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  return socket_data;
}

// Builder for a SequencedSocketData that expects nothing. This does not
// set the connect data, so the calling code must do that explicitly.
std::unique_ptr<SequencedSocketData> BuildNullSocketData() {
  return std::make_unique<SequencedSocketData>(nullptr, 0, nullptr, 0);
}

class MockWeakTimer : public base::MockTimer,
                      public base::SupportsWeakPtr<MockWeakTimer> {
 public:
  MockWeakTimer(bool retain_user_task, bool is_repeating)
      : MockTimer(retain_user_task, is_repeating) {}
};

static url::Origin Origin() {
  return url::Origin::Create(GURL("http://www.example.org/"));
}

static GURL Url() {
  return GURL("http://www.example.org/foobar");
}

class WebSocketStreamCreateTest : public ::testing::Test,
                                  public WebSocketStreamCreateTestBase {
 protected:
  ~WebSocketStreamCreateTest() override {
    // Permit any endpoint locks to be released.
    stream_request_.reset();
    stream_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void AddSSLData() {
    auto ssl_data = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    ssl_data->ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
    ASSERT_TRUE(ssl_data->ssl_info.cert.get());
    url_request_context_host_.AddSSLSocketDataProvider(std::move(ssl_data));
  }

  // Set up mock data and start websockets request, either for WebSocket
  // upgraded from an HTTP/1 connection, or for a WebSocket request over HTTP/2.
  void CreateAndConnectStandard(
      const std::string& socket_url,
      const std::string& socket_host,
      const std::string& socket_path,
      const std::vector<std::string>& sub_protocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const WebSocketExtraHeaders& send_additional_request_headers,
      const WebSocketExtraHeaders& extra_request_headers,
      const WebSocketExtraHeaders& extra_response_headers,
      std::unique_ptr<base::Timer> timer = {},
      const std::string& additional_data = {}) {
    url_request_context_host_.SetExpectations(
        WebSocketStandardRequest(
            socket_path, socket_host, origin,
            WebSocketExtraHeadersToString(send_additional_request_headers),
            WebSocketExtraHeadersToString(extra_request_headers)),
        WebSocketStandardResponse(
            WebSocketExtraHeadersToString(extra_response_headers)) +
            additional_data);
    CreateAndConnectStream(
        GURL(socket_url), sub_protocols, origin, site_for_cookies,
        WebSocketExtraHeadersToString(send_additional_request_headers),
        std::move(timer));
  }

  // Like CreateAndConnectStandard(), but allow for arbitrary response body.
  // Only for HTTP/1-based WebSockets.
  void CreateAndConnectCustomResponse(
      const std::string& socket_url,
      const std::string& socket_host,
      const std::string& socket_path,
      const std::vector<std::string>& sub_protocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const WebSocketExtraHeaders& send_additional_request_headers,
      const WebSocketExtraHeaders& extra_request_headers,
      const std::string& response_body) {
    url_request_context_host_.SetExpectations(
        WebSocketStandardRequest(
            socket_path, socket_host, origin,
            WebSocketExtraHeadersToString(send_additional_request_headers),
            WebSocketExtraHeadersToString(extra_request_headers)),
        response_body);
    CreateAndConnectStream(
        GURL(socket_url), sub_protocols, origin, site_for_cookies,
        WebSocketExtraHeadersToString(send_additional_request_headers),
        nullptr);
  }

  // Like CreateAndConnectStandard(), but take extra response headers as a
  // string.  This can save space in case of a very large response.
  // Only for HTTP/1-based WebSockets.
  void CreateAndConnectStringResponse(
      const std::string& socket_url,
      const std::string& socket_host,
      const std::string& socket_path,
      const std::vector<std::string>& sub_protocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const std::string& extra_response_headers) {
    url_request_context_host_.SetExpectations(
        WebSocketStandardRequest(socket_path, socket_host, origin, "", ""),
        WebSocketStandardResponse(extra_response_headers));
    CreateAndConnectStream(GURL(socket_url), sub_protocols, origin,
                           site_for_cookies, "", nullptr);
  }

  // Like CreateAndConnectStandard(), but take raw mock data.
  void CreateAndConnectRawExpectations(
      const std::string& socket_url,
      const std::vector<std::string>& sub_protocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const std::string& send_additional_request_headers,
      std::unique_ptr<SequencedSocketData> socket_data,
      std::unique_ptr<base::Timer> timer = std::unique_ptr<base::Timer>()) {
    url_request_context_host_.AddRawExpectations(std::move(socket_data));
    CreateAndConnectStream(GURL(socket_url), sub_protocols, origin,
                           site_for_cookies, send_additional_request_headers,
                           std::move(timer));
  }
};

// There are enough tests of the Sec-WebSocket-Extensions header that they
// deserve their own test fixture.
class WebSocketStreamCreateExtensionTest : public WebSocketStreamCreateTest {
 protected:
  // Performs a standard connect, with the value of the Sec-WebSocket-Extensions
  // header in the response set to |extensions_header_value|. Runs the event
  // loop to allow the connect to complete.
  void CreateAndConnectWithExtensions(
      const std::string& extensions_header_value) {
    AddSSLData();
    CreateAndConnectStandard(
        "wss://www.example.org/testing_path", "www.example.org",
        "/testing_path", NoSubProtocols(), Origin(), Url(), {}, {},
        {{"Sec-WebSocket-Extensions", extensions_header_value}});
    WaitUntilConnectDone();
  }
};

// Common code to construct expectations for authentication tests that receive
// the auth challenge on one connection and then create a second connection to
// send the authenticated request on.
class CommonAuthTestHelper {
 public:
  CommonAuthTestHelper() : reads1_(), writes1_(), reads2_(), writes2_() {}

  std::unique_ptr<SequencedSocketData> BuildSocketData1(
      const std::string& response) {
    request1_ =
        WebSocketStandardRequest("/", "www.example.org", Origin(), "", "");
    writes1_[0] = MockWrite(SYNCHRONOUS, 0, request1_.c_str());
    response1_ = response;
    reads1_[0] = MockRead(SYNCHRONOUS, 1, response1_.c_str());
    reads1_[1] = MockRead(SYNCHRONOUS, OK, 2);  // Close connection

    return BuildSocketData(reads1_, writes1_);
  }

  std::unique_ptr<SequencedSocketData> BuildSocketData2(
      const std::string& request,
      const std::string& response) {
    request2_ = request;
    response2_ = response;
    writes2_[0] = MockWrite(SYNCHRONOUS, 0, request2_.c_str());
    reads2_[0] = MockRead(SYNCHRONOUS, 1, response2_.c_str());
    return BuildSocketData(reads2_, writes2_);
  }

 private:
  // These need to be object-scoped since they have to remain valid until all
  // socket operations in the test are complete.
  std::string request1_;
  std::string request2_;
  std::string response1_;
  std::string response2_;
  MockRead reads1_[2];
  MockWrite writes1_[1];
  MockRead reads2_[1];
  MockWrite writes2_[1];

  DISALLOW_COPY_AND_ASSIGN(CommonAuthTestHelper);
};

// Data and methods for BasicAuth tests.
class WebSocketStreamCreateBasicAuthTest : public WebSocketStreamCreateTest {
 protected:
  void CreateAndConnectAuthHandshake(const std::string& url,
                                     const std::string& base64_user_pass,
                                     const std::string& response2) {
    url_request_context_host_.AddRawExpectations(
        helper_.BuildSocketData1(kUnauthorizedResponse));

    static const char request2format[] =
        "GET / HTTP/1.1\r\n"
        "Host: www.example.org\r\n"
        "Connection: Upgrade\r\n"
        "Pragma: no-cache\r\n"
        "Cache-Control: no-cache\r\n"
        "Authorization: Basic %s\r\n"
        "Upgrade: websocket\r\n"
        "Origin: http://www.example.org\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "User-Agent:\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Accept-Language: en-us,fr\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Extensions: permessage-deflate; "
        "client_max_window_bits\r\n"
        "\r\n";
    const std::string request =
        base::StringPrintf(request2format, base64_user_pass.c_str());
    CreateAndConnectRawExpectations(
        url, NoSubProtocols(), Origin(), Url(), "",
        helper_.BuildSocketData2(request, response2));
  }

  static const char kUnauthorizedResponse[];

  CommonAuthTestHelper helper_;
};

class WebSocketStreamCreateDigestAuthTest : public WebSocketStreamCreateTest {
 protected:
  static const char kUnauthorizedResponse[];
  static const char kAuthorizedRequest[];

  CommonAuthTestHelper helper_;
};

const char WebSocketStreamCreateBasicAuthTest::kUnauthorizedResponse[] =
    "HTTP/1.1 401 Unauthorized\r\n"
    "Content-Length: 0\r\n"
    "WWW-Authenticate: Basic realm=\"camelot\"\r\n"
    "\r\n";

// These negotiation values are borrowed from
// http_auth_handler_digest_unittest.cc. Feel free to come up with new ones if
// you are bored. Only the weakest (no qop) variants of Digest authentication
// can be tested by this method, because the others involve random input.
const char WebSocketStreamCreateDigestAuthTest::kUnauthorizedResponse[] =
    "HTTP/1.1 401 Unauthorized\r\n"
    "Content-Length: 0\r\n"
    "WWW-Authenticate: Digest realm=\"Oblivion\", nonce=\"nonce-value\"\r\n"
    "\r\n";

const char WebSocketStreamCreateDigestAuthTest::kAuthorizedRequest[] =
    "GET / HTTP/1.1\r\n"
    "Host: www.example.org\r\n"
    "Connection: Upgrade\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Authorization: Digest username=\"FooBar\", realm=\"Oblivion\", "
    "nonce=\"nonce-value\", uri=\"/\", "
    "response=\"f72ff54ebde2f928860f806ec04acd1b\"\r\n"
    "Upgrade: websocket\r\n"
    "Origin: http://www.example.org\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "User-Agent:\r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Extensions: permessage-deflate; "
    "client_max_window_bits\r\n"
    "\r\n";

class WebSocketStreamCreateUMATest : public WebSocketStreamCreateTest {
 protected:
  // This enum should match with the enum in Delegate in websocket_stream.cc.
  enum HandshakeResult {
    INCOMPLETE,
    CONNECTED,
    FAILED,
    NUM_HANDSHAKE_RESULT_TYPES,
  };
};

// Confirm that the basic case works as expected.
TEST_F(WebSocketStreamCreateTest, SimpleSuccess) {
  AddSSLData();
  EXPECT_FALSE(url_request_);
  CreateAndConnectStandard("wss://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {});
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  EXPECT_TRUE(url_request_);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_TRUE(response_info_);
  EXPECT_EQ(ERR_WS_UPGRADE,
            url_request_context_host_.network_delegate().last_error());
}

TEST_F(WebSocketStreamCreateTest, HandshakeInfo) {
  static const char kResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "foo: bar, baz\r\n"
      "hoge: fuga\r\n"
      "hoge: piyo\r\n"
      "\r\n";

  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kResponse);
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  WaitUntilConnectDone();
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(request_info_);
  ASSERT_TRUE(response_info_);
  std::vector<HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(request_info_->headers);
  // We examine the contents of request_info_ and response_info_
  // mainly only in this test case.
  EXPECT_EQ(GURL("ws://www.example.org/"), request_info_->url);
  EXPECT_EQ(GURL("ws://www.example.org/"), response_info_->url);
  EXPECT_EQ(101, response_info_->status_code);
  EXPECT_EQ("Switching Protocols", response_info_->status_text);
  ASSERT_EQ(12u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "www.example.org"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://www.example.org"),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", ""), request_headers[7]);
  EXPECT_EQ(HeaderKeyValuePair("Accept-Encoding", "gzip, deflate"),
            request_headers[8]);
  EXPECT_EQ(HeaderKeyValuePair("Accept-Language", "en-us,fr"),
            request_headers[9]);
  EXPECT_EQ("Sec-WebSocket-Key",  request_headers[10].first);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions",
                               "permessage-deflate; client_max_window_bits"),
            request_headers[11]);

  std::vector<HeaderKeyValuePair> response_headers =
      ResponseHeadersToVector(*response_info_->headers.get());
  ASSERT_EQ(6u, response_headers.size());
  // Sort the headers for ease of verification.
  std::sort(response_headers.begin(), response_headers.end());

  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), response_headers[0]);
  EXPECT_EQ("Sec-WebSocket-Accept", response_headers[1].first);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), response_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("foo", "bar, baz"), response_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("hoge", "fuga"), response_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("hoge", "piyo"), response_headers[5]);
}

// Confirms that request headers are overriden/added after handshake
TEST_F(WebSocketStreamCreateTest, HandshakeOverrideHeaders) {
  WebSocketExtraHeaders additional_headers(
      {{"User-Agent", "OveRrIde"}, {"rAnDomHeader", "foobar"}});
  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(),
                           additional_headers, additional_headers, {});
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_TRUE(response_info_);

  std::vector<HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(request_info_->headers);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", "OveRrIde"), request_headers[7]);
  EXPECT_EQ(HeaderKeyValuePair("rAnDomHeader", "foobar"), request_headers[8]);
}

// Confirm that the stream isn't established until the message loop runs.
TEST_F(WebSocketStreamCreateTest, NeedsToRunLoop) {
  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {});
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
}

// Check the path is used.
TEST_F(WebSocketStreamCreateTest, PathIsUsed) {
  AddSSLData();
  CreateAndConnectStandard("wss://www.example.org/testing_path",
                           "www.example.org", "/testing_path", NoSubProtocols(),
                           Origin(), Url(), {}, {}, {});
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
}

// Check that sub-protocols are sent and parsed.
TEST_F(WebSocketStreamCreateTest, SubProtocolIsUsed) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", "www.example.org", "/testing_path",
      sub_protocols, Origin(), Url(), {},
      {{"Sec-WebSocket-Protocol",
        "chatv11.chromium.org, chatv20.chromium.org"}},
      {{"Sec-WebSocket-Protocol", "chatv20.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
  EXPECT_EQ("chatv20.chromium.org", stream_->GetSubProtocol());
}

// Unsolicited sub-protocols are rejected.
TEST_F(WebSocketStreamCreateTest, UnsolicitedSubProtocol) {
  AddSSLData();
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", "www.example.org", "/testing_path",
      NoSubProtocols(), Origin(), Url(), {}, {},
      {{"Sec-WebSocket-Protocol", "chatv20.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Response must not include 'Sec-WebSocket-Protocol' header "
            "if not present in request: chatv20.chromium.org",
            failure_message());
  EXPECT_EQ(ERR_INVALID_RESPONSE,
            url_request_context_host_.network_delegate().last_error());
}

// Missing sub-protocol response is rejected.
TEST_F(WebSocketStreamCreateTest, UnacceptedSubProtocol) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chat.example.com");
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", "www.example.org", "/testing_path",
      sub_protocols, Origin(), Url(), {},
      {{"Sec-WebSocket-Protocol", "chat.example.com"}}, {});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Sent non-empty 'Sec-WebSocket-Protocol' header "
            "but no response was received",
            failure_message());
}

// Only one sub-protocol can be accepted.
TEST_F(WebSocketStreamCreateTest, MultipleSubProtocolsInResponse) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard("wss://www.example.org/testing_path",
                           "www.example.org", "/testing_path", sub_protocols,
                           Origin(), Url(), {},
                           {{"Sec-WebSocket-Protocol",
                             "chatv11.chromium.org, chatv20.chromium.org"}},
                           {{"Sec-WebSocket-Protocol",
                             "chatv11.chromium.org, chatv20.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: "
      "'Sec-WebSocket-Protocol' header must not appear "
      "more than once in a response",
      failure_message());
}

// Unmatched sub-protocol should be rejected.
TEST_F(WebSocketStreamCreateTest, UnmatchedSubProtocolInResponse) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", "www.example.org", "/testing_path",
      sub_protocols, Origin(), Url(), {},
      {{"Sec-WebSocket-Protocol",
        "chatv11.chromium.org, chatv20.chromium.org"}},
      {{"Sec-WebSocket-Protocol", "chatv21.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Sec-WebSocket-Protocol' header value 'chatv21.chromium.org' "
            "in response does not match any of sent values",
            failure_message());
}

// permessage-deflate extension basic success case.
TEST_F(WebSocketStreamCreateExtensionTest, PerMessageDeflateSuccess) {
  CreateAndConnectWithExtensions("permessage-deflate");
  EXPECT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
}

// permessage-deflate extensions success with all parameters.
TEST_F(WebSocketStreamCreateExtensionTest, PerMessageDeflateParamsSuccess) {
  CreateAndConnectWithExtensions(
      "permessage-deflate; client_no_context_takeover; "
      "server_max_window_bits=11; client_max_window_bits=13; "
      "server_no_context_takeover");
  EXPECT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
}

// Verify that incoming messages are actually decompressed with
// permessage-deflate enabled.
TEST_F(WebSocketStreamCreateExtensionTest, PerMessageDeflateInflates) {
  AddSSLData();
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", "www.example.org", "/testing_path",
      NoSubProtocols(), Origin(), Url(), {}, {},
      {{"Sec-WebSocket-Extensions", "permessage-deflate"}},
      std::unique_ptr<base::Timer>(),
      std::string(
          "\xc1\x07"  // WebSocket header (FIN + RSV1, Text payload 7 bytes)
          "\xf2\x48\xcd\xc9\xc9\x07\x00",  // "Hello" DEFLATE compressed
          9));
  WaitUntilConnectDone();

  ASSERT_TRUE(stream_);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  TestCompletionCallback callback;
  int rv = stream_->ReadFrames(&frames, callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_THAT(rv, IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_EQ(5U, frames[0]->header.payload_length);
  EXPECT_EQ("Hello", std::string(frames[0]->data->data(), 5));
}

// Unknown extension in the response is rejected
TEST_F(WebSocketStreamCreateExtensionTest, UnknownExtension) {
  CreateAndConnectWithExtensions("x-unknown-extension");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Found an unsupported extension 'x-unknown-extension' "
            "in 'Sec-WebSocket-Extensions' header",
            failure_message());
}

// Malformed extensions are rejected (this file does not cover all possible
// parse failures, as the parser is covered thoroughly by its own unit tests).
TEST_F(WebSocketStreamCreateExtensionTest, MalformedExtension) {
  CreateAndConnectWithExtensions(";");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: 'Sec-WebSocket-Extensions' header "
      "value is rejected by the parser: ;",
      failure_message());
}

// The permessage-deflate extension may only be specified once.
TEST_F(WebSocketStreamCreateExtensionTest, OnlyOnePerMessageDeflateAllowed) {
  CreateAndConnectWithExtensions(
      "permessage-deflate, permessage-deflate; client_max_window_bits=10");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: "
      "Received duplicate permessage-deflate response",
      failure_message());
}

// client_max_window_bits must have an argument
TEST_F(WebSocketStreamCreateExtensionTest, NoMaxWindowBitsArgument) {
  CreateAndConnectWithExtensions("permessage-deflate; client_max_window_bits");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: Error in permessage-deflate: "
      "client_max_window_bits must have value",
      failure_message());
}

// Other cases for permessage-deflate parameters are tested in
// websocket_deflate_parameters_test.cc.

// TODO(ricea): Check that WebSocketDeflateStream is initialised with the
// arguments from the server. This is difficult because the data written to the
// socket is randomly masked.

// Additional Sec-WebSocket-Accept headers should be rejected.
TEST_F(WebSocketStreamCreateTest, DoubleAccept) {
  CreateAndConnectStandard(
      "ws://www.example.org/", "www.example.org", "/", NoSubProtocols(),
      Origin(), Url(), {}, {},
      {{"Sec-WebSocket-Accept", "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Sec-WebSocket-Accept' header must not appear "
            "more than once in a response",
            failure_message());
}

// Response code 200 must be rejected.
TEST_F(WebSocketStreamCreateTest, InvalidStatusCode) {
  static const char kInvalidStatusCodeResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kInvalidStatusCodeResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: Unexpected response code: 200",
            failure_message());
}

// Redirects are not followed (according to the WHATWG WebSocket API, which
// overrides RFC6455 for browser applications).
TEST_F(WebSocketStreamCreateTest, RedirectsRejected) {
  static const char kRedirectResponse[] =
      "HTTP/1.1 302 Moved Temporarily\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: 34\r\n"
      "Connection: keep-alive\r\n"
      "Location: wss://www.example.org/other\r\n"
      "\r\n"
      "<title>Moved</title><h1>Moved</h1>";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kRedirectResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: Unexpected response code: 302",
            failure_message());
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
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kMalformedResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: Invalid status line",
            failure_message());
}

// Upgrade header must be present.
TEST_F(WebSocketStreamCreateTest, MissingUpgradeHeader) {
  static const char kMissingUpgradeResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kMissingUpgradeResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: 'Upgrade' header is missing",
            failure_message());
}

// There must only be one upgrade header.
TEST_F(WebSocketStreamCreateTest, DoubleUpgradeHeader) {
  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {},
                           {{"Upgrade", "HTTP/2.0"}});
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Upgrade' header must not appear more than once in a response",
            failure_message());
}

// There must only be one correct upgrade header.
TEST_F(WebSocketStreamCreateTest, IncorrectUpgradeHeader) {
  static const char kMissingUpgradeResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Upgrade: hogefuga\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kMissingUpgradeResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Upgrade' header value is not 'WebSocket': hogefuga",
            failure_message());
}

// Connection header must be present.
TEST_F(WebSocketStreamCreateTest, MissingConnectionHeader) {
  static const char kMissingConnectionResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kMissingConnectionResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Connection' header is missing",
            failure_message());
}

// Connection header must contain "Upgrade".
TEST_F(WebSocketStreamCreateTest, IncorrectConnectionHeader) {
  static const char kMissingConnectionResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Connection: hogefuga\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kMissingConnectionResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Connection' header value must contain 'Upgrade'",
            failure_message());
}

// Connection header is permitted to contain other tokens.
TEST_F(WebSocketStreamCreateTest, AdditionalTokenInConnectionHeader) {
  static const char kAdditionalConnectionTokenResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade, Keep-Alive\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kAdditionalConnectionTokenResponse);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
}

// Sec-WebSocket-Accept header must be present.
TEST_F(WebSocketStreamCreateTest, MissingSecWebSocketAccept) {
  static const char kMissingAcceptResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kMissingAcceptResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Sec-WebSocket-Accept' header is missing",
            failure_message());
}

// Sec-WebSocket-Accept header must match the key that was sent.
TEST_F(WebSocketStreamCreateTest, WrongSecWebSocketAccept) {
  static const char kIncorrectAcceptResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: x/byyPZ2tOFvJCGkkugcKvqhhPk=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kIncorrectAcceptResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Incorrect 'Sec-WebSocket-Accept' header value",
            failure_message());
}

// Cancellation works.
TEST_F(WebSocketStreamCreateTest, Cancellation) {
  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {});
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Connect failure must look just like negotiation failure.
TEST_F(WebSocketStreamCreateTest, ConnectionFailure) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error in connection establishment: net::ERR_CONNECTION_REFUSED",
            failure_message());
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Connect timeout must look just like any other failure.
TEST_F(WebSocketStreamCreateTest, ConnectionTimeout) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(
      MockConnect(ASYNC, ERR_CONNECTION_TIMED_OUT));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT",
            failure_message());
}

// The server doesn't respond to the opening handshake.
TEST_F(WebSocketStreamCreateTest, HandshakeTimeout) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  std::unique_ptr<MockWeakTimer> timer(new MockWeakTimer(false, false));
  base::WeakPtr<MockWeakTimer> weak_timer = timer->AsWeakPtr();
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data),
                                  std::move(timer));
  EXPECT_FALSE(has_failed());
  ASSERT_TRUE(weak_timer.get());
  EXPECT_TRUE(weak_timer->IsRunning());

  weak_timer->Fire();
  WaitUntilConnectDone();

  EXPECT_TRUE(has_failed());
  EXPECT_EQ("WebSocket opening handshake timed out", failure_message());
  ASSERT_TRUE(weak_timer.get());
  EXPECT_FALSE(weak_timer->IsRunning());
}

// When the connection establishes the timer should be stopped.
TEST_F(WebSocketStreamCreateTest, HandshakeTimerOnSuccess) {
  std::unique_ptr<MockWeakTimer> timer(new MockWeakTimer(false, false));
  base::WeakPtr<MockWeakTimer> weak_timer = timer->AsWeakPtr();

  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {},
                           std::move(timer));
  ASSERT_TRUE(weak_timer);
  EXPECT_TRUE(weak_timer->IsRunning());

  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(weak_timer);
  EXPECT_FALSE(weak_timer->IsRunning());
}

// When the connection fails the timer should be stopped.
TEST_F(WebSocketStreamCreateTest, HandshakeTimerOnFailure) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  std::unique_ptr<MockWeakTimer> timer(new MockWeakTimer(false, false));
  base::WeakPtr<MockWeakTimer> weak_timer = timer->AsWeakPtr();
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data),
                                  std::move(timer));
  ASSERT_TRUE(weak_timer.get());
  EXPECT_TRUE(weak_timer->IsRunning());

  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error in connection establishment: net::ERR_CONNECTION_REFUSED",
            failure_message());
  ASSERT_TRUE(weak_timer.get());
  EXPECT_FALSE(weak_timer->IsRunning());
}

// Cancellation during connect works.
TEST_F(WebSocketStreamCreateTest, CancellationDuringConnect) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
}

// Cancellation during write of the request headers works.
TEST_F(WebSocketStreamCreateTest, CancellationDuringWrite) {
  // First write never completes.
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  SequencedSocketData* socket_data(
      new SequencedSocketData(NULL, 0, writes, arraysize(writes)));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "",
                                  base::WrapUnique(socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(socket_data->AllWriteDataConsumed());
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Cancellation during read of the response headers works.
TEST_F(WebSocketStreamCreateTest, CancellationDuringRead) {
  std::string request =
      WebSocketStandardRequest("/", "www.example.org", Origin(), "", "");
  MockWrite writes[] = {MockWrite(ASYNC, 0, request.c_str())};
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1),
  };
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  SequencedSocketData* socket_data_raw_ptr = socket_data.get();
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(socket_data_raw_ptr->AllReadDataConsumed());
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Over-size response headers (> 256KB) should not cause a crash.  This is a
// regression test for crbug.com/339456. It is based on the layout test
// "cookie-flood.html".
TEST_F(WebSocketStreamCreateTest, VeryLargeResponseHeaders) {
  std::string set_cookie_headers;
  set_cookie_headers.reserve(24 * 20000);
  for (int i = 0; i < 20000; ++i) {
    set_cookie_headers += base::StringPrintf("Set-Cookie: ws-%d=1\r\n", i);
  }
  ASSERT_GT(set_cookie_headers.size(), 256U * 1024U);
  CreateAndConnectStringResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(),
                                 set_cookie_headers);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_FALSE(response_info_);
}

// If the remote host closes the connection without sending headers, we should
// log the console message "Connection closed before receiving a handshake
// response".
TEST_F(WebSocketStreamCreateTest, NoResponse) {
  std::string request =
      WebSocketStandardRequest("/", "www.example.org", Origin(), "", "");
  MockWrite writes[] = {MockWrite(ASYNC, request.data(), request.size(), 0)};
  MockRead reads[] = {MockRead(ASYNC, 0, 1)};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  SequencedSocketData* socket_data_raw_ptr = socket_data.get();
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(socket_data_raw_ptr->AllReadDataConsumed());
  EXPECT_TRUE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_FALSE(response_info_);
  EXPECT_EQ("Connection closed before receiving a handshake response",
            failure_message());
}

TEST_F(WebSocketStreamCreateTest, SelfSignedCertificateFailure) {
  ssl_data_.push_back(std::make_unique<SSLSocketDataProvider>(
      ASYNC, ERR_CERT_AUTHORITY_INVALID));
  ssl_data_[0]->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_data_[0]->ssl_info.cert.get());
  std::unique_ptr<SequencedSocketData> raw_socket_data(BuildNullSocketData());
  CreateAndConnectRawExpectations("wss://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "",
                                  std::move(raw_socket_data));
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  ASSERT_TRUE(ssl_error_callbacks_);
  ssl_error_callbacks_->CancelSSLRequest(ERR_CERT_AUTHORITY_INVALID,
                                         &ssl_info_);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
}

TEST_F(WebSocketStreamCreateTest, SelfSignedCertificateSuccess) {
  ssl_data_.push_back(std::make_unique<SSLSocketDataProvider>(
      ASYNC, ERR_CERT_AUTHORITY_INVALID));
  ssl_data_[0]->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_data_[0]->ssl_info.cert.get());
  ssl_data_.push_back(std::make_unique<SSLSocketDataProvider>(ASYNC, OK));
  url_request_context_host_.AddRawExpectations(BuildNullSocketData());
  CreateAndConnectStandard("wss://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {});
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ssl_error_callbacks_);
  ssl_error_callbacks_->ContinueSSLRequest();
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
}

// If the server requests authorisation, but we have no credentials, the
// connection should fail cleanly.
TEST_F(WebSocketStreamCreateBasicAuthTest, FailureNoCredentials) {
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kUnauthorizedResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("HTTP Authentication failed; no valid credentials available",
            failure_message());
  EXPECT_TRUE(response_info_);
}

TEST_F(WebSocketStreamCreateBasicAuthTest, SuccessPasswordInUrl) {
  CreateAndConnectAuthHandshake("ws://foo:bar@www.example.org/", "Zm9vOmJhcg==",
                                WebSocketStandardResponse(std::string()));
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(response_info_);
  EXPECT_EQ(101, response_info_->status_code);
}

TEST_F(WebSocketStreamCreateBasicAuthTest, FailureIncorrectPasswordInUrl) {
  CreateAndConnectAuthHandshake("ws://foo:baz@www.example.org/",
                                "Zm9vOmJheg==", kUnauthorizedResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_TRUE(response_info_);
}

// Digest auth has the same connection semantics as Basic auth, so we can
// generally assume that whatever works for Basic auth will also work for
// Digest. There's just one test here, to confirm that it works at all.
TEST_F(WebSocketStreamCreateDigestAuthTest, DigestPasswordInUrl) {
  url_request_context_host_.AddRawExpectations(
      helper_.BuildSocketData1(kUnauthorizedResponse));

  CreateAndConnectRawExpectations(
      "ws://FooBar:pass@www.example.org/", NoSubProtocols(), Origin(), Url(),
      "",
      helper_.BuildSocketData2(kAuthorizedRequest,
                               WebSocketStandardResponse(std::string())));
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(response_info_);
  EXPECT_EQ(101, response_info_->status_code);
}

TEST_F(WebSocketStreamCreateUMATest, Incomplete) {
  base::HistogramTester histogram_tester;

  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {});
  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult");
  EXPECT_EQ(1, samples->GetCount(INCOMPLETE));
  EXPECT_EQ(0, samples->GetCount(CONNECTED));
  EXPECT_EQ(0, samples->GetCount(FAILED));
}

TEST_F(WebSocketStreamCreateUMATest, Connected) {
  base::HistogramTester histogram_tester;

  CreateAndConnectStandard("ws://www.example.org/", "www.example.org", "/",
                           NoSubProtocols(), Origin(), Url(), {}, {}, {});
  WaitUntilConnectDone();
  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult");
  EXPECT_EQ(0, samples->GetCount(INCOMPLETE));
  EXPECT_EQ(1, samples->GetCount(CONNECTED));
  EXPECT_EQ(0, samples->GetCount(FAILED));
}

TEST_F(WebSocketStreamCreateUMATest, Failed) {
  base::HistogramTester histogram_tester;

  static const char kInvalidStatusCodeResponse[] =
      "HTTP/1.1 200 OK\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", "www.example.org",
                                 "/", NoSubProtocols(), Origin(), Url(), {}, {},
                                 kInvalidStatusCodeResponse);
  WaitUntilConnectDone();
  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult");
  EXPECT_EQ(1, samples->GetCount(INCOMPLETE));
  EXPECT_EQ(0, samples->GetCount(CONNECTED));
  EXPECT_EQ(0, samples->GetCount(FAILED));
}

TEST_F(WebSocketStreamCreateTest, HandleErrConnectionClosed) {
  static const char kTruncatedResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Cache-Control: no-sto";

  std::string request =
      WebSocketStandardRequest("/", "www.example.org", Origin(), "", "");
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 1, kTruncatedResponse),
      MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED, 2),
  };
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, request.c_str())};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
}

TEST_F(WebSocketStreamCreateTest, HandleErrTunnelConnectionFailed) {
  static const char kConnectRequest[] =
      "CONNECT www.example.org:80 HTTP/1.1\r\n"
      "Host: www.example.org:80\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "\r\n";

  static const char kProxyResponse[] =
      "HTTP/1.1 403 Forbidden\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: 9\r\n"
      "Connection: keep-alive\r\n"
      "\r\n"
      "Forbidden";

  MockRead reads[] = {MockRead(SYNCHRONOUS, 1, kProxyResponse)};
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, kConnectRequest)};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  url_request_context_host_.SetProxyConfig("https=proxy:8000");
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  Origin(), Url(), "", std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Establishing a tunnel via proxy server failed.",
            failure_message());
}

}  // namespace
}  // namespace net

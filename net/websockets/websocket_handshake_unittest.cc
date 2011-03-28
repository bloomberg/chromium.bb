// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/websockets/websocket_handshake.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class WebSocketHandshakeTest : public testing::Test {
 public:
  static void SetUpParameter(WebSocketHandshake* handshake,
                             uint32 number_1, uint32 number_2,
                             const std::string& key_1, const std::string& key_2,
                             const std::string& key_3) {
    WebSocketHandshake::Parameter* parameter =
        new WebSocketHandshake::Parameter;
    parameter->number_1_ = number_1;
    parameter->number_2_ = number_2;
    parameter->key_1_ = key_1;
    parameter->key_2_ = key_2;
    parameter->key_3_ = key_3;
    handshake->parameter_.reset(parameter);
  }

  static void ExpectHeaderEquals(const std::string& expected,
                          const std::string& actual) {
    std::vector<std::string> expected_lines;
    Tokenize(expected, "\r\n", &expected_lines);
    std::vector<std::string> actual_lines;
    Tokenize(actual, "\r\n", &actual_lines);
    // Request lines.
    EXPECT_EQ(expected_lines[0], actual_lines[0]);

    std::vector<std::string> expected_headers;
    for (size_t i = 1; i < expected_lines.size(); i++) {
      // Finish at first CRLF CRLF.  Note that /key_3/ might include CRLF.
      if (expected_lines[i] == "")
        break;
      expected_headers.push_back(expected_lines[i]);
    }
    sort(expected_headers.begin(), expected_headers.end());

    std::vector<std::string> actual_headers;
    for (size_t i = 1; i < actual_lines.size(); i++) {
      // Finish at first CRLF CRLF.  Note that /key_3/ might include CRLF.
      if (actual_lines[i] == "")
        break;
      actual_headers.push_back(actual_lines[i]);
    }
    sort(actual_headers.begin(), actual_headers.end());

    EXPECT_EQ(expected_headers.size(), actual_headers.size())
        << "expected:" << expected
        << "\nactual:" << actual;
    for (size_t i = 0; i < expected_headers.size(); i++) {
      EXPECT_EQ(expected_headers[i], actual_headers[i]);
    }
  }

  static void ExpectHandshakeMessageEquals(const std::string& expected,
                                           const std::string& actual) {
    // Headers.
    ExpectHeaderEquals(expected, actual);
    // Compare tailing \r\n\r\n<key3> (4 + 8 bytes).
    ASSERT_GT(expected.size(), 12U);
    const char* expected_key3 = expected.data() + expected.size() - 12;
    EXPECT_GT(actual.size(), 12U);
    if (actual.size() <= 12U)
      return;
    const char* actual_key3 = actual.data() + actual.size() - 12;
    EXPECT_TRUE(memcmp(expected_key3, actual_key3, 12) == 0)
        << "expected_key3:" << DumpKey(expected_key3, 12)
        << ", actual_key3:" << DumpKey(actual_key3, 12);
  }

  static std::string DumpKey(const char* buf, int len) {
    std::string s;
    for (int i = 0; i < len; i++) {
      if (isprint(buf[i]))
        s += base::StringPrintf("%c", buf[i]);
      else
        s += base::StringPrintf("\\x%02x", buf[i]);
    }
    return s;
  }

  static std::string GetResourceName(WebSocketHandshake* handshake) {
    return handshake->GetResourceName();
  }
  static std::string GetHostFieldValue(WebSocketHandshake* handshake) {
    return handshake->GetHostFieldValue();
  }
  static std::string GetOriginFieldValue(WebSocketHandshake* handshake) {
    return handshake->GetOriginFieldValue();
  }
};


TEST_F(WebSocketHandshakeTest, Connect) {
  const std::string kExpectedClientHandshakeMessage =
      "GET /demo HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Host: example.com\r\n"
      "Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Key1: 388P O503D&ul7 {K%gX( %7  15\r\n"
      "Sec-WebSocket-Key2: 1 N ?|k UT0or 3o  4 I97N 5-S3O 31\r\n"
      "\r\n"
      "\x47\x30\x22\x2D\x5A\x3F\x47\x58";

  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com/demo"),
                             "http://example.com",
                             "ws://example.com/demo",
                             "sample"));
  SetUpParameter(handshake.get(), 777007543U, 114997259U,
                 "388P O503D&ul7 {K%gX( %7  15",
                 "1 N ?|k UT0or 3o  4 I97N 5-S3O 31",
                 std::string("\x47\x30\x22\x2D\x5A\x3F\x47\x58", 8));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  ExpectHandshakeMessageEquals(
      kExpectedClientHandshakeMessage,
      handshake->CreateClientHandshakeMessage());

  const char kResponse[] = "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "\x30\x73\x74\x33\x52\x6C\x26\x71\x2D\x32\x5A\x55\x5E\x77\x65\x75";
  std::vector<std::string> response_lines;
  base::SplitStringDontTrim(kResponse, '\n', &response_lines);

  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  // too short
  EXPECT_EQ(-1, handshake->ReadServerHandshake(kResponse, 16));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());

  // only status line
  std::string response = response_lines[0];
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      response.data(), response.size()));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  // by upgrade header
  response += response_lines[1];
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      response.data(), response.size()));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  // by connection header
  response += response_lines[2];
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      response.data(), response.size()));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());

  response += response_lines[3];  // Sec-WebSocket-Origin
  response += response_lines[4];  // Sec-WebSocket-Location
  response += response_lines[5];  // Sec-WebSocket-Protocol
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      response.data(), response.size()));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());

  response += response_lines[6];  // \r\n
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      response.data(), response.size()));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());

  int handshake_length = sizeof(kResponse) - 1;  // -1 for terminating \0
  EXPECT_EQ(handshake_length, handshake->ReadServerHandshake(
      kResponse, handshake_length));  // -1 for terminating \0
  EXPECT_EQ(WebSocketHandshake::MODE_CONNECTED, handshake->mode());
}

TEST_F(WebSocketHandshakeTest, ServerSentData) {
  const std::string kExpectedClientHandshakeMessage =
      "GET /demo HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Host: example.com\r\n"
      "Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Key1: 388P O503D&ul7 {K%gX( %7  15\r\n"
      "Sec-WebSocket-Key2: 1 N ?|k UT0or 3o  4 I97N 5-S3O 31\r\n"
      "\r\n"
      "\x47\x30\x22\x2D\x5A\x3F\x47\x58";
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com/demo"),
                             "http://example.com",
                             "ws://example.com/demo",
                             "sample"));
  SetUpParameter(handshake.get(), 777007543U, 114997259U,
                 "388P O503D&ul7 {K%gX( %7  15",
                 "1 N ?|k UT0or 3o  4 I97N 5-S3O 31",
                 std::string("\x47\x30\x22\x2D\x5A\x3F\x47\x58", 8));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  ExpectHandshakeMessageEquals(
      kExpectedClientHandshakeMessage,
      handshake->CreateClientHandshakeMessage());

  const char kResponse[] = "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://example.com\r\n"
      "Sec-WebSocket-Location: ws://example.com/demo\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "\r\n"
      "\x30\x73\x74\x33\x52\x6C\x26\x71\x2D\x32\x5A\x55\x5E\x77\x65\x75"
      "\0Hello\xff";

  int handshake_length = strlen(kResponse);  // key3 doesn't contain \0.
  EXPECT_EQ(handshake_length, handshake->ReadServerHandshake(
      kResponse, sizeof(kResponse) - 1));  // -1 for terminating \0
  EXPECT_EQ(WebSocketHandshake::MODE_CONNECTED, handshake->mode());
}

TEST_F(WebSocketHandshakeTest, is_secure_false) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com/demo"),
                             "http://example.com",
                             "ws://example.com/demo",
                             "sample"));
  EXPECT_FALSE(handshake->is_secure());
}

TEST_F(WebSocketHandshakeTest, is_secure_true) {
  // wss:// is secure.
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("wss://example.com/demo"),
                             "http://example.com",
                             "wss://example.com/demo",
                             "sample"));
  EXPECT_TRUE(handshake->is_secure());
}

TEST_F(WebSocketHandshakeTest, CreateClientHandshakeMessage_ResourceName) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com/Test?q=xxx&p=%20"),
                             "http://example.com",
                             "ws://example.com/demo",
                             "sample"));
  // Path and query should be preserved as-is.
  EXPECT_EQ("/Test?q=xxx&p=%20", GetResourceName(handshake.get()));
}

TEST_F(WebSocketHandshakeTest, CreateClientHandshakeMessage_Host) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://Example.Com/demo"),
                             "http://Example.Com",
                             "ws://Example.Com/demo",
                             "sample"));
  // Host should be lowercased
  EXPECT_EQ("example.com", GetHostFieldValue(handshake.get()));
  EXPECT_EQ("http://example.com", GetOriginFieldValue(handshake.get()));
}

TEST_F(WebSocketHandshakeTest, CreateClientHandshakeMessage_TrimPort80) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com:80/demo"),
                             "http://example.com",
                             "ws://example.com/demo",
                             "sample"));
  // :80 should be trimmed as it's the default port for ws://.
  EXPECT_EQ("example.com", GetHostFieldValue(handshake.get()));
}

TEST_F(WebSocketHandshakeTest, CreateClientHandshakeMessage_TrimPort443) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("wss://example.com:443/demo"),
                             "http://example.com",
                             "wss://example.com/demo",
                             "sample"));
  // :443 should be trimmed as it's the default port for wss://.
  EXPECT_EQ("example.com", GetHostFieldValue(handshake.get()));
}

TEST_F(WebSocketHandshakeTest,
       CreateClientHandshakeMessage_NonDefaultPortForWs) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com:8080/demo"),
                             "http://example.com",
                             "wss://example.com/demo",
                             "sample"));
  // :8080 should be preserved as it's not the default port for ws://.
  EXPECT_EQ("example.com:8080", GetHostFieldValue(handshake.get()));
}

TEST_F(WebSocketHandshakeTest,
     CreateClientHandshakeMessage_NonDefaultPortForWss) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("wss://example.com:4443/demo"),
                             "http://example.com",
                             "wss://example.com/demo",
                             "sample"));
  // :4443 should be preserved as it's not the default port for wss://.
  EXPECT_EQ("example.com:4443", GetHostFieldValue(handshake.get()));
}

TEST_F(WebSocketHandshakeTest, CreateClientHandshakeMessage_WsBut443) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("ws://example.com:443/demo"),
                             "http://example.com",
                             "ws://example.com/demo",
                             "sample"));
  // :443 should be preserved as it's not the default port for ws://.
  EXPECT_EQ("example.com:443", GetHostFieldValue(handshake.get()));
}

TEST_F(WebSocketHandshakeTest, CreateClientHandshakeMessage_WssBut80) {
  scoped_ptr<WebSocketHandshake> handshake(
      new WebSocketHandshake(GURL("wss://example.com:80/demo"),
                             "http://example.com",
                             "wss://example.com/demo",
                             "sample"));
  // :80 should be preserved as it's not the default port for wss://.
  EXPECT_EQ("example.com:80", GetHostFieldValue(handshake.get()));
}

}  // namespace net

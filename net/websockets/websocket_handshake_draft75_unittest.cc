// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/websockets/websocket_handshake_draft75.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace net {

TEST(WebSocketHandshakeDraft75Test, Connect) {
  const std::string kExpectedClientHandshakeMessage =
      "GET /demo HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Host: example.com\r\n"
      "Origin: http://example.com\r\n"
      "WebSocket-Protocol: sample\r\n"
      "\r\n";

  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com/demo"),
                                    "http://example.com",
                                    "ws://example.com/demo",
                                    "sample"));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  EXPECT_EQ(kExpectedClientHandshakeMessage,
            handshake->CreateClientHandshakeMessage());

  const char kResponse[] = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "WebSocket-Origin: http://example.com\r\n"
      "WebSocket-Location: ws://example.com/demo\r\n"
      "WebSocket-Protocol: sample\r\n"
      "\r\n";

  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  // too short
  EXPECT_EQ(-1, handshake->ReadServerHandshake(kResponse, 16));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  // only status line
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      kResponse,
      WebSocketHandshakeDraft75::kServerHandshakeHeaderLength));
  EXPECT_EQ(WebSocketHandshake::MODE_NORMAL, handshake->mode());
  // by upgrade header
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      kResponse,
      WebSocketHandshakeDraft75::kServerHandshakeHeaderLength +
      WebSocketHandshakeDraft75::kUpgradeHeaderLength));
  EXPECT_EQ(WebSocketHandshake::MODE_NORMAL, handshake->mode());
  // by connection header
  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      kResponse,
      WebSocketHandshakeDraft75::kServerHandshakeHeaderLength +
      WebSocketHandshakeDraft75::kUpgradeHeaderLength +
      WebSocketHandshakeDraft75::kConnectionHeaderLength));
  EXPECT_EQ(WebSocketHandshake::MODE_NORMAL, handshake->mode());

  EXPECT_EQ(-1, handshake->ReadServerHandshake(
      kResponse, sizeof(kResponse) - 2));
  EXPECT_EQ(WebSocketHandshake::MODE_NORMAL, handshake->mode());

  int handshake_length = strlen(kResponse);
  EXPECT_EQ(handshake_length, handshake->ReadServerHandshake(
      kResponse, sizeof(kResponse) - 1));  // -1 for terminating \0
  EXPECT_EQ(WebSocketHandshake::MODE_CONNECTED, handshake->mode());
}

TEST(WebSocketHandshakeDraft75Test, ServerSentData) {
  const std::string kExpectedClientHandshakeMessage =
      "GET /demo HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Host: example.com\r\n"
      "Origin: http://example.com\r\n"
      "WebSocket-Protocol: sample\r\n"
      "\r\n";
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com/demo"),
                                    "http://example.com",
                                    "ws://example.com/demo",
                                    "sample"));
  EXPECT_EQ(WebSocketHandshake::MODE_INCOMPLETE, handshake->mode());
  EXPECT_EQ(kExpectedClientHandshakeMessage,
            handshake->CreateClientHandshakeMessage());

  const char kResponse[] ="HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "WebSocket-Origin: http://example.com\r\n"
      "WebSocket-Location: ws://example.com/demo\r\n"
      "WebSocket-Protocol: sample\r\n"
      "\r\n"
      "\0Hello\xff";

  int handshake_length = strlen(kResponse);
  EXPECT_EQ(handshake_length, handshake->ReadServerHandshake(
      kResponse, sizeof(kResponse) - 1));  // -1 for terminating \0
  EXPECT_EQ(WebSocketHandshake::MODE_CONNECTED, handshake->mode());
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_Simple) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com/demo"),
                                    "http://example.com",
                                    "ws://example.com/demo",
                                    "sample"));
  EXPECT_EQ("GET /demo HTTP/1.1\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: Upgrade\r\n"
            "Host: example.com\r\n"
            "Origin: http://example.com\r\n"
            "WebSocket-Protocol: sample\r\n"
            "\r\n",
            handshake->CreateClientHandshakeMessage());
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_PathAndQuery) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com/Test?q=xxx&p=%20"),
                                    "http://example.com",
                                    "ws://example.com/demo",
                                    "sample"));
  // Path and query should be preserved as-is.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("GET /Test?q=xxx&p=%20 HTTP/1.1\r\n"));
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_Host) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://Example.Com/demo"),
                                    "http://Example.Com",
                                    "ws://Example.Com/demo",
                                    "sample"));
  // Host should be lowercased
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com\r\n"));
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Origin: http://example.com\r\n"));
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_TrimPort80) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com:80/demo"),
                                    "http://example.com",
                                    "ws://example.com/demo",
                                    "sample"));
  // :80 should be trimmed as it's the default port for ws://.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com\r\n"));
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_TrimPort443) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("wss://example.com:443/demo"),
                                    "http://example.com",
                                    "wss://example.com/demo",
                                    "sample"));
  // :443 should be trimmed as it's the default port for wss://.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com\r\n"));
}

TEST(WebSocketHandshakeDraft75Test,
     CreateClientHandshakeMessage_NonDefaultPortForWs) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com:8080/demo"),
                                    "http://example.com",
                                    "wss://example.com/demo",
                                    "sample"));
  // :8080 should be preserved as it's not the default port for ws://.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com:8080\r\n"));
}

TEST(WebSocketHandshakeDraft75Test,
     CreateClientHandshakeMessage_NonDefaultPortForWss) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("wss://example.com:4443/demo"),
                                    "http://example.com",
                                    "wss://example.com/demo",
                                    "sample"));
  // :4443 should be preserved as it's not the default port for wss://.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com:4443\r\n"));
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_WsBut443) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("ws://example.com:443/demo"),
                                    "http://example.com",
                                    "ws://example.com/demo",
                                    "sample"));
  // :443 should be preserved as it's not the default port for ws://.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com:443\r\n"));
}

TEST(WebSocketHandshakeDraft75Test, CreateClientHandshakeMessage_WssBut80) {
  scoped_ptr<WebSocketHandshakeDraft75> handshake(
      new WebSocketHandshakeDraft75(GURL("wss://example.com:80/demo"),
                                    "http://example.com",
                                    "wss://example.com/demo",
                                    "sample"));
  // :80 should be preserved as it's not the default port for wss://.
  EXPECT_THAT(handshake->CreateClientHandshakeMessage(),
              testing::HasSubstr("Host: example.com:80\r\n"));
}

}  // namespace net

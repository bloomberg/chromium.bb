// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_DRAFT75_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_DRAFT75_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "net/websockets/websocket_handshake.h"

namespace net {

class HttpResponseHeaders;

class WebSocketHandshakeDraft75 : public WebSocketHandshake {
 public:
  static const int kWebSocketPort;
  static const int kSecureWebSocketPort;
  static const char kServerHandshakeHeader[];
  static const size_t kServerHandshakeHeaderLength;
  static const char kUpgradeHeader[];
  static const size_t kUpgradeHeaderLength;
  static const char kConnectionHeader[];
  static const size_t kConnectionHeaderLength;

  WebSocketHandshakeDraft75(const GURL& url,
                            const std::string& origin,
                            const std::string& location,
                            const std::string& protocol);
  virtual ~WebSocketHandshakeDraft75();

  // Creates the client handshake message from |this|.
  virtual std::string CreateClientHandshakeMessage();

  // Reads server handshake message in |len| of |data|, updates |mode_| and
  // returns number of bytes of the server handshake message.
  // Once connection is established, |mode_| will be MODE_CONNECTED.
  // If connection establishment failed, |mode_| will be MODE_FAILED.
  // Returns negative if the server handshake message is incomplete.
  virtual int ReadServerHandshake(const char* data, size_t len);

 private:
  // Processes server handshake message, parsed as |headers|, and updates
  // |ws_origin_|, |ws_location_| and |ws_protocol_|.
  // Returns true if it's ok.
  // Returns false otherwise (e.g. duplicate WebSocket-Origin: header, etc.)
  virtual bool ProcessHeaders(const HttpResponseHeaders& headers);

  // Checks |ws_origin_|, |ws_location_| and |ws_protocol_| are valid
  // against |origin_|, |location_| and |protocol_|.
  // Returns true if it's ok.
  // Returns false otherwise (e.g. origin mismatch, etc.)
  virtual bool CheckResponseHeaders() const;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeDraft75);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_DRAFT75_H_

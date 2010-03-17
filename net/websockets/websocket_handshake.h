// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_H_

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace net {

class HttpResponseHeaders;

class WebSocketHandshake {
 public:
  static const int kWebSocketPort;
  static const int kSecureWebSocketPort;
  static const char kServerHandshakeHeader[];
  static const size_t kServerHandshakeHeaderLength;
  static const char kUpgradeHeader[];
  static const size_t kUpgradeHeaderLength;
  static const char kConnectionHeader[];
  static const size_t kConnectionHeaderLength;

  enum Mode {
    MODE_INCOMPLETE, MODE_NORMAL, MODE_FAILED, MODE_CONNECTED
  };
  WebSocketHandshake(const GURL& url,
                     const std::string& origin,
                     const std::string& location,
                     const std::string& protocol);
  ~WebSocketHandshake();

  bool is_secure() const;
  // Creates the client handshake message from |this|.
  std::string CreateClientHandshakeMessage() const;

  // Reads server handshake message in |len| of |data|, updates |mode_| and
  // returns number of bytes of the server handshake message.
  // Once connection is established, |mode_| will be MODE_CONNECTED.
  // If connection establishment failed, |mode_| will be MODE_FAILED.
  // Returns negative if the server handshake message is incomplete.
  int ReadServerHandshake(const char* data, size_t len);
  Mode mode() const { return mode_; }

 private:
  // Processes server handshake message, parsed as |headers|, and updates
  // |ws_origin_|, |ws_location_| and |ws_protocol_|.
  // Returns true if it's ok.
  // Returns false otherwise (e.g. duplicate WebSocket-Origin: header, etc.)
  bool ProcessHeaders(const HttpResponseHeaders& headers);

  // Checks |ws_origin_|, |ws_location_| and |ws_protocol_| are valid
  // against |origin_|, |location_| and |protocol_|.
  // Returns true if it's ok.
  // Returns false otherwise (e.g. origin mismatch, etc.)
  bool CheckResponseHeaders() const;

  GURL url_;
  // Handshake messages that the client is going to send out.
  std::string origin_;
  std::string location_;
  std::string protocol_;

  Mode mode_;

  // Handshake messages that server sent.
  std::string ws_origin_;
  std::string ws_location_;
  std::string ws_protocol_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshake);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_H_

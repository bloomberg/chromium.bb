// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_

// This file is included from net/http files.
// Since net/http can be built without linking net/websockets code,
// this file must not introduce any link-time dependencies on websockets.

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "net/base/net_export.h"
#include "net/http/http_stream.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class ClientSocketHandle;
class SpdySession;
class HttpRequestHeaders;
class HttpResponseHeaders;

// WebSocketHandshakeStreamBase is the base class of
// WebSocketBasicHandshakeStream.  net/http code uses this interface to handle
// WebSocketBasicHandshakeStream when it needs to be treated differently from
// HttpStreamBase.
class NET_EXPORT WebSocketHandshakeStreamBase : public HttpStream {
 public:
  WebSocketHandshakeStreamBase() = default;
  ~WebSocketHandshakeStreamBase() override = default;

  // An object that stores data needed for the creation of a
  // WebSocketBasicHandshakeStream object. A new CreateHelper is used for each
  // WebSocket connection.
  class NET_EXPORT_PRIVATE CreateHelper : public base::SupportsUserData::Data {
   public:
    ~CreateHelper() override {}

    // Create a WebSocketBasicHandshakeStream. This is called after the
    // underlying connection has been established but before any handshake data
    // has been transferred. This can be called more than once in the case that
    // HTTP authentication is needed.
    virtual std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
        std::unique_ptr<ClientSocketHandle> connection,
        bool using_proxy) = 0;

    // Create a WebSocketHttp2HandshakeStream. This is called after the
    // underlying HTTP/2 connection has been established but before the stream
    // has been opened.  This cannot be called more than once.
    virtual std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp2Stream(
        base::WeakPtr<SpdySession> session) = 0;
  };

  // After the handshake has completed, this method creates a WebSocketStream
  // (of the appropriate type) from the WebSocketHandshakeStreamBase object.
  // The WebSocketHandshakeStreamBase object is unusable after Upgrade() has
  // been called.
  virtual std::unique_ptr<WebSocketStream> Upgrade() = 0;

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override {}

  static std::string MultipleHeaderValuesMessage(
      const std::string& header_name);

 protected:
  // TODO(ricea): If more extensions are added, replace this with a more general
  // mechanism.
  struct WebSocketExtensionParams {
    bool deflate_enabled = false;
    WebSocketDeflateParameters deflate_parameters;
  };

  static void AddVectorHeaderIfNonEmpty(const char* name,
                                        const std::vector<std::string>& value,
                                        HttpRequestHeaders* headers);

  static bool ValidateSubProtocol(
      const HttpResponseHeaders* headers,
      const std::vector<std::string>& requested_sub_protocols,
      std::string* sub_protocol,
      std::string* failure_message);

  static bool ValidateExtensions(const HttpResponseHeaders* headers,
                                 std::string* accepted_extensions_descriptor,
                                 std::string* failure_message,
                                 WebSocketExtensionParams* params);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeStreamBase);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_

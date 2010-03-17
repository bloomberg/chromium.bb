// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake.h"

#include "base/ref_counted.h"
#include "base/string_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

const int WebSocketHandshake::kWebSocketPort = 80;
const int WebSocketHandshake::kSecureWebSocketPort = 443;

const char WebSocketHandshake::kServerHandshakeHeader[] =
    "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
const size_t WebSocketHandshake::kServerHandshakeHeaderLength =
    sizeof(kServerHandshakeHeader) - 1;

const char WebSocketHandshake::kUpgradeHeader[] = "Upgrade: WebSocket\r\n";
const size_t WebSocketHandshake::kUpgradeHeaderLength =
    sizeof(kUpgradeHeader) - 1;

const char WebSocketHandshake::kConnectionHeader[] = "Connection: Upgrade\r\n";
const size_t WebSocketHandshake::kConnectionHeaderLength =
    sizeof(kConnectionHeader) - 1;

WebSocketHandshake::WebSocketHandshake(
    const GURL& url,
    const std::string& origin,
    const std::string& location,
    const std::string& protocol)
    : url_(url),
      origin_(origin),
      location_(location),
      protocol_(protocol),
      mode_(MODE_INCOMPLETE) {
}

WebSocketHandshake::~WebSocketHandshake() {
}

bool WebSocketHandshake::is_secure() const {
  return url_.SchemeIs("wss");
}

std::string WebSocketHandshake::CreateClientHandshakeMessage() const {
  std::string msg;
  msg = "GET ";
  msg += url_.path();
  if (url_.has_query()) {
    msg += "?";
    msg += url_.query();
  }
  msg += " HTTP/1.1\r\n";
  msg += kUpgradeHeader;
  msg += kConnectionHeader;
  msg += "Host: ";
  msg += StringToLowerASCII(url_.host());
  if (url_.has_port()) {
    bool secure = is_secure();
    int port = url_.EffectiveIntPort();
    if ((!secure &&
         port != kWebSocketPort && port != url_parse::PORT_UNSPECIFIED) ||
        (secure &&
         port != kSecureWebSocketPort && port != url_parse::PORT_UNSPECIFIED)) {
      msg += ":";
      msg += IntToString(port);
    }
  }
  msg += "\r\n";
  msg += "Origin: ";
  // It's OK to lowercase the origin as the Origin header does not contain
  // the path or query portions, as per
  // http://tools.ietf.org/html/draft-abarth-origin-00.
  //
  // TODO(satorux): Should we trim the port portion here if it's 80 for
  // http:// or 443 for https:// ? Or can we assume it's done by the
  // client of the library?
  msg += StringToLowerASCII(origin_);
  msg += "\r\n";
  if (!protocol_.empty()) {
    msg += "WebSocket-Protocol: ";
    msg += protocol_;
    msg += "\r\n";
  }
  // TODO(ukai): Add cookie if necessary.
  msg += "\r\n";
  return msg;
}

int WebSocketHandshake::ReadServerHandshake(const char* data, size_t len) {
  mode_ = MODE_INCOMPLETE;
  if (len < kServerHandshakeHeaderLength) {
    return -1;
  }
  if (!memcmp(data, kServerHandshakeHeader, kServerHandshakeHeaderLength)) {
    mode_ = MODE_NORMAL;
  } else {
    int eoh = HttpUtil::LocateEndOfHeaders(data, len);
    if (eoh < 0)
      return -1;
    return eoh;
  }
  const char* p = data + kServerHandshakeHeaderLength;
  const char* end = data + len + 1;

  if (mode_ == MODE_NORMAL) {
    size_t header_size = end - p;
    if (header_size < kUpgradeHeaderLength)
      return -1;
    if (memcmp(p, kUpgradeHeader, kUpgradeHeaderLength)) {
      mode_ = MODE_FAILED;
      DLOG(INFO) << "Bad Upgrade Header "
                 << std::string(p, kUpgradeHeaderLength);
      return p - data;
    }
    p += kUpgradeHeaderLength;
    header_size = end - p;
    if (header_size < kConnectionHeaderLength)
      return -1;
    if (memcmp(p, kConnectionHeader, kConnectionHeaderLength)) {
      mode_ = MODE_FAILED;
      DLOG(INFO) << "Bad Connection Header "
                 << std::string(p, kConnectionHeaderLength);
      return p - data;
    }
    p += kConnectionHeaderLength;
  }

  int eoh = HttpUtil::LocateEndOfHeaders(data, len);
  if (eoh == -1)
    return eoh;

  scoped_refptr<HttpResponseHeaders> headers(
      new HttpResponseHeaders(HttpUtil::AssembleRawHeaders(data, eoh)));
  if (!ProcessHeaders(*headers)) {
    DLOG(INFO) << "Process Headers failed: "
               << std::string(data, eoh);
    mode_ = MODE_FAILED;
  }
  switch (mode_) {
    case MODE_NORMAL:
      if (CheckResponseHeaders()) {
        mode_ = MODE_CONNECTED;
      } else {
        mode_ = MODE_FAILED;
      }
      break;
    default:
      mode_ = MODE_FAILED;
      break;
  }
  return eoh;
}

// Gets the value of the specified header.
// It assures only one header of |name| in |headers|.
// Returns true iff single header of |name| is found in |headers|
// and |value| is filled with the value.
// Returns false otherwise.
static bool GetSingleHeader(const HttpResponseHeaders& headers,
                            const std::string& name,
                            std::string* value) {
  std::string first_value;
  void* iter = NULL;
  if (!headers.EnumerateHeader(&iter, name, &first_value))
    return false;

  // Checks no more |name| found in |headers|.
  // Second call of EnumerateHeader() must return false.
  std::string second_value;
  if (headers.EnumerateHeader(&iter, name, &second_value))
    return false;
  *value = first_value;
  return true;
}

bool WebSocketHandshake::ProcessHeaders(const HttpResponseHeaders& headers) {
  if (!GetSingleHeader(headers, "websocket-origin", &ws_origin_))
    return false;

  if (!GetSingleHeader(headers, "websocket-location", &ws_location_))
    return false;

  // If |protocol_| is not specified by client, we don't care if there's
  // protocol field or not as specified in the spec.
  if (!protocol_.empty()
      && !GetSingleHeader(headers, "websocket-protocol", &ws_protocol_))
    return false;
  return true;
}

bool WebSocketHandshake::CheckResponseHeaders() const {
  DCHECK(mode_ == MODE_NORMAL);
  if (!LowerCaseEqualsASCII(origin_, ws_origin_.c_str()))
    return false;
  if (location_ != ws_location_)
    return false;
  if (!protocol_.empty() && protocol_ != ws_protocol_)
    return false;
  return true;
}



}  // namespace net

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_draft75.h"

#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

const char WebSocketHandshakeDraft75::kServerHandshakeHeader[] =
    "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
const size_t WebSocketHandshakeDraft75::kServerHandshakeHeaderLength =
    sizeof(kServerHandshakeHeader) - 1;

const char WebSocketHandshakeDraft75::kUpgradeHeader[] =
    "Upgrade: WebSocket\r\n";
const size_t WebSocketHandshakeDraft75::kUpgradeHeaderLength =
    sizeof(kUpgradeHeader) - 1;

const char WebSocketHandshakeDraft75::kConnectionHeader[] =
    "Connection: Upgrade\r\n";
const size_t WebSocketHandshakeDraft75::kConnectionHeaderLength =
    sizeof(kConnectionHeader) - 1;

WebSocketHandshakeDraft75::WebSocketHandshakeDraft75(
    const GURL& url,
    const std::string& origin,
    const std::string& location,
    const std::string& protocol)
    : WebSocketHandshake(url, origin, location, protocol) {
}

WebSocketHandshakeDraft75::~WebSocketHandshakeDraft75() {
}

std::string WebSocketHandshakeDraft75::CreateClientHandshakeMessage() {
  std::string msg;
  msg = "GET ";
  msg += GetResourceName();
  msg += " HTTP/1.1\r\n";
  msg += kUpgradeHeader;
  msg += kConnectionHeader;
  msg += "Host: ";
  msg += GetHostFieldValue();
  msg += "\r\n";
  msg += "Origin: ";
  msg += GetOriginFieldValue();
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

int WebSocketHandshakeDraft75::ReadServerHandshake(
    const char* data, size_t len) {
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
  const char* end = data + len;

  if (mode_ == MODE_NORMAL) {
    size_t header_size = end - p;
    if (header_size < kUpgradeHeaderLength)
      return -1;
    if (memcmp(p, kUpgradeHeader, kUpgradeHeaderLength)) {
      mode_ = MODE_FAILED;
      DVLOG(1) << "Bad Upgrade Header " << std::string(p, kUpgradeHeaderLength);
      return p - data;
    }
    p += kUpgradeHeaderLength;
    header_size = end - p;
    if (header_size < kConnectionHeaderLength)
      return -1;
    if (memcmp(p, kConnectionHeader, kConnectionHeaderLength)) {
      mode_ = MODE_FAILED;
      DVLOG(1) << "Bad Connection Header "
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
    DVLOG(1) << "Process Headers failed: " << std::string(data, eoh);
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

bool WebSocketHandshakeDraft75::ProcessHeaders(
    const HttpResponseHeaders& headers) {
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

bool WebSocketHandshakeDraft75::CheckResponseHeaders() const {
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

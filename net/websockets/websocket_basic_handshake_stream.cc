// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_handshake_stream.h"

#include <algorithm>
#include <iterator>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_handler.h"
#include "net/websockets/websocket_stream.h"

namespace net {
namespace {

std::string GenerateHandshakeChallenge() {
  std::string raw_challenge(websockets::kRawChallengeLength, '\0');
  crypto::RandBytes(string_as_array(&raw_challenge), raw_challenge.length());
  std::string encoded_challenge;
  base::Base64Encode(raw_challenge, &encoded_challenge);
  return encoded_challenge;
}

void AddVectorHeaderIfNonEmpty(const char* name,
                               const std::vector<std::string>& value,
                               HttpRequestHeaders* headers) {
  if (value.empty())
    return;
  headers->SetHeader(name, JoinString(value, ", "));
}

// If |case_sensitive| is false, then |value| must be in lower-case.
bool ValidateSingleTokenHeader(
    const scoped_refptr<HttpResponseHeaders>& headers,
    const base::StringPiece& name,
    const std::string& value,
    bool case_sensitive) {
  void* state = NULL;
  std::string token;
  int tokens = 0;
  bool has_value = false;
  while (headers->EnumerateHeader(&state, name, &token)) {
    if (++tokens > 1)
      return false;
    has_value = case_sensitive ? value == token
                               : LowerCaseEqualsASCII(token, value.c_str());
  }
  return has_value;
}

bool ValidateSubProtocol(
    const scoped_refptr<HttpResponseHeaders>& headers,
    const std::vector<std::string>& requested_sub_protocols,
    std::string* sub_protocol) {
  void* state = NULL;
  std::string token;
  base::hash_set<std::string> requested_set(requested_sub_protocols.begin(),
                                            requested_sub_protocols.end());
  int accepted = 0;
  while (headers->EnumerateHeader(
      &state, websockets::kSecWebSocketProtocol, &token)) {
    if (requested_set.count(token) == 0)
      return false;

    *sub_protocol = token;
    // The server is only allowed to accept one protocol.
    if (++accepted > 1)
      return false;
  }
  // If the browser requested > 0 protocols, the server is required to accept
  // one.
  return requested_set.empty() || accepted == 1;
}

bool ValidateExtensions(const scoped_refptr<HttpResponseHeaders>& headers,
                        const std::vector<std::string>& requested_extensions,
                        std::string* extensions) {
  void* state = NULL;
  std::string token;
  while (headers->EnumerateHeader(
      &state, websockets::kSecWebSocketExtensions, &token)) {
    // TODO(ricea): Accept permessage-deflate with valid parameters.
    return false;
  }
  return true;
}

}  // namespace

WebSocketBasicHandshakeStream::WebSocketBasicHandshakeStream(
    scoped_ptr<ClientSocketHandle> connection,
    bool using_proxy,
    std::vector<std::string> requested_sub_protocols,
    std::vector<std::string> requested_extensions)
    : state_(connection.release(), using_proxy),
      http_response_info_(NULL),
      requested_sub_protocols_(requested_sub_protocols),
      requested_extensions_(requested_extensions) {}

WebSocketBasicHandshakeStream::~WebSocketBasicHandshakeStream() {}

int WebSocketBasicHandshakeStream::InitializeStream(
    const HttpRequestInfo* request_info,
    RequestPriority priority,
    const BoundNetLog& net_log,
    const CompletionCallback& callback) {
  state_.Initialize(request_info, priority, net_log, callback);
  return OK;
}

int WebSocketBasicHandshakeStream::SendRequest(
    const HttpRequestHeaders& headers,
    HttpResponseInfo* response,
    const CompletionCallback& callback) {
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketKey));
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketProtocol));
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketExtensions));
  DCHECK(headers.HasHeader(HttpRequestHeaders::kOrigin));
  DCHECK(headers.HasHeader(websockets::kUpgrade));
  DCHECK(headers.HasHeader(HttpRequestHeaders::kConnection));
  DCHECK(headers.HasHeader(websockets::kSecWebSocketVersion));
  DCHECK(parser());

  http_response_info_ = response;

  // Create a copy of the headers object, so that we can add the
  // Sec-WebSockey-Key header.
  HttpRequestHeaders enriched_headers;
  enriched_headers.CopyFrom(headers);
  std::string handshake_challenge;
  if (handshake_challenge_for_testing_) {
    handshake_challenge = *handshake_challenge_for_testing_;
    handshake_challenge_for_testing_.reset();
  } else {
    handshake_challenge = GenerateHandshakeChallenge();
  }
  enriched_headers.SetHeader(websockets::kSecWebSocketKey, handshake_challenge);

  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketProtocol,
                            requested_sub_protocols_,
                            &enriched_headers);
  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketExtensions,
                            requested_extensions_,
                            &enriched_headers);

  ComputeSecWebSocketAccept(handshake_challenge,
                            &handshake_challenge_response_);

  return parser()->SendRequest(
      state_.GenerateRequestLine(), enriched_headers, response, callback);
}

int WebSocketBasicHandshakeStream::ReadResponseHeaders(
    const CompletionCallback& callback) {
  // HttpStreamParser uses a weak pointer when reading from the
  // socket, so it won't be called back after being destroyed. The
  // HttpStreamParser is owned by HttpBasicState which is owned by this object,
  // so this use of base::Unretained() is safe.
  int rv = parser()->ReadResponseHeaders(
      base::Bind(&WebSocketBasicHandshakeStream::ReadResponseHeadersCallback,
                 base::Unretained(this),
                 callback));
  return rv == OK ? ValidateResponse() : rv;
}

const HttpResponseInfo* WebSocketBasicHandshakeStream::GetResponseInfo() const {
  return parser()->GetResponseInfo();
}

int WebSocketBasicHandshakeStream::ReadResponseBody(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  return parser()->ReadResponseBody(buf, buf_len, callback);
}

void WebSocketBasicHandshakeStream::Close(bool not_reusable) {
  // This class ignores the value of |not_reusable| and never lets the socket be
  // re-used.
  if (parser())
    parser()->Close(true);
}

bool WebSocketBasicHandshakeStream::IsResponseBodyComplete() const {
  return parser()->IsResponseBodyComplete();
}

bool WebSocketBasicHandshakeStream::CanFindEndOfResponse() const {
  return parser() && parser()->CanFindEndOfResponse();
}

bool WebSocketBasicHandshakeStream::IsConnectionReused() const {
  return parser()->IsConnectionReused();
}

void WebSocketBasicHandshakeStream::SetConnectionReused() {
  parser()->SetConnectionReused();
}

bool WebSocketBasicHandshakeStream::IsConnectionReusable() const {
  return false;
}

int64 WebSocketBasicHandshakeStream::GetTotalReceivedBytes() const {
  return 0;
}

bool WebSocketBasicHandshakeStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return state_.connection()->GetLoadTimingInfo(IsConnectionReused(),
                                                load_timing_info);
}

void WebSocketBasicHandshakeStream::GetSSLInfo(SSLInfo* ssl_info) {
  parser()->GetSSLInfo(ssl_info);
}

void WebSocketBasicHandshakeStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  parser()->GetSSLCertRequestInfo(cert_request_info);
}

bool WebSocketBasicHandshakeStream::IsSpdyHttpStream() const { return false; }

void WebSocketBasicHandshakeStream::Drain(HttpNetworkSession* session) {
  HttpResponseBodyDrainer* drainer = new HttpResponseBodyDrainer(this);
  drainer->Start(session);
  // |drainer| will delete itself.
}

void WebSocketBasicHandshakeStream::SetPriority(RequestPriority priority) {
  // TODO(ricea): See TODO comment in HttpBasicStream::SetPriority(). If it is
  // gone, then copy whatever has happened there over here.
}

scoped_ptr<WebSocketStream> WebSocketBasicHandshakeStream::Upgrade() {
  // TODO(ricea): Add deflate support.

  // The HttpStreamParser object has a pointer to our ClientSocketHandle. Make
  // sure it does not touch it again before it is destroyed.
  state_.DeleteParser();
  return scoped_ptr<WebSocketStream>(
      new WebSocketBasicStream(state_.ReleaseConnection(),
                               state_.read_buf(),
                               sub_protocol_,
                               extensions_));
}

void WebSocketBasicHandshakeStream::SetWebSocketKeyForTesting(
    const std::string& key) {
  handshake_challenge_for_testing_.reset(new std::string(key));
}

void WebSocketBasicHandshakeStream::ReadResponseHeadersCallback(
    const CompletionCallback& callback,
    int result) {
  if (result == OK)
    result = ValidateResponse();
  callback.Run(result);
}

int WebSocketBasicHandshakeStream::ValidateResponse() {
  DCHECK(http_response_info_);
  const scoped_refptr<HttpResponseHeaders>& headers =
      http_response_info_->headers;

  switch (headers->response_code()) {
    case HTTP_SWITCHING_PROTOCOLS:
      return ValidateUpgradeResponse(headers);

    // We need to pass these through for authentication to work.
    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return OK;

    // Other status codes are potentially risky (see the warnings in the
    // WHATWG WebSocket API spec) and so are dropped by default.
    default:
      return ERR_INVALID_RESPONSE;
  }
}

int WebSocketBasicHandshakeStream::ValidateUpgradeResponse(
    const scoped_refptr<HttpResponseHeaders>& headers) {
  if (ValidateSingleTokenHeader(headers,
                                websockets::kUpgrade,
                                websockets::kWebSocketLowercase,
                                false) &&
      ValidateSingleTokenHeader(headers,
                                websockets::kSecWebSocketAccept,
                                handshake_challenge_response_,
                                true) &&
      headers->HasHeaderValue(HttpRequestHeaders::kConnection,
                              websockets::kUpgrade) &&
      ValidateSubProtocol(headers, requested_sub_protocols_, &sub_protocol_) &&
      ValidateExtensions(headers, requested_extensions_, &extensions_)) {
    return OK;
  }
  return ERR_INVALID_RESPONSE;
}

}  // namespace net

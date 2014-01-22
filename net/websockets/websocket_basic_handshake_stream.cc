// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_handshake_stream.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "crypto/random.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_extension_parser.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_handler.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_stream.h"

namespace net {
namespace {

enum GetHeaderResult {
  GET_HEADER_OK,
  GET_HEADER_MISSING,
  GET_HEADER_MULTIPLE,
};

std::string MissingHeaderMessage(const std::string& header_name) {
  return std::string("'") + header_name + "' header is missing";
}

std::string MultipleHeaderValuesMessage(const std::string& header_name) {
  return
      std::string("'") +
      header_name +
      "' header must not appear more than once in a response";
}

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

GetHeaderResult GetSingleHeaderValue(const HttpResponseHeaders* headers,
                                     const base::StringPiece& name,
                                     std::string* value) {
  void* state = NULL;
  size_t num_values = 0;
  std::string temp_value;
  while (headers->EnumerateHeader(&state, name, &temp_value)) {
    if (++num_values > 1)
      return GET_HEADER_MULTIPLE;
    *value = temp_value;
  }
  return num_values > 0 ? GET_HEADER_OK : GET_HEADER_MISSING;
}

bool ValidateHeaderHasSingleValue(GetHeaderResult result,
                                  const std::string& header_name,
                                  std::string* failure_message) {
  if (result == GET_HEADER_MISSING) {
    *failure_message = MissingHeaderMessage(header_name);
    return false;
  }
  if (result == GET_HEADER_MULTIPLE) {
    *failure_message = MultipleHeaderValuesMessage(header_name);
    return false;
  }
  DCHECK_EQ(result, GET_HEADER_OK);
  return true;
}

bool ValidateUpgrade(const HttpResponseHeaders* headers,
                     std::string* failure_message) {
  std::string value;
  GetHeaderResult result =
      GetSingleHeaderValue(headers, websockets::kUpgrade, &value);
  if (!ValidateHeaderHasSingleValue(result,
                                    websockets::kUpgrade,
                                    failure_message)) {
    return false;
  }

  if (!LowerCaseEqualsASCII(value, websockets::kWebSocketLowercase)) {
    *failure_message =
        "'Upgrade' header value is not 'WebSocket': " + value;
    return false;
  }
  return true;
}

bool ValidateSecWebSocketAccept(const HttpResponseHeaders* headers,
                                const std::string& expected,
                                std::string* failure_message) {
  std::string actual;
  GetHeaderResult result =
      GetSingleHeaderValue(headers, websockets::kSecWebSocketAccept, &actual);
  if (!ValidateHeaderHasSingleValue(result,
                                    websockets::kSecWebSocketAccept,
                                    failure_message)) {
    return false;
  }

  if (expected != actual) {
    *failure_message = "Incorrect 'Sec-WebSocket-Accept' header value";
    return false;
  }
  return true;
}

bool ValidateConnection(const HttpResponseHeaders* headers,
                        std::string* failure_message) {
  // Connection header is permitted to contain other tokens.
  if (!headers->HasHeader(HttpRequestHeaders::kConnection)) {
    *failure_message = MissingHeaderMessage(HttpRequestHeaders::kConnection);
    return false;
  }
  if (!headers->HasHeaderValue(HttpRequestHeaders::kConnection,
                               websockets::kUpgrade)) {
    *failure_message = "'Connection' header value must contain 'Upgrade'";
    return false;
  }
  return true;
}

bool ValidateSubProtocol(
    const HttpResponseHeaders* headers,
    const std::vector<std::string>& requested_sub_protocols,
    std::string* sub_protocol,
    std::string* failure_message) {
  void* state = NULL;
  std::string value;
  base::hash_set<std::string> requested_set(requested_sub_protocols.begin(),
                                            requested_sub_protocols.end());
  int count = 0;
  bool has_multiple_protocols = false;
  bool has_invalid_protocol = false;

  while (!has_invalid_protocol || !has_multiple_protocols) {
    std::string temp_value;
    if (!headers->EnumerateHeader(
            &state, websockets::kSecWebSocketProtocol, &temp_value))
      break;
    value = temp_value;
    if (requested_set.count(value) == 0)
      has_invalid_protocol = true;
    if (++count > 1)
      has_multiple_protocols = true;
  }

  if (has_multiple_protocols) {
    *failure_message =
        MultipleHeaderValuesMessage(websockets::kSecWebSocketProtocol);
    return false;
  } else if (count > 0 && requested_sub_protocols.size() == 0) {
    *failure_message =
        std::string("Response must not include 'Sec-WebSocket-Protocol' "
                    "header if not present in request: ")
        + value;
    return false;
  } else if (has_invalid_protocol) {
    *failure_message =
        "'Sec-WebSocket-Protocol' header value '" +
        value +
        "' in response does not match any of sent values";
    return false;
  } else if (requested_sub_protocols.size() > 0 && count == 0) {
    *failure_message =
        "Sent non-empty 'Sec-WebSocket-Protocol' header "
        "but no response was received";
    return false;
  }
  *sub_protocol = value;
  return true;
}

bool ValidateExtensions(const HttpResponseHeaders* headers,
                        const std::vector<std::string>& requested_extensions,
                        std::string* extensions,
                        std::string* failure_message) {
  void* state = NULL;
  std::string value;
  while (headers->EnumerateHeader(
      &state, websockets::kSecWebSocketExtensions, &value)) {
    WebSocketExtensionParser parser;
    parser.Parse(value);
    if (parser.has_error()) {
      // TODO(yhirano) Set appropriate failure message.
      *failure_message =
          "'Sec-WebSocket-Extensions' header value is "
          "rejected by the parser: " +
          value;
      return false;
    }
    // TODO(ricea): Accept permessage-deflate with valid parameters.
    *failure_message =
        "Found an unsupported extension '" +
        parser.extension().name() +
        "' in 'Sec-WebSocket-Extensions' header";
    return false;
  }
  return true;
}

}  // namespace

WebSocketBasicHandshakeStream::WebSocketBasicHandshakeStream(
    scoped_ptr<ClientSocketHandle> connection,
    WebSocketStream::ConnectDelegate* connect_delegate,
    bool using_proxy,
    std::vector<std::string> requested_sub_protocols,
    std::vector<std::string> requested_extensions)
    : state_(connection.release(), using_proxy),
      connect_delegate_(connect_delegate),
      http_response_info_(NULL),
      requested_sub_protocols_(requested_sub_protocols),
      requested_extensions_(requested_extensions) {}

WebSocketBasicHandshakeStream::~WebSocketBasicHandshakeStream() {}

int WebSocketBasicHandshakeStream::InitializeStream(
    const HttpRequestInfo* request_info,
    RequestPriority priority,
    const BoundNetLog& net_log,
    const CompletionCallback& callback) {
  url_ = request_info->url;
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

  DCHECK(connect_delegate_);
  scoped_ptr<WebSocketHandshakeRequestInfo> request(
      new WebSocketHandshakeRequestInfo(url_, base::Time::Now()));
  request->headers.CopyFrom(enriched_headers);
  connect_delegate_->OnStartOpeningHandshake(request.Pass());

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
  if (rv == ERR_IO_PENDING)
    return rv;
  if (rv == OK)
    return ValidateResponse();
  OnFinishOpeningHandshake();
  return rv;
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

std::string WebSocketBasicHandshakeStream::GetFailureMessage() const {
  return failure_message_;
}

void WebSocketBasicHandshakeStream::ReadResponseHeadersCallback(
    const CompletionCallback& callback,
    int result) {
  if (result == OK)
    result = ValidateResponse();
  else
    OnFinishOpeningHandshake();
  callback.Run(result);
}

void WebSocketBasicHandshakeStream::OnFinishOpeningHandshake() {
  DCHECK(connect_delegate_);
  DCHECK(http_response_info_);
  scoped_refptr<HttpResponseHeaders> headers = http_response_info_->headers;
  scoped_ptr<WebSocketHandshakeResponseInfo> response(
      new WebSocketHandshakeResponseInfo(url_,
                                         headers->response_code(),
                                         headers->GetStatusText(),
                                         headers,
                                         http_response_info_->response_time));
  connect_delegate_->OnFinishOpeningHandshake(response.Pass());
}

int WebSocketBasicHandshakeStream::ValidateResponse() {
  DCHECK(http_response_info_);
  const scoped_refptr<HttpResponseHeaders>& headers =
      http_response_info_->headers;

  switch (headers->response_code()) {
    case HTTP_SWITCHING_PROTOCOLS:
      OnFinishOpeningHandshake();
      return ValidateUpgradeResponse(headers);

    // We need to pass these through for authentication to work.
    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return OK;

    // Other status codes are potentially risky (see the warnings in the
    // WHATWG WebSocket API spec) and so are dropped by default.
    default:
      failure_message_ = base::StringPrintf("Unexpected status code: %d",
                                            headers->response_code());
      OnFinishOpeningHandshake();
      return ERR_INVALID_RESPONSE;
  }
}

int WebSocketBasicHandshakeStream::ValidateUpgradeResponse(
    const scoped_refptr<HttpResponseHeaders>& headers) {
  if (ValidateUpgrade(headers.get(), &failure_message_) &&
      ValidateSecWebSocketAccept(headers.get(),
                                 handshake_challenge_response_,
                                 &failure_message_) &&
      ValidateConnection(headers.get(), &failure_message_) &&
      ValidateSubProtocol(headers.get(),
                          requested_sub_protocols_,
                          &sub_protocol_,
                          &failure_message_) &&
      ValidateExtensions(headers.get(),
                         requested_extensions_,
                         &extensions_,
                         &failure_message_)) {
    return OK;
  }
  failure_message_ = "Error during WebSocket handshake: " + failure_message_;
  return ERR_INVALID_RESPONSE;
}

}  // namespace net

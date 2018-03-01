// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_http2_handshake_stream.h"

#include <cstddef>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/spdy/chromium/spdy_http_utils.h"
#include "net/spdy/chromium/spdy_session.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_deflate_predictor_impl.h"
#include "net/websockets/websocket_deflate_stream.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_extension_parser.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_request_info.h"

namespace net {

// TODO(ricea): If more extensions are added, replace this with a more general
// mechanism.
struct WebSocketExtensionParams {
  bool deflate_enabled = false;
  WebSocketDeflateParameters deflate_parameters;
};

namespace {

void AddVectorHeaderIfNonEmpty(const char* name,
                               const std::vector<std::string>& value,
                               HttpRequestHeaders* headers) {
  if (value.empty())
    return;
  headers->SetHeader(name, base::JoinString(value, ", "));
}

std::string MultipleHeaderValuesMessage(const std::string& header_name) {
  return std::string("'") + header_name +
         "' header must not appear more than once in a response";
}

bool ValidateStatus(const HttpResponseHeaders* headers) {
  return headers->GetStatusLine() == "HTTP/1.1 200";
}

bool ValidateSubProtocol(
    const HttpResponseHeaders* headers,
    const std::vector<std::string>& requested_sub_protocols,
    std::string* sub_protocol,
    std::string* failure_message) {
  size_t iter = 0;
  std::string value;
  std::unordered_set<std::string> requested_set(requested_sub_protocols.begin(),
                                                requested_sub_protocols.end());
  int count = 0;
  bool has_multiple_protocols = false;
  bool has_invalid_protocol = false;

  while (!has_invalid_protocol || !has_multiple_protocols) {
    std::string temp_value;
    if (!headers->EnumerateHeader(
            &iter, websockets::kSecWebSocketProtocolLowercase, &temp_value))
      break;
    value = temp_value;
    if (requested_set.count(value) == 0)
      has_invalid_protocol = true;
    if (++count > 1)
      has_multiple_protocols = true;
  }

  if (has_multiple_protocols) {
    *failure_message =
        MultipleHeaderValuesMessage(websockets::kSecWebSocketProtocolLowercase);
    return false;
  } else if (count > 0 && requested_sub_protocols.size() == 0) {
    *failure_message = std::string(
                           "Response must not include 'Sec-WebSocket-Protocol' "
                           "header if not present in request: ") +
                       value;
    return false;
  } else if (has_invalid_protocol) {
    *failure_message = "'Sec-WebSocket-Protocol' header value '" + value +
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
                        std::string* accepted_extensions_descriptor,
                        std::string* failure_message,
                        WebSocketExtensionParams* params) {
  size_t iter = 0;
  std::string header_value;
  std::vector<std::string> header_values;
  // TODO(ricea): If adding support for additional extensions, generalise this
  // code.
  bool seen_permessage_deflate = false;
  while (headers->EnumerateHeader(
      &iter, websockets::kSecWebSocketExtensionsLowercase, &header_value)) {
    WebSocketExtensionParser parser;
    if (!parser.Parse(header_value)) {
      // TODO(yhirano) Set appropriate failure message.
      *failure_message =
          "'Sec-WebSocket-Extensions' header value is "
          "rejected by the parser: " +
          header_value;
      return false;
    }

    const std::vector<WebSocketExtension>& extensions = parser.extensions();
    for (const auto& extension : extensions) {
      if (extension.name() == "permessage-deflate") {
        if (seen_permessage_deflate) {
          *failure_message = "Received duplicate permessage-deflate response";
          return false;
        }
        seen_permessage_deflate = true;
        auto& deflate_parameters = params->deflate_parameters;
        if (!deflate_parameters.Initialize(extension, failure_message) ||
            !deflate_parameters.IsValidAsResponse(failure_message)) {
          *failure_message = "Error in permessage-deflate: " + *failure_message;
          return false;
        }
        // Note that we don't have to check the request-response compatibility
        // here because we send a request compatible with any valid responses.
        // TODO(yhirano): Place a DCHECK here.

        header_values.push_back(header_value);
      } else {
        *failure_message = "Found an unsupported extension '" +
                           extension.name() +
                           "' in 'Sec-WebSocket-Extensions' header";
        return false;
      }
    }
  }
  *accepted_extensions_descriptor = base::JoinString(header_values, ", ");
  params->deflate_enabled = seen_permessage_deflate;
  return true;
}

}  // namespace

WebSocketHttp2HandshakeStream::WebSocketHttp2HandshakeStream(
    base::WeakPtr<SpdySession> session,
    WebSocketStream::ConnectDelegate* connect_delegate,
    std::vector<std::string> requested_sub_protocols,
    std::vector<std::string> requested_extensions,
    WebSocketStreamRequest* request)
    : session_(session),
      connect_delegate_(connect_delegate),
      http_response_info_(nullptr),
      requested_sub_protocols_(requested_sub_protocols),
      requested_extensions_(requested_extensions),
      stream_request_(request),
      request_info_(nullptr),
      stream_closed_(false),
      stream_error_(OK),
      response_headers_complete_(false) {
  DCHECK(connect_delegate);
  DCHECK(request);
}

WebSocketHttp2HandshakeStream::~WebSocketHttp2HandshakeStream() {
  spdy_stream_request_.reset();
}

int WebSocketHttp2HandshakeStream::InitializeStream(
    const HttpRequestInfo* request_info,
    bool can_send_early,
    RequestPriority priority,
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback) {
  request_info_ = request_info;
  priority_ = priority;
  net_log_ = net_log;
  return OK;
}

int WebSocketHttp2HandshakeStream::SendRequest(
    const HttpRequestHeaders& headers,
    HttpResponseInfo* response,
    CompletionOnceCallback callback) {
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketKey));
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketProtocol));
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketExtensions));
  DCHECK(headers.HasHeader(HttpRequestHeaders::kOrigin));
  DCHECK(headers.HasHeader(websockets::kUpgrade));
  DCHECK(headers.HasHeader(HttpRequestHeaders::kConnection));
  DCHECK(headers.HasHeader(websockets::kSecWebSocketVersion));

  if (!session_) {
    OnFailure("Connection closed before sending request.");
    return ERR_CONNECTION_CLOSED;
  }

  http_response_info_ = response;

  IPEndPoint address;
  int result = session_->GetPeerAddress(&address);
  if (result != OK) {
    OnFailure("Error getting IP address.");
    return result;
  }
  http_response_info_->socket_address = HostPortPair::FromIPEndPoint(address);

  auto request = std::make_unique<WebSocketHandshakeRequestInfo>(
      request_info_->url, base::Time::Now());
  request->headers.CopyFrom(headers);

  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketExtensions,
                            requested_extensions_, &request->headers);
  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketProtocol,
                            requested_sub_protocols_, &request->headers);

  CreateSpdyHeadersFromHttpRequestForWebSocket(
      request_info_->url, request->headers, &http2_request_headers_);

  connect_delegate_->OnStartOpeningHandshake(std::move(request));

  callback_ = std::move(callback);
  spdy_stream_request_ = std::make_unique<SpdyStreamRequest>();
  // TODO(https://crbug.com/656607): Add proper annotation here.
  int rv = spdy_stream_request_->StartRequest(
      SPDY_BIDIRECTIONAL_STREAM, session_, request_info_->url, priority_,
      request_info_->socket_tag, net_log_,
      base::BindOnce(&WebSocketHttp2HandshakeStream::StartRequestCallback,
                     base::Unretained(this)),
      NO_TRAFFIC_ANNOTATION_BUG_656607);
  if (rv == OK) {
    StartRequestCallback(rv);
    return ERR_IO_PENDING;
  }
  return rv;
}

int WebSocketHttp2HandshakeStream::ReadResponseHeaders(
    CompletionOnceCallback callback) {
  if (stream_closed_)
    return stream_error_;

  if (response_headers_complete_)
    return ValidateResponse();

  callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int WebSocketHttp2HandshakeStream::ReadResponseBody(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback) {
  // Callers should instead call Upgrade() to get a WebSocketStream
  // and call ReadFrames() on that.
  NOTREACHED();
  return OK;
}

void WebSocketHttp2HandshakeStream::Close(bool not_reusable) {
  spdy_stream_request_.reset();
  if (stream_) {
    stream_ = nullptr;
    stream_closed_ = true;
    stream_error_ = ERR_CONNECTION_CLOSED;
  }
  stream_adapter_.reset();
}

bool WebSocketHttp2HandshakeStream::IsResponseBodyComplete() const {
  return false;
}

bool WebSocketHttp2HandshakeStream::IsConnectionReused() const {
  return true;
}

void WebSocketHttp2HandshakeStream::SetConnectionReused() {}

bool WebSocketHttp2HandshakeStream::CanReuseConnection() const {
  return false;
}

int64_t WebSocketHttp2HandshakeStream::GetTotalReceivedBytes() const {
  return stream_ ? stream_->raw_received_bytes() : 0;
}

int64_t WebSocketHttp2HandshakeStream::GetTotalSentBytes() const {
  return stream_ ? stream_->raw_sent_bytes() : 0;
}

bool WebSocketHttp2HandshakeStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  return false;
}

bool WebSocketHttp2HandshakeStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return stream_ && stream_->GetLoadTimingInfo(load_timing_info);
}

void WebSocketHttp2HandshakeStream::GetSSLInfo(SSLInfo* ssl_info) {
  if (stream_)
    stream_->GetSSLInfo(ssl_info);
}

void WebSocketHttp2HandshakeStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  // A multiplexed stream cannot request client certificates. Client
  // authentication may only occur during the initial SSL handshake.
  NOTREACHED();
}

bool WebSocketHttp2HandshakeStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return session_ && session_->GetRemoteEndpoint(endpoint);
}

void WebSocketHttp2HandshakeStream::PopulateNetErrorDetails(
    NetErrorDetails* /*details*/) {
  return;
}

Error WebSocketHttp2HandshakeStream::GetTokenBindingSignature(
    crypto::ECPrivateKey* key,
    TokenBindingType tb_type,
    std::vector<uint8_t>* out) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void WebSocketHttp2HandshakeStream::Drain(HttpNetworkSession* session) {
  Close(true /* not_reusable */);
}

void WebSocketHttp2HandshakeStream::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (stream_)
    stream_->set_priority(priority_);
}

HttpStream* WebSocketHttp2HandshakeStream::RenewStreamForAuth() {
  // Renewing the stream is not supported.
  return nullptr;
}

std::unique_ptr<WebSocketStream> WebSocketHttp2HandshakeStream::Upgrade() {
  DCHECK(extension_params_.get());

  stream_adapter_->DetachDelegate();
  std::unique_ptr<WebSocketStream> basic_stream =
      std::make_unique<WebSocketBasicStream>(
          std::move(stream_adapter_), nullptr, sub_protocol_, extensions_);

  if (!extension_params_->deflate_enabled)
    return basic_stream;

  UMA_HISTOGRAM_ENUMERATION(
      "Net.WebSocket.DeflateMode",
      extension_params_->deflate_parameters.client_context_take_over_mode(),
      WebSocketDeflater::NUM_CONTEXT_TAKEOVER_MODE_TYPES);

  return std::make_unique<WebSocketDeflateStream>(
      std::move(basic_stream), extension_params_->deflate_parameters,
      std::make_unique<WebSocketDeflatePredictorImpl>());
}

void WebSocketHttp2HandshakeStream::OnHeadersSent() {
  base::ResetAndReturn(&callback_).Run(OK);
}

void WebSocketHttp2HandshakeStream::OnHeadersReceived(
    const SpdyHeaderBlock& response_headers) {
  DCHECK(!response_headers_complete_);
  DCHECK(http_response_info_);

  response_headers_complete_ = true;

  const bool headers_valid =
      SpdyHeadersToHttpResponse(response_headers, http_response_info_);
  DCHECK(headers_valid);

  http_response_info_->response_time = stream_->response_time();
  // Do not store SSLInfo in the response here, HttpNetworkTransaction will take
  // care of that part.
  http_response_info_->was_alpn_negotiated = true;
  http_response_info_->request_time = stream_->GetRequestTime();
  http_response_info_->connection_info =
      HttpResponseInfo::CONNECTION_INFO_HTTP2;
  http_response_info_->alpn_negotiated_protocol =
      HttpResponseInfo::ConnectionInfoToString(
          http_response_info_->connection_info);
  http_response_info_->vary_data.Init(*request_info_,
                                      *http_response_info_->headers.get());

  if (callback_)
    base::ResetAndReturn(&callback_).Run(ValidateResponse());
}

void WebSocketHttp2HandshakeStream::OnClose(int status) {
  DCHECK(stream_adapter_);
  DCHECK_GT(ERR_IO_PENDING, status);

  stream_closed_ = true;
  stream_error_ = status;
  stream_ = nullptr;

  stream_adapter_.reset();

  OnFailure(std::string("Stream closed with error: ") + ErrorToString(status));

  if (callback_)
    base::ResetAndReturn(&callback_).Run(status);
}

void WebSocketHttp2HandshakeStream::StartRequestCallback(int rv) {
  DCHECK(callback_);
  if (rv != OK) {
    spdy_stream_request_.reset();
    base::ResetAndReturn(&callback_).Run(rv);
    return;
  }
  stream_ = spdy_stream_request_->ReleaseStream();
  spdy_stream_request_.reset();
  stream_adapter_ =
      std::make_unique<WebSocketSpdyStreamAdapter>(stream_, this, net_log_);
  rv = stream_->SendRequestHeaders(std::move(http2_request_headers_),
                                   MORE_DATA_TO_SEND);
  // SendRequestHeaders() always returns asynchronously,
  // and instead of taking a callback, it calls OnHeadersSent().
  DCHECK_EQ(ERR_IO_PENDING, rv);
}

int WebSocketHttp2HandshakeStream::ValidateResponse() {
  DCHECK(http_response_info_);
  const HttpResponseHeaders* headers = http_response_info_->headers.get();
  const int response_code = headers->response_code();
  switch (response_code) {
    case HTTP_OK:
      OnFinishOpeningHandshake();
      return ValidateUpgradeResponse(headers);

    // We need to pass these through for authentication to work.
    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return OK;

    // Other status codes are potentially risky (see the warnings in the
    // WHATWG WebSocket API spec) and so are dropped by default.
    default:
      OnFailure(base::StringPrintf(
          "Error during WebSocket handshake: Unexpected response code: %d",
          headers->response_code()));
      OnFinishOpeningHandshake();
      return ERR_INVALID_RESPONSE;
  }
}

int WebSocketHttp2HandshakeStream::ValidateUpgradeResponse(
    const HttpResponseHeaders* headers) {
  extension_params_ = std::make_unique<WebSocketExtensionParams>();
  std::string failure_message;
  if (ValidateStatus(headers) &&
      ValidateSubProtocol(headers, requested_sub_protocols_, &sub_protocol_,
                          &failure_message) &&
      ValidateExtensions(headers, &extensions_, &failure_message,
                         extension_params_.get())) {
    return OK;
  }
  OnFailure("Error during WebSocket handshake: " + failure_message);
  return ERR_INVALID_RESPONSE;
}

void WebSocketHttp2HandshakeStream::OnFinishOpeningHandshake() {
  DCHECK(http_response_info_);
  WebSocketDispatchOnFinishOpeningHandshake(
      connect_delegate_, request_info_->url, http_response_info_->headers,
      http_response_info_->response_time);
}

void WebSocketHttp2HandshakeStream::OnFailure(const std::string& message) {
  stream_request_->OnFailure(message);
}

}  // namespace net

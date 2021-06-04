// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/tools/quic_simple_server_stream.h"

#include <list>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quic/core/http/quic_spdy_stream.h"
#include "quic/core/http/spdy_utils.h"
#include "quic/core/http/web_transport_http3.h"
#include "quic/core/quic_utils.h"
#include "quic/platform/api/quic_bug_tracker.h"
#include "quic/platform/api/quic_flags.h"
#include "quic/platform/api/quic_logging.h"
#include "quic/platform/api/quic_map_util.h"
#include "quic/tools/quic_simple_server_session.h"
#include "spdy/core/spdy_protocol.h"

using spdy::Http2HeaderBlock;

namespace quic {

QuicSimpleServerStream::QuicSimpleServerStream(
    QuicStreamId id,
    QuicSpdySession* session,
    StreamType type,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSpdyServerStreamBase(id, session, type),
      content_length_(-1),
      generate_bytes_length_(0),
      quic_simple_server_backend_(quic_simple_server_backend) {
  QUICHE_DCHECK(quic_simple_server_backend_);
}

QuicSimpleServerStream::QuicSimpleServerStream(
    PendingStream* pending,
    QuicSpdySession* session,
    StreamType type,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicSpdyServerStreamBase(pending, session, type),
      content_length_(-1),
      generate_bytes_length_(0),
      quic_simple_server_backend_(quic_simple_server_backend) {
  QUICHE_DCHECK(quic_simple_server_backend_);
}

QuicSimpleServerStream::~QuicSimpleServerStream() {
  quic_simple_server_backend_->CloseBackendResponseStream(this);
}

void QuicSimpleServerStream::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len, header_list);
  if (!SpdyUtils::CopyAndValidateHeaders(header_list, &content_length_,
                                         &request_headers_)) {
    QUIC_DVLOG(1) << "Invalid headers";
    SendErrorResponse();
  }
  ConsumeHeaderList();
  if (!fin && !response_sent_) {
    // CONNECT and other CONNECT-like methods (such as CONNECT-UDP) require
    // sending the response right after parsing the headers even though the FIN
    // bit has not been received on the request stream.
    auto it = request_headers_.find(":method");
    if (it != request_headers_.end() &&
        absl::StartsWith(it->second, "CONNECT")) {
      SendResponse();
    }
  }
}

void QuicSimpleServerStream::OnTrailingHeadersComplete(
    bool /*fin*/,
    size_t /*frame_len*/,
    const QuicHeaderList& /*header_list*/) {
  QUIC_BUG(quic_bug_10962_1) << "Server does not support receiving Trailers.";
  SendErrorResponse();
}

void QuicSimpleServerStream::OnBodyAvailable() {
  while (HasBytesToRead()) {
    struct iovec iov;
    if (GetReadableRegions(&iov, 1) == 0) {
      // No more data to read.
      break;
    }
    QUIC_DVLOG(1) << "Stream " << id() << " processed " << iov.iov_len
                  << " bytes.";
    body_.append(static_cast<char*>(iov.iov_base), iov.iov_len);

    if (content_length_ >= 0 &&
        body_.size() > static_cast<uint64_t>(content_length_)) {
      QUIC_DVLOG(1) << "Body size (" << body_.size() << ") > content length ("
                    << content_length_ << ").";
      SendErrorResponse();
      return;
    }
    MarkConsumed(iov.iov_len);
  }
  if (!sequencer()->IsClosed()) {
    sequencer()->SetUnblocked();
    return;
  }

  // If the sequencer is closed, then all the body, including the fin, has been
  // consumed.
  OnFinRead();

  if (write_side_closed() || fin_buffered()) {
    return;
  }

  SendResponse();
}

void QuicSimpleServerStream::PushResponse(
    Http2HeaderBlock push_request_headers) {
  if (QuicUtils::IsClientInitiatedStreamId(session()->transport_version(),
                                           id())) {
    QUIC_BUG(quic_bug_10962_2)
        << "Client initiated stream shouldn't be used as promised stream.";
    return;
  }
  // Change the stream state to emulate a client request.
  request_headers_ = std::move(push_request_headers);
  content_length_ = 0;
  QUIC_DVLOG(1) << "Stream " << id()
                << " ready to receive server push response.";
  QUICHE_DCHECK(reading_stopped());

  // Directly send response based on the emulated request_headers_.
  SendResponse();
}

void QuicSimpleServerStream::SendResponse() {
  if (request_headers_.empty()) {
    QUIC_DVLOG(1) << "Request headers empty.";
    SendErrorResponse();
    return;
  }

  if (content_length_ > 0 &&
      static_cast<uint64_t>(content_length_) != body_.size()) {
    QUIC_DVLOG(1) << "Content length (" << content_length_ << ") != body size ("
                  << body_.size() << ").";
    SendErrorResponse();
    return;
  }

  if (!QuicContainsKey(request_headers_, ":authority")) {
    QUIC_DVLOG(1) << "Request headers do not contain :authority.";
    SendErrorResponse();
    return;
  }

  if (!QuicContainsKey(request_headers_, ":path")) {
    // CONNECT and other CONNECT-like methods (such as CONNECT-UDP) do not all
    // require :path to be present.
    auto it = request_headers_.find(":method");
    if (it == request_headers_.end() ||
        !absl::StartsWith(it->second, "CONNECT")) {
      QUIC_DVLOG(1) << "Request headers do not contain :path.";
      SendErrorResponse();
      return;
    }
  }

  if (quic_simple_server_backend_ == nullptr) {
    QUIC_DVLOG(1) << "Backend is missing.";
    SendErrorResponse();
    return;
  }

  if (web_transport() != nullptr) {
    QuicSimpleServerBackend::WebTransportResponse response =
        quic_simple_server_backend_->ProcessWebTransportRequest(
            request_headers_, web_transport());
    if (response.response_headers[":status"] == "200") {
      WriteHeaders(std::move(response.response_headers), false, nullptr);
      if (response.visitor != nullptr) {
        web_transport()->SetVisitor(std::move(response.visitor));
      }
      web_transport()->HeadersReceived(request_headers_);
    } else {
      WriteHeaders(std::move(response.response_headers), true, nullptr);
    }
    return;
  }

  // Fetch the response from the backend interface and wait for callback once
  // response is ready
  quic_simple_server_backend_->FetchResponseFromBackend(request_headers_, body_,
                                                        this);
}

QuicConnectionId QuicSimpleServerStream::connection_id() const {
  return spdy_session()->connection_id();
}

QuicStreamId QuicSimpleServerStream::stream_id() const {
  return id();
}

std::string QuicSimpleServerStream::peer_host() const {
  return spdy_session()->peer_address().host().ToString();
}

void QuicSimpleServerStream::OnResponseBackendComplete(
    const QuicBackendResponse* response) {
  if (response == nullptr) {
    QUIC_DVLOG(1) << "Response not found in cache.";
    SendNotFoundResponse();
    return;
  }

  // Send Early Hints first.
  for (const auto& headers : response->early_hints()) {
    QUIC_DVLOG(1) << "Stream " << id() << " sending an Early Hints response: "
                  << headers.DebugString();
    WriteHeaders(headers.Clone(), false, nullptr);
  }

  if (response->response_type() == QuicBackendResponse::CLOSE_CONNECTION) {
    QUIC_DVLOG(1) << "Special response: closing connection.";
    OnUnrecoverableError(QUIC_NO_ERROR, "Toy server forcing close");
    return;
  }

  if (response->response_type() == QuicBackendResponse::IGNORE_REQUEST) {
    QUIC_DVLOG(1) << "Special response: ignoring request.";
    return;
  }

  if (response->response_type() == QuicBackendResponse::BACKEND_ERR_RESPONSE) {
    QUIC_DVLOG(1) << "Quic Proxy: Backend connection error.";
    /*502 Bad Gateway
      The server was acting as a gateway or proxy and received an
      invalid response from the upstream server.*/
    SendErrorResponse(502);
    return;
  }

  // Examing response status, if it was not pure integer as typical h2
  // response status, send error response. Notice that
  // QuicHttpResponseCache push urls are strictly authority + path only,
  // scheme is not included (see |QuicHttpResponseCache::GetKey()|).
  std::string request_url = request_headers_[":authority"].as_string() +
                            request_headers_[":path"].as_string();
  int response_code;
  const Http2HeaderBlock& response_headers = response->headers();
  if (!ParseHeaderStatusCode(response_headers, &response_code)) {
    auto status = response_headers.find(":status");
    if (status == response_headers.end()) {
      QUIC_LOG(WARNING)
          << ":status not present in response from cache for request "
          << request_url;
    } else {
      QUIC_LOG(WARNING) << "Illegal (non-integer) response :status from cache: "
                        << status->second << " for request " << request_url;
    }
    SendErrorResponse();
    return;
  }

  if (QuicUtils::IsServerInitiatedStreamId(session()->transport_version(),
                                           id())) {
    // A server initiated stream is only used for a server push response,
    // and only 200 and 30X response codes are supported for server push.
    // This behavior mirrors the HTTP/2 implementation.
    bool is_redirection = response_code / 100 == 3;
    if (response_code != 200 && !is_redirection) {
      QUIC_LOG(WARNING) << "Response to server push request " << request_url
                        << " result in response code " << response_code;
      Reset(QUIC_STREAM_CANCELLED);
      return;
    }
  }

  if (response->response_type() == QuicBackendResponse::INCOMPLETE_RESPONSE) {
    QUIC_DVLOG(1)
        << "Stream " << id()
        << " sending an incomplete response, i.e. no trailer, no fin.";
    SendIncompleteResponse(response->headers().Clone(), response->body());
    return;
  }

  if (response->response_type() == QuicBackendResponse::GENERATE_BYTES) {
    QUIC_DVLOG(1) << "Stream " << id() << " sending a generate bytes response.";
    std::string path = request_headers_[":path"].as_string().substr(1);
    if (!absl::SimpleAtoi(path, &generate_bytes_length_)) {
      QUIC_LOG(ERROR) << "Path is not a number.";
      SendNotFoundResponse();
      return;
    }
    Http2HeaderBlock headers = response->headers().Clone();
    headers["content-length"] = absl::StrCat(generate_bytes_length_);

    WriteHeaders(std::move(headers), false, nullptr);
    QUICHE_DCHECK(!response_sent_);
    response_sent_ = true;

    WriteGeneratedBytes();

    return;
  }

  QUIC_DVLOG(1) << "Stream " << id() << " sending response.";
  SendHeadersAndBodyAndTrailers(response->headers().Clone(), response->body(),
                                response->trailers().Clone());
}

void QuicSimpleServerStream::OnCanWrite() {
  QuicSpdyStream::OnCanWrite();
  WriteGeneratedBytes();
}

void QuicSimpleServerStream::WriteGeneratedBytes() {
  static size_t kChunkSize = 1024;
  while (!HasBufferedData() && generate_bytes_length_ > 0) {
    size_t len = std::min<size_t>(kChunkSize, generate_bytes_length_);
    std::string data(len, 'a');
    generate_bytes_length_ -= len;
    bool fin = generate_bytes_length_ == 0;
    WriteOrBufferBody(data, fin);
  }
}

void QuicSimpleServerStream::SendNotFoundResponse() {
  QUIC_DVLOG(1) << "Stream " << id() << " sending not found response.";
  Http2HeaderBlock headers;
  headers[":status"] = "404";
  headers["content-length"] = absl::StrCat(strlen(kNotFoundResponseBody));
  SendHeadersAndBody(std::move(headers), kNotFoundResponseBody);
}

void QuicSimpleServerStream::SendErrorResponse() {
  SendErrorResponse(0);
}

void QuicSimpleServerStream::SendErrorResponse(int resp_code) {
  QUIC_DVLOG(1) << "Stream " << id() << " sending error response.";
  Http2HeaderBlock headers;
  if (resp_code <= 0) {
    headers[":status"] = "500";
  } else {
    headers[":status"] = absl::StrCat(resp_code);
  }
  headers["content-length"] = absl::StrCat(strlen(kErrorResponseBody));
  SendHeadersAndBody(std::move(headers), kErrorResponseBody);
}

void QuicSimpleServerStream::SendIncompleteResponse(
    Http2HeaderBlock response_headers,
    absl::string_view body) {
  QUIC_DLOG(INFO) << "Stream " << id() << " writing headers (fin = false) : "
                  << response_headers.DebugString();
  WriteHeaders(std::move(response_headers), /*fin=*/false, nullptr);
  QUICHE_DCHECK(!response_sent_);
  response_sent_ = true;

  QUIC_DLOG(INFO) << "Stream " << id()
                  << " writing body (fin = false) with size: " << body.size();
  if (!body.empty()) {
    WriteOrBufferBody(body, /*fin=*/false);
  }
}

void QuicSimpleServerStream::SendHeadersAndBody(
    Http2HeaderBlock response_headers,
    absl::string_view body) {
  SendHeadersAndBodyAndTrailers(std::move(response_headers), body,
                                Http2HeaderBlock());
}

void QuicSimpleServerStream::SendHeadersAndBodyAndTrailers(
    Http2HeaderBlock response_headers,
    absl::string_view body,
    Http2HeaderBlock response_trailers) {
  // Send the headers, with a FIN if there's nothing else to send.
  bool send_fin = (body.empty() && response_trailers.empty());
  QUIC_DLOG(INFO) << "Stream " << id() << " writing headers (fin = " << send_fin
                  << ") : " << response_headers.DebugString();
  WriteHeaders(std::move(response_headers), send_fin, nullptr);
  QUICHE_DCHECK(!response_sent_);
  response_sent_ = true;
  if (send_fin) {
    // Nothing else to send.
    return;
  }

  // Send the body, with a FIN if there's no trailers to send.
  send_fin = response_trailers.empty();
  QUIC_DLOG(INFO) << "Stream " << id() << " writing body (fin = " << send_fin
                  << ") with size: " << body.size();
  if (!body.empty() || send_fin) {
    WriteOrBufferBody(body, send_fin);
  }
  if (send_fin) {
    // Nothing else to send.
    return;
  }

  // Send the trailers. A FIN is always sent with trailers.
  QUIC_DLOG(INFO) << "Stream " << id() << " writing trailers (fin = true): "
                  << response_trailers.DebugString();
  WriteTrailers(std::move(response_trailers), nullptr);
}

const char* const QuicSimpleServerStream::kErrorResponseBody = "bad";
const char* const QuicSimpleServerStream::kNotFoundResponseBody =
    "file not found";

}  // namespace quic

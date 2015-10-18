// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_server_stream.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "net/quic/quic_data_stream.h"
#include "net/quic/quic_spdy_session.h"
#include "net/quic/spdy_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/quic/quic_in_memory_cache.h"

using base::StringPiece;
using base::StringToInt;
using std::string;

namespace net {
namespace tools {

QuicSpdyServerStream::QuicSpdyServerStream(QuicStreamId id,
                                           QuicSpdySession* session)
    : QuicDataStream(id, session), content_length_(-1) {
}

QuicSpdyServerStream::~QuicSpdyServerStream() {
}

void QuicSpdyServerStream::OnStreamHeadersComplete(bool fin, size_t frame_len) {
  QuicDataStream::OnStreamHeadersComplete(fin, frame_len);
  if (!ParseRequestHeaders(decompressed_headers().data(),
                           decompressed_headers().length())) {
    // Headers were invalid.
    SendErrorResponse();
  }
  MarkHeadersConsumed(decompressed_headers().length());
}

void QuicSpdyServerStream::OnDataAvailable() {
  while (HasBytesToRead()) {
    struct iovec iov;
    if (GetReadableRegions(&iov, 1) == 0) {
      // No more data to read.
      break;
    }
    DVLOG(1) << "Processed " << iov.iov_len << " bytes for stream " << id();
    body_.append(static_cast<char*>(iov.iov_base), iov.iov_len);

    if (content_length_ >= 0 &&
        static_cast<int>(body_.size()) > content_length_) {
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

  if (request_headers_.empty()) {
    SendErrorResponse();
    return;
  }

  if (content_length_ > 0 &&
      content_length_ != static_cast<int>(body_.size())) {
    SendErrorResponse();
    return;
  }

  SendResponse();
}

bool QuicSpdyServerStream::ParseRequestHeaders(const char* data,
                                               uint32 data_len) {
  DCHECK(headers_decompressed());
  SpdyFramer framer(HTTP2);
  size_t len = framer.ParseHeaderBlockInBuffer(data,
                                               data_len,
                                               &request_headers_);
  DCHECK_LE(len, data_len);
  if (len == 0 || request_headers_.empty()) {
    return false;  // Headers were invalid.
  }

  if (data_len > len) {
    body_.append(data + len, data_len - len);
  }
  if (ContainsKey(request_headers_, "content-length")) {
    string delimiter;
    delimiter.push_back('\0');
    std::vector<string> values =
        base::SplitString(request_headers_["content-length"], delimiter,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    for (const string& value : values) {
      int new_value;
      if (!StringToInt(value, &new_value) || new_value < 0) {
        return false;
      }
      if (content_length_ < 0) {
        content_length_ = new_value;
        continue;
      }
      if (new_value != content_length_) {
        return false;
      }
    }
  }

  return true;
}

void QuicSpdyServerStream::SendResponse() {
  if (!ContainsKey(request_headers_, ":authority") ||
      !ContainsKey(request_headers_, ":path")) {
    SendErrorResponse();
    return;
  }

  // Find response in cache. If not found, send error response.
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(
          request_headers_[":authority"], request_headers_[":path"]);
  if (response == nullptr) {
    SendErrorResponse();
    return;
  }

  if (response->response_type() == QuicInMemoryCache::CLOSE_CONNECTION) {
    DVLOG(1) << "Special response: closing connection.";
    CloseConnection(QUIC_NO_ERROR);
    return;
  }

  if (response->response_type() == QuicInMemoryCache::IGNORE_REQUEST) {
    DVLOG(1) << "Special response: ignoring request.";
    return;
  }

  DVLOG(1) << "Sending response for stream " << id();
  SendHeadersAndBody(
      SpdyUtils::ConvertSpdy3ResponseHeadersToSpdy4(response->headers()),
      response->body());
}

void QuicSpdyServerStream::SendErrorResponse() {
  DVLOG(1) << "Sending error response for stream " << id();
  SpdyHeaderBlock headers;
  headers[":status"] = "500";
  headers["content-length"] = "3";
  SendHeadersAndBody(headers, "bad");
}

void QuicSpdyServerStream::SendHeadersAndBody(
    const SpdyHeaderBlock& response_headers,
    StringPiece body) {
  // We only support SPDY and HTTP, and neither handles bidirectional streaming.
  if (!read_side_closed()) {
    CloseReadSide();
  }

  WriteHeaders(response_headers, body.empty(), nullptr);

  if (!body.empty()) {
    WriteOrBufferData(body, true, nullptr);
  }
}

}  // namespace tools
}  // namespace net

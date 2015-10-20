// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_client_stream.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/spdy_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/spdy_balsa_utils.h"

using base::StringPiece;
using std::string;
using base::StringToInt;

namespace net {
namespace tools {

QuicSpdyClientStream::QuicSpdyClientStream(QuicStreamId id,
                                           QuicClientSession* session)
    : QuicDataStream(id, session),
      content_length_(-1),
      response_code_(0),
      header_bytes_read_(0),
      header_bytes_written_(0),
      allow_bidirectional_data_(false) {}

QuicSpdyClientStream::~QuicSpdyClientStream() {
}

void QuicSpdyClientStream::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!allow_bidirectional_data_ && !write_side_closed()) {
    DVLOG(1) << "Got a response before the request was complete.  "
             << "Aborting request.";
    CloseWriteSide();
  }
  QuicDataStream::OnStreamFrame(frame);
}

void QuicSpdyClientStream::OnStreamHeadersComplete(bool fin,
                                                   size_t frame_len) {
  header_bytes_read_ = frame_len;
  QuicDataStream::OnStreamHeadersComplete(fin, frame_len);
  if (!ParseResponseHeaders(decompressed_headers().data(),
                            decompressed_headers().length())) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }
  MarkHeadersConsumed(decompressed_headers().length());
}

void QuicSpdyClientStream::OnDataAvailable() {
  while (HasBytesToRead()) {
    struct iovec iov;
    if (GetReadableRegions(&iov, 1) == 0) {
      // No more data to read.
      break;
    }
    DVLOG(1) << "Client processed " << iov.iov_len << " bytes for stream "
             << id();
    data_.append(static_cast<char*>(iov.iov_base), iov.iov_len);

    if (content_length_ >= 0 &&
        static_cast<int>(data_.size()) > content_length_) {
      Reset(QUIC_BAD_APPLICATION_PAYLOAD);
      return;
    }
    MarkConsumed(iov.iov_len);
  }
  if (sequencer()->IsClosed()) {
    OnFinRead();
  } else {
    sequencer()->SetUnblocked();
  }
}

bool QuicSpdyClientStream::ParseResponseHeaders(const char* data,
                                                uint32 data_len) {
  DCHECK(headers_decompressed());
  SpdyFramer framer(HTTP2);
  size_t len = framer.ParseHeaderBlockInBuffer(data,
                                               data_len,
                                               &response_headers_);
  DCHECK_LE(len, data_len);
  if (len == 0 || response_headers_.empty()) {
    return false;  // Headers were invalid.
  }

  if (data_len > len) {
    data_.append(data + len, data_len - len);
  }
  if (ContainsKey(response_headers_, "content-length") &&
      !StringToInt(StringPiece(response_headers_["content-length"]),
                   &content_length_)) {
    return false;  // Invalid content-length.
  }
  string status = response_headers_[":status"].as_string();
  size_t end = status.find(" ");
  if (end != string::npos) {
    status.erase(end);
  }
  if (!StringToInt(status, &response_code_)) {
    return false;  // Invalid response code.
  }
  return true;
}

size_t QuicSpdyClientStream::SendRequest(const SpdyHeaderBlock& headers,
                                         StringPiece body,
                                         bool fin) {
  bool send_fin_with_headers = fin && body.empty();
  size_t bytes_sent = body.size();
  header_bytes_written_ =
      WriteHeaders(headers, send_fin_with_headers, nullptr);
  bytes_sent += header_bytes_written_;

  if (!body.empty()) {
    WriteOrBufferData(body, fin, nullptr);
  }

  return bytes_sent;
}

void QuicSpdyClientStream::SendBody(const string& data, bool fin) {
  SendBody(data, fin, nullptr);
}

void QuicSpdyClientStream::SendBody(const string& data,
                                    bool fin,
                                    QuicAckListenerInterface* delegate) {
  WriteOrBufferData(data, fin, delegate);
}

}  // namespace tools
}  // namespace net

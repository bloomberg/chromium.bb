// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_client_stream.h"

#include "net/spdy/spdy_framer.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/spdy_utils.h"

using base::StringPiece;
using std::string;

namespace net {
namespace tools {

static const size_t kHeaderBufInitialSize = 4096;

QuicSpdyClientStream::QuicSpdyClientStream(QuicStreamId id,
                                           QuicClientSession* session)
    : QuicDataStream(id, session),
      read_buf_(new GrowableIOBuffer()),
      response_headers_received_(false) {
}

QuicSpdyClientStream::~QuicSpdyClientStream() {
}

bool QuicSpdyClientStream::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!write_side_closed()) {
    DLOG(INFO) << "Got a response before the request was complete.  "
               << "Aborting request.";
    CloseWriteSide();
  }
  return QuicDataStream::OnStreamFrame(frame);
}

uint32 QuicSpdyClientStream::ProcessData(const char* data, uint32 length) {
  uint32 total_bytes_processed = 0;

  // Are we still reading the response headers.
  if (!response_headers_received_) {
    // Grow the read buffer if necessary.
    if (read_buf_->RemainingCapacity() < (int)length) {
      read_buf_->SetCapacity(read_buf_->capacity() + kHeaderBufInitialSize);
    }
    memcpy(read_buf_->data(), data, length);
    read_buf_->set_offset(read_buf_->offset() + length);
    ParseResponseHeaders();
  } else {
    data_.append(data + total_bytes_processed, length - total_bytes_processed);
  }
  return length;
}

void QuicSpdyClientStream::OnFinRead() {
  ReliableQuicStream::OnFinRead();
  if (!response_headers_received_) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  } else if ((headers().content_length_status() ==
             BalsaHeadersEnums::VALID_CONTENT_LENGTH) &&
             data_.size() != headers().content_length()) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  }
}

ssize_t QuicSpdyClientStream::SendRequest(const BalsaHeaders& headers,
                                          StringPiece body,
                                          bool fin) {
  SpdyHeaderBlock header_block =
      SpdyUtils::RequestHeadersToSpdyHeaders(headers);

  bool send_fin_with_headers = fin && body.empty();
  string headers_string = session()->compressor()->CompressHeadersWithPriority(
      priority(), header_block);
  WriteOrBufferData(headers_string, send_fin_with_headers);

  if (!body.empty()) {
    WriteOrBufferData(body, fin);
  }

  return headers_string.size() + body.size();
}

int QuicSpdyClientStream::ParseResponseHeaders() {
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  SpdyFramer framer(SPDY3);
  SpdyHeaderBlock headers;
  char* data = read_buf_->StartOfBuffer();
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                               &headers);
  if (len == 0) {
    return -1;
  }

  if (!SpdyUtils::FillBalsaResponseHeaders(headers, &headers_)) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return -1;
  }
  response_headers_received_ = true;

  size_t delta = read_buf_len - len;
  if (delta > 0) {
    data_.append(data + len, delta);
  }

  return len;
}

// Sends body data to the server and returns the number of bytes sent.
void QuicSpdyClientStream::SendBody(const string& data, bool fin) {
  return WriteOrBufferData(data, fin);
}

}  // namespace tools
}  // namespace net

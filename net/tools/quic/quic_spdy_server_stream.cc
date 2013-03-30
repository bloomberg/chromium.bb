// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_server_stream.h"

#include "net/spdy/spdy_framer.h"
#include "net/tools/quic/spdy_utils.h"

using std::string;

namespace net {

static const size_t kHeaderBufInitialSize = 4096;

QuicSpdyServerStream::QuicSpdyServerStream(QuicStreamId id,
                                           QuicSession* session)
    : QuicReliableServerStream(id, session),
      read_buf_(new GrowableIOBuffer()),
      request_headers_received_(false) {
}

QuicSpdyServerStream::~QuicSpdyServerStream() {
}

uint32 QuicSpdyServerStream::ProcessData(const char* data, uint32 length) {
  uint32 total_bytes_processed = 0;

  // Are we still reading the request headers.
  if (!request_headers_received_) {
    // Grow the read buffer if necessary.
    if (read_buf_->RemainingCapacity() < (int)length) {
      read_buf_->SetCapacity(read_buf_->capacity() + kHeaderBufInitialSize);
    }
    memcpy(read_buf_->data(), data, length);
    read_buf_->set_offset(read_buf_->offset() + length);
    ParseRequestHeaders();
  } else {
    mutable_body()->append(data + total_bytes_processed,
                           length - total_bytes_processed);
  }
  return length;
}

void QuicSpdyServerStream::TerminateFromPeer(bool half_close) {
  ReliableQuicStream::TerminateFromPeer(half_close);
  // This is a full close: do not send a response.
  if (!half_close) {
    return;
  }
  if (!request_headers_received_) {
    SendErrorResponse();  // We're not done writing headers.
  } else if ((headers().content_length_status() ==
             BalsaHeadersEnums::VALID_CONTENT_LENGTH) &&
             mutable_body()->size() != headers().content_length()) {
    SendErrorResponse();  // Invalid content length
  } else {
    SendResponse();
  }
}

void QuicSpdyServerStream::SendHeaders(
    const BalsaHeaders& response_headers) {
  string headers =
      SpdyUtils::SerializeResponseHeaders(response_headers);
  WriteData(headers, false);
}

int QuicSpdyServerStream::ParseRequestHeaders() {
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  SpdyFramer framer(3);
  SpdyHeaderBlock headers;
  char* data = read_buf_->StartOfBuffer();
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                               &headers);
  if (len == 0) {
    return -1;
  }

  if (!SpdyUtils::FillBalsaRequestHeaders(headers, mutable_headers())) {
    SendErrorResponse();
    return -1;
  }

  size_t delta = read_buf_len - len;
  if (delta > 0) {
    mutable_body()->append(data + len, delta);
  }

  request_headers_received_ = true;
  return len;
}

}  // namespace net

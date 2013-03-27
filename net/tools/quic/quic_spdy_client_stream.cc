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

static const size_t kHeaderBufInitialSize = 4096;

QuicSpdyClientStream::QuicSpdyClientStream(QuicStreamId id,
                                           QuicClientSession* session)
    : QuicReliableClientStream(id, session),
      read_buf_(new GrowableIOBuffer()),
      response_headers_received_(false) {
}

QuicSpdyClientStream::~QuicSpdyClientStream() {
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
    mutable_data()->append(data + total_bytes_processed,
                           length - total_bytes_processed);
  }
  return length;
}

ssize_t QuicSpdyClientStream::SendRequest(const BalsaHeaders& headers,
                                          StringPiece body,
                                          bool fin) {
  string headers_string = SpdyUtils::SerializeRequestHeaders(headers);

  bool has_body = !body.empty();

  WriteData(headers_string, fin && !has_body);

  if (has_body) {
    WriteData(body, fin);
  }

  return headers_string.size() + body.size();
}

int QuicSpdyClientStream::ParseResponseHeaders() {
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  SpdyFramer framer(3);
  SpdyHeaderBlock headers;
  char* data = read_buf_->StartOfBuffer();
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                               &headers);
  if (len == 0) {
    return -1;
  }

  if (!SpdyUtils::FillBalsaResponseHeaders(headers, mutable_headers())) {
    Close(QUIC_BAD_APPLICATION_PAYLOAD);
    return -1;
  }

  size_t delta = read_buf_len - len;
  if (delta > 0) {
    mutable_data()->append(data + len, delta);
  }

  return len;
}

}  // namespace net

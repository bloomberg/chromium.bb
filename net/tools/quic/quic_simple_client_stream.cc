// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client_stream.h"

#include "net/http/http_response_info.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/tools/quic/quic_simple_client_session.h"

using base::StringPiece;
using std::string;

namespace net {
namespace tools {

static const size_t kHeaderBufInitialSize = 4096;

QuicSimpleClientStream::QuicSimpleClientStream(QuicStreamId id,
                                               QuicSimpleClientSession* session)
    : QuicDataStream(id, session),
      read_buf_(new GrowableIOBuffer()),
      response_headers_received_(false),
      header_bytes_read_(0),
      header_bytes_written_(0) {
  read_buf_->SetCapacity(kHeaderBufInitialSize);
}

QuicSimpleClientStream::~QuicSimpleClientStream() {
}

void QuicSimpleClientStream::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!write_side_closed()) {
    DVLOG(1) << "Got a response before the request was complete.  "
             << "Aborting request.";
    CloseWriteSide();
  }
  QuicDataStream::OnStreamFrame(frame);
}

void QuicSimpleClientStream::OnStreamHeadersComplete(bool fin,
                                                   size_t frame_len) {
  header_bytes_read_ = frame_len;
  QuicDataStream::OnStreamHeadersComplete(fin, frame_len);
}

uint32 QuicSimpleClientStream::ProcessData(const char* data,
                                           uint32 data_len) {
  int total_bytes_processed = 0;

  // Are we still reading the response headers.
  if (!response_headers_received_) {
    // Grow the read buffer if necessary.
    if (read_buf_->RemainingCapacity() < (int)data_len) {
      read_buf_->SetCapacity(read_buf_->capacity() + kHeaderBufInitialSize);
    }
    memcpy(read_buf_->data(), data, data_len);
    read_buf_->set_offset(read_buf_->offset() + data_len);
    ParseResponseHeaders();
  } else {
    data_.append(data + total_bytes_processed,
                 data_len - total_bytes_processed);
  }
  return data_len;
}

void QuicSimpleClientStream::OnFinRead() {
  ReliableQuicStream::OnFinRead();
  if (!response_headers_received_) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  } else if (headers()->GetContentLength() != -1 &&
             data_.size() !=
                 static_cast<size_t>(headers()->GetContentLength())) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  }
}

size_t QuicSimpleClientStream::SendRequest(const HttpRequestInfo& headers,
                                           StringPiece body,
                                           bool fin) {
  SpdyHeaderBlock header_block;
  CreateSpdyHeadersFromHttpRequest(headers,
                                   headers.extra_headers,
                                   SPDY3,
                                   /*direct=*/ true,
                                   &header_block);

  bool send_fin_with_headers = fin && body.empty();
  size_t bytes_sent = body.size();
  header_bytes_written_ =
      WriteHeaders(header_block, send_fin_with_headers, nullptr);
  bytes_sent += header_bytes_written_;

  if (!body.empty()) {
    WriteOrBufferData(body, fin, nullptr);
  }

  return bytes_sent;
}

int QuicSimpleClientStream::ParseResponseHeaders() {
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  SpdyFramer framer(SPDY3);
  SpdyHeaderBlock headers;
  char* data = read_buf_->StartOfBuffer();
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                               &headers);
  if (len == 0) {
    return -1;
  }

  HttpResponseInfo info;
  if (!SpdyHeadersToHttpResponse(headers, SPDY3, &info)) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return -1;
  }
  headers_ = info.headers;

  response_headers_received_ = true;

  size_t delta = read_buf_len - len;
  if (delta > 0) {
    data_.append(data + len, delta);
  }

  return static_cast<int>(len);
}

void QuicSimpleClientStream::SendBody(const string& data, bool fin) {
  SendBody(data, fin, nullptr);
}

void QuicSimpleClientStream::SendBody(
    const string& data,
    bool fin,
    QuicAckNotifier::DelegateInterface* delegate) {
  WriteOrBufferData(data, fin, delegate);
}

}  // namespace tools
}  // namespace net

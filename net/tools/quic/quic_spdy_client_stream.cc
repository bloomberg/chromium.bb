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
    : QuicSpdyStream(id, session),
      content_length_(-1),
      response_code_(0),
      header_bytes_read_(0),
      header_bytes_written_(0),
      allow_bidirectional_data_(false) {}

QuicSpdyClientStream::~QuicSpdyClientStream() {}

void QuicSpdyClientStream::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!allow_bidirectional_data_ && !write_side_closed()) {
    DVLOG(1) << "Got a response before the request was complete.  "
             << "Aborting request.";
    CloseWriteSide();
  }
  QuicSpdyStream::OnStreamFrame(frame);
}

void QuicSpdyClientStream::OnInitialHeadersComplete(bool fin,
                                                    size_t frame_len) {
  QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len);

  DCHECK(headers_decompressed());
  header_bytes_read_ = frame_len;
  if (!SpdyUtils::ParseHeaders(decompressed_headers().data(),
                               decompressed_headers().length(),
                               &content_length_, &response_headers_)) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  string status = response_headers_[":status"].as_string();
  size_t end = status.find(" ");
  if (end != string::npos) {
    status.erase(end);
  }
  if (!StringToInt(status, &response_code_)) {
    // Invalid response code.
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  MarkHeadersConsumed(decompressed_headers().length());
}

void QuicSpdyClientStream::OnTrailingHeadersComplete(bool fin,
                                                     size_t frame_len) {
  QuicSpdyStream::OnTrailingHeadersComplete(fin, frame_len);

  size_t final_byte_offset = 0;
  if (!SpdyUtils::ParseTrailers(decompressed_trailers().data(),
                                decompressed_trailers().length(),
                                &final_byte_offset, &response_trailers_)) {
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }
  MarkTrailersConsumed(decompressed_trailers().length());

  // The data on this stream ends at |final_byte_offset|.
  DVLOG(1) << "Stream ends at byte offset: " << final_byte_offset
           << "  currently read: " << stream_bytes_read();
  OnStreamFrame(
      QuicStreamFrame(id(), /*fin=*/true, final_byte_offset, StringPiece()));
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

size_t QuicSpdyClientStream::SendRequest(const SpdyHeaderBlock& headers,
                                         StringPiece body,
                                         bool fin) {
  bool send_fin_with_headers = fin && body.empty();
  size_t bytes_sent = body.size();
  header_bytes_written_ = WriteHeaders(headers, send_fin_with_headers, nullptr);
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
                                    QuicAckListenerInterface* listener) {
  WriteOrBufferData(data, fin, listener);
}

}  // namespace tools
}  // namespace net

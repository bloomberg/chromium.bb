// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_reliable_server_stream.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "net/tools/quic/quic_in_memory_cache.h"

using base::StringPiece;

namespace net {

QuicReliableServerStream::QuicReliableServerStream(QuicStreamId id,
                                                  QuicSession* session)
    : ReliableQuicStream(id, session) {
}


void QuicReliableServerStream::SendResponse() {
  // Find response in cache. If not found, send error response.
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(headers_);
  if (response == NULL) {
    SendErrorResponse();
    return;
  }

  DLOG(INFO) << "Sending response for stream " << id();
  SendHeaders(response->headers());
  WriteData(response->body(), true);
}

void QuicReliableServerStream::SendErrorResponse() {
  DLOG(INFO) << "Sending error response for stream " << id();
  BalsaHeaders headers;
  headers.SetResponseFirstlineFromStringPieces(
      "HTTP/1.1", "500", "Server Error");
  headers.ReplaceOrAppendHeader("content-length", "3");
  SendHeaders(headers);
  WriteData("bad", true);
}

QuicConsumedData QuicReliableServerStream::WriteData(StringPiece data,
                                                     bool fin) {
  // We only support SPDY and HTTP, and neither handles bidirectional streaming.
  if (!read_side_closed()) {
    CloseReadSide();
  }
  return ReliableQuicStream::WriteData(data, fin);
}

}  // namespace net

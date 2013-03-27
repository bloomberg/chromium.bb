// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_reliable_client_stream.h"

using std::string;

namespace net {

// Sends body data to the server and returns the number of bytes sent.
ssize_t QuicReliableClientStream::SendBody(const string& data, bool fin) {
  return WriteData(data, fin).bytes_consumed;
}

bool QuicReliableClientStream::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!write_side_closed()) {
    DLOG(INFO) << "Got a response before the request was complete.  "
               << "Aborting request.";
    CloseWriteSide();
  }
  return ReliableQuicStream::OnStreamFrame(frame);
}

}  // namespace net

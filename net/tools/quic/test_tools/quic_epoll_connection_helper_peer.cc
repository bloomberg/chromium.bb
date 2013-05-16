// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_epoll_connection_helper_peer.h"

#include "net/tools/quic/quic_epoll_connection_helper.h"

namespace net {
namespace tools {
namespace test {

// static
void QuicEpollConnectionHelperPeer::SetWriter(QuicEpollConnectionHelper* helper,
                                              QuicPacketWriter* writer) {
  helper->writer_ = writer;
}

}  // namespace test
}  // namespace tools
}  // namespace net

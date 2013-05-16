// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_client_peer.h"

#include "net/tools/quic/quic_client.h"

namespace net {
namespace tools {
namespace test {

// static
void QuicClientPeer::Reinitialize(QuicClient* client) {
  client->initialized_ = false;
  client->epoll_server_.UnregisterFD(client->fd_);
  client->Initialize();
}

// static
int QuicClientPeer::GetFd(QuicClient* client) {
  return client->fd_;
}

}  // namespace test
}  // namespace tools
}  // namespace net

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_CLIENT_PEER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_CLIENT_PEER_H_

namespace net {
namespace tools {

class QuicClient;

namespace test {

class QuicClientPeer {
 public:
  static int GetFd(QuicClient* client);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_CLIENT_PEER_H_

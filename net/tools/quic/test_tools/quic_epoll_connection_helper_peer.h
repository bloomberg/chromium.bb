// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_EPOLL_CONNECTION_HELPER_PEER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_EPOLL_CONNECTION_HELPER_PEER_H_

#include "base/basictypes.h"

namespace net {
namespace tools {

class QuicPacketWriter;
class QuicEpollConnectionHelper;

namespace test {

class QuicEpollConnectionHelperPeer {
 public:
  static void SetWriter(QuicEpollConnectionHelper* helper,
                        QuicPacketWriter* writer);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicEpollConnectionHelperPeer);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_EPOLL_CONNECTION_HELPER_PEER_H_

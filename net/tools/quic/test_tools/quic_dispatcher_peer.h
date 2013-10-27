// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_

#include "net/quic/test_tools/quic_test_writer.h"
#include "net/tools/quic/quic_dispatcher.h"

namespace net {
namespace tools {
namespace test {

class QuicDispatcherPeer {
 public:
  static void SetTimeWaitListManager(
      QuicDispatcher* dispatcher,
      QuicTimeWaitListManager* time_wait_list_manager);

  static void SetWriteBlocked(QuicDispatcher* dispatcher);

  static void UseWriter(QuicDispatcher* dispatcher,
                        net::test::QuicTestWriter* writer);

  static QuicPacketWriter* GetWriter(QuicDispatcher* dispatcher);

  static QuicEpollConnectionHelper* GetHelper(QuicDispatcher* dispatcher);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_

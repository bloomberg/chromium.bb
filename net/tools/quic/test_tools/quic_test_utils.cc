// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_test_utils.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"

using net::test::MockHelper;

namespace net {
namespace tools {
namespace test {

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               int fd,
                               EpollServer* eps,
                               bool is_server)
    : QuicConnection(guid, address,
                     new QuicEpollConnectionHelper(fd, eps), is_server),
      has_mock_helper_(false) {
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               bool is_server)
    : QuicConnection(guid, address, new MockHelper(), is_server),
      has_mock_helper_(true) {
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               bool is_server)
    : QuicConnection(guid, address, helper, is_server),
      has_mock_helper_(false) {
}

MockConnection::~MockConnection() {
}

void MockConnection::AdvanceTime(QuicTime::Delta delta) {
  CHECK(has_mock_helper_) << "Cannot advance time unless a MockClock is being"
                             " used";
  static_cast<MockHelper*>(helper())->AdvanceTime(delta);
}

}  // namespace test
}  // namespace tools
}  // namespace net

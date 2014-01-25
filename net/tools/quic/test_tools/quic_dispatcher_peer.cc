// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_dispatcher_peer.h"

#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"

namespace net {
namespace tools {
namespace test {

// static
void QuicDispatcherPeer::SetTimeWaitListManager(
    QuicDispatcher* dispatcher,
    QuicTimeWaitListManager* time_wait_list_manager) {
  dispatcher->time_wait_list_manager_.reset(time_wait_list_manager);
}

// static
void QuicDispatcherPeer::UseWriter(QuicDispatcher* dispatcher,
                                   QuicPacketWriterWrapper* writer) {
  writer->set_writer(dispatcher->writer_->release_writer());
  dispatcher->writer_->set_writer(writer);
}

// static
QuicPacketWriterWrapper* QuicDispatcherPeer::GetWriter(
    QuicDispatcher* dispatcher) {
  return dispatcher->writer_.get();
}

// static
QuicEpollConnectionHelper* QuicDispatcherPeer::GetHelper(
    QuicDispatcher* dispatcher) {
  return dispatcher->helper_.get();
}

}  // namespace test
}  // namespace tools
}  // namespace net

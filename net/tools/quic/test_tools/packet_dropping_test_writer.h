// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_alarm.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/test_tools/quic_test_writer.h"
#include "net/tools/quic/quic_epoll_clock.h"
#include "net/tools/quic/test_tools/quic_test_client.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"

namespace net {
namespace tools {
namespace test {

// Simulates a connection that drops packets a configured percentage of the time
// and has a blocked socket a configured percentage of the time.
class PacketDroppingTestWriter : public net::test::QuicTestWriter {
 public:
  PacketDroppingTestWriter();

  virtual ~PacketDroppingTestWriter();

  void SetConnectionHelper(QuicEpollConnectionHelper* helper);

  // QuicPacketWriter methods:
  virtual WriteResult WritePacket(
      const char* buffer, size_t buf_len,
      const IPAddressNumber& self_address,
      const IPEndPoint& peer_address,
      QuicBlockedWriterInterface* blocked_writer) OVERRIDE;

  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;

  QuicBlockedWriterInterface* blocked_writer() { return blocked_writer_; }

  // The percent of time a packet is simulated as being lost.
  void set_fake_packet_loss_percentage(int32 fake_packet_loss_percentage) {
    fake_packet_loss_percentage_ = fake_packet_loss_percentage;
  }

  // The percent of time WritePacket will block and set WriteResult's status
  // to WRITE_STATUS_BLOCKED.
  void set_fake_blocked_socket_percentage(
      int32 fake_blocked_socket_percentage) {
    DCHECK(clock_);
    fake_blocked_socket_percentage_  = fake_blocked_socket_percentage;
  }

 private:
  const QuicClock* clock_;
  scoped_ptr<QuicAlarm> write_unblocked_alarm_;
  QuicBlockedWriterInterface* blocked_writer_;
  uint32 fake_packet_loss_percentage_;
  uint32 fake_blocked_socket_percentage_;
  SimpleRandom simple_random_;

  DISALLOW_COPY_AND_ASSIGN(PacketDroppingTestWriter);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_

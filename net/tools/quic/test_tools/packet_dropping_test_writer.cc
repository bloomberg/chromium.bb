// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/packet_dropping_test_writer.h"

#include "base/rand_util.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_socket_utils.h"

using net::test::QuicTestWriter;

namespace net {
namespace tools {
namespace test {

// An alarm that is scheduled if a blocked socket is simulated to indicate
// it's writable again.
class WriteUnblockedAlarm : public QuicAlarm::Delegate {
 public:
  explicit WriteUnblockedAlarm(PacketDroppingTestWriter* writer)
      : writer_(writer) { }

  virtual QuicTime OnAlarm() OVERRIDE {
    DCHECK(writer_->blocked_writer());
    writer_->blocked_writer()->OnCanWrite();
    return QuicTime::Zero();
  }

 private:
  PacketDroppingTestWriter* writer_;
};

PacketDroppingTestWriter::PacketDroppingTestWriter()
    : clock_(NULL),
      blocked_writer_(NULL),
      fake_packet_loss_percentage_(0),
      fake_blocked_socket_percentage_(0) {
  int64 seed = base::RandUint64();
  LOG(INFO) << "Seeding packet loss with " << seed;
  simple_random_.set_seed(seed);
}

PacketDroppingTestWriter::~PacketDroppingTestWriter() { }

void PacketDroppingTestWriter::SetConnectionHelper(
    QuicEpollConnectionHelper* helper) {
  clock_ = helper->GetClock();
  write_unblocked_alarm_.reset(
      helper->CreateAlarm(new WriteUnblockedAlarm(this)));
}

WriteResult PacketDroppingTestWriter::WritePacket(
    const char* buffer, size_t buf_len,
    const net::IPAddressNumber& self_address,
    const net::IPEndPoint& peer_address,
    QuicBlockedWriterInterface* blocked_writer) {
  if (fake_packet_loss_percentage_ > 0 &&
      simple_random_.RandUint64() % 100 < fake_packet_loss_percentage_) {
    DLOG(INFO) << "Dropping packet.";
    return WriteResult(WRITE_STATUS_OK, buf_len);
  }
  if (fake_blocked_socket_percentage_ > 0 &&
      simple_random_.RandUint64() % 100 < fake_blocked_socket_percentage_) {
    DLOG(INFO) << "Blocking socket.";
    if (!write_unblocked_alarm_->IsSet()) {
      blocked_writer_ = blocked_writer;
      // Set the alarm for 1ms in the future.
      write_unblocked_alarm_->Set(
          clock_->ApproximateNow().Add(
              QuicTime::Delta::FromMilliseconds(1)));
    }
    return WriteResult(WRITE_STATUS_BLOCKED, EAGAIN);
  }

  return writer()->WritePacket(buffer, buf_len, self_address, peer_address,
                               blocked_writer);
}

bool PacketDroppingTestWriter::IsWriteBlockedDataBuffered() const {
  return false;
}

}  // namespace test
}  // namespace tools
}  // namespace net

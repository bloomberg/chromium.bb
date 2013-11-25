// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/packet_dropping_test_writer.h"

#include <limits>

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

// An alarm that is scheduled every time a new packet is to be written at a
// later point.
class DelayAlarm : public QuicAlarm::Delegate {
 public:
  explicit DelayAlarm(PacketDroppingTestWriter* writer)
      : writer_(writer) { }

  virtual QuicTime OnAlarm() OVERRIDE {
    return writer_->ReleaseOldPackets();
  }

 private:
  PacketDroppingTestWriter* writer_;
};

PacketDroppingTestWriter::PacketDroppingTestWriter()
    : clock_(NULL),
      blocked_writer_(NULL),
      cur_buffer_size_(0),
      config_mutex_(),
      fake_packet_loss_percentage_(0),
      fake_blocked_socket_percentage_(0),
      fake_packet_reorder_percentage_(0),
      fake_packet_delay_(QuicTime::Delta::Zero()),
      fake_bandwidth_(QuicBandwidth::Zero()),
      buffer_size_(0) {
  uint32 seed = base::RandInt(0, std::numeric_limits<int32>::max());
  LOG(INFO) << "Seeding packet loss with " << seed;
  simple_random_.set_seed(seed);
}

PacketDroppingTestWriter::~PacketDroppingTestWriter() { }

void PacketDroppingTestWriter::SetConnectionHelper(
    QuicEpollConnectionHelper* helper) {
  clock_ = helper->GetClock();
  write_unblocked_alarm_.reset(
      helper->CreateAlarm(new WriteUnblockedAlarm(this)));
  delay_alarm_.reset(
        helper->CreateAlarm(new DelayAlarm(this)));
}

WriteResult PacketDroppingTestWriter::WritePacket(
    const char* buffer, size_t buf_len,
    const net::IPAddressNumber& self_address,
    const net::IPEndPoint& peer_address,
    QuicBlockedWriterInterface* blocked_writer) {
  ReleaseOldPackets();

  base::AutoLock locked(config_mutex_);
  if (fake_packet_loss_percentage_ > 0 &&
      simple_random_.RandUint64() % 100 <
          static_cast<uint64>(fake_packet_loss_percentage_)) {
    DLOG(INFO) << "Dropping packet.";
    return WriteResult(WRITE_STATUS_OK, buf_len);
  }
  if (fake_blocked_socket_percentage_ > 0 &&
      simple_random_.RandUint64() % 100 <
          static_cast<uint64>(fake_blocked_socket_percentage_)) {
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

  if (!fake_packet_delay_.IsZero() || !fake_bandwidth_.IsZero()) {
    if (buffer_size_ > 0 && buf_len + cur_buffer_size_ > buffer_size_) {
      // Drop packets which do not fit into the buffer.
      DLOG(INFO) << "Dropping packet because the buffer is full.";
      return WriteResult(WRITE_STATUS_OK, buf_len);
    }

    // Queue it to be sent.
    QuicTime send_time = clock_->ApproximateNow().Add(fake_packet_delay_);
    if (!fake_bandwidth_.IsZero()) {
      // Calculate a time the bandwidth limit would impose.
      QuicTime::Delta bandwidth_delay = QuicTime::Delta::FromMicroseconds(
          (buf_len * kNumMicrosPerSecond) /
          fake_bandwidth_.ToBytesPerSecond());
      send_time = delayed_packets_.empty() ?
          send_time.Add(bandwidth_delay) :
          delayed_packets_.back().send_time.Add(bandwidth_delay);
    }
    delayed_packets_.push_back(DelayedWrite(buffer, buf_len, self_address,
                                            peer_address, send_time));
    cur_buffer_size_ += buf_len;

    // Set the alarm if it's not yet set.
    if (!delay_alarm_->IsSet()) {
      delay_alarm_->Set(send_time);
    }

    return WriteResult(WRITE_STATUS_OK, buf_len);
  }

  return writer()->WritePacket(buffer, buf_len, self_address, peer_address,
                               blocked_writer);
}

bool PacketDroppingTestWriter::IsWriteBlockedDataBuffered() const {
  return false;
}

QuicTime PacketDroppingTestWriter::ReleaseNextPacket() {
  if (delayed_packets_.empty()) {
    return QuicTime::Zero();
  }
  base::AutoLock locked(config_mutex_);
  DelayedPacketList::iterator iter = delayed_packets_.begin();
  // Determine if we should re-order.
  if (delayed_packets_.size() > 1 && fake_packet_reorder_percentage_ > 0 &&
      simple_random_.RandUint64() % 100 <
          static_cast<uint64>(fake_packet_reorder_percentage_)) {
    DLOG(INFO) << "Reordering packets.";
    ++iter;
    // Swap the send times when re-ordering packets.
    delayed_packets_.begin()->send_time = iter->send_time;
  }

  DLOG(INFO) << "Releasing packet.  " << (delayed_packets_.size() - 1)
             << " remaining.";
  // Grab the next one off the queue and send it.
  writer()->WritePacket(iter->buffer.data(), iter->buffer.length(),
                        iter->self_address, iter->peer_address, NULL);
  DCHECK_GE(cur_buffer_size_, iter->buffer.length());
  cur_buffer_size_ -= iter->buffer.length();
  delayed_packets_.erase(iter);

  // If there are others, find the time for the next to be sent.
  if (delayed_packets_.empty()) {
    return QuicTime::Zero();
  }
  return delayed_packets_.begin()->send_time;
}

QuicTime PacketDroppingTestWriter::ReleaseOldPackets() {
  while (!delayed_packets_.empty()) {
    QuicTime next_send_time = delayed_packets_.front().send_time;
    if (next_send_time > clock_->Now()) {
      return next_send_time;
    }
    ReleaseNextPacket();
  }
  return QuicTime::Zero();
}

PacketDroppingTestWriter::DelayedWrite::DelayedWrite(
    const char* buffer,
    size_t buf_len,
    const net::IPAddressNumber& self_address,
    const net::IPEndPoint& peer_address,
    QuicTime send_time)
    : buffer(buffer, buf_len),
      self_address(self_address),
      peer_address(peer_address),
      send_time(send_time) {
}

PacketDroppingTestWriter::DelayedWrite::~DelayedWrite() {}

}  // namespace test
}  // namespace tools
}  // namespace net

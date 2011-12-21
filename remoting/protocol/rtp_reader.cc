// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_reader.h"

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"

namespace remoting {
namespace protocol {

namespace {
const int kInitialSequenceNumber = -1;

// Recommended values from RTP spec.
const int kMaxDropout = 3000;
const int kMaxMisorder = 100;
}  // namespace

RtpPacket::RtpPacket()
    : extended_sequence_number_(0) {
}

RtpPacket::~RtpPacket() { }

// RtpReader class.
RtpReader::RtpReader()
    : max_sequence_number_(0),
      wrap_around_count_(0),
      start_sequence_number_(kInitialSequenceNumber),
      total_packets_received_(0) {
}

RtpReader::~RtpReader() {
}

void RtpReader::Init(net::Socket* socket,
                     const OnMessageCallback& on_message_callback) {
  DCHECK(!on_message_callback.is_null());
  on_message_callback_ = on_message_callback;
  SocketReaderBase::Init(socket);
}

void RtpReader::OnDataReceived(net::IOBuffer* buffer, int data_size) {
  scoped_ptr<RtpPacket> packet(new RtpPacket());
  int header_size = UnpackRtpHeader(reinterpret_cast<uint8*>(buffer->data()),
                                    data_size, packet->mutable_header());
  if (header_size < 0) {
    LOG(WARNING) << "Received invalid RTP packet.";
    return;
  }

  int descriptor_size = UnpackVp8Descriptor(
      reinterpret_cast<uint8*>(buffer->data()) + header_size,
      data_size - header_size, packet->mutable_vp8_descriptor());
  if (descriptor_size < 0) {
    LOG(WARNING) << "Received RTP packet with an invalid VP8 descriptor.";
    return;
  }

  packet->mutable_payload()->Append(
      buffer, buffer->data() + header_size + descriptor_size,
      data_size - header_size - descriptor_size);

  uint16 sequence_number = packet->header().sequence_number;

  // Reset |start_sequence_number_| after we've received first packet.
  if (start_sequence_number_ == kInitialSequenceNumber) {
    start_sequence_number_ = sequence_number;
    max_sequence_number_ = sequence_number;
  }

  int16 delta = sequence_number - max_sequence_number_;
  if (delta <= -kMaxMisorder || delta > kMaxDropout) {
    // TODO(sergeyu): Do we need to handle restarted transmission?
    LOG(WARNING) << "Received RTP packet with invalid sequence number.";
    return;
  }

  packet->set_extended_sequence_number(
      (wrap_around_count_ << 16) + max_sequence_number_ + delta);

  if (delta > 0 && delta < kMaxDropout) {
    if (sequence_number < max_sequence_number_) {
      wrap_around_count_++;
    }
    max_sequence_number_ = sequence_number;
  }

  ++total_packets_received_;

  on_message_callback_.Run(packet.release());
}

void RtpReader::GetReceiverReport(RtcpReceiverReport* report) {
  int expected_packets = start_sequence_number_ >= 0 ?
      1 + max_sequence_number_ - start_sequence_number_ : 0;
  if (expected_packets > total_packets_received_) {
    report->total_lost_packets = expected_packets - total_packets_received_;
  } else {
    report->total_lost_packets = 0;
  }

  double loss_fraction = expected_packets > 0 ?
      report->total_lost_packets / expected_packets : 0.0L;
  DCHECK_GE(loss_fraction, 0.0);
  DCHECK_LE(loss_fraction, 1.0);
  report->loss_fraction = static_cast<uint8>(255 * loss_fraction);

  report->last_sequence_number = max_sequence_number_;

  // TODO(sergeyu): Implement jitter calculation.
  //
  // TODO(sergeyu): Set last_sender_report_timestamp and
  // last_sender_report_delay fields when sender reports are
  // implemented.
}

}  // namespace protocol
}  // namespace remoting

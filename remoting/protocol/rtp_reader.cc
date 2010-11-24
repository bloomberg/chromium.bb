// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_reader.h"

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"

namespace remoting {
namespace protocol {

namespace {
// Recomended values from RTP spec.
const int kMaxDropout = 3000;
const int kMaxMisorder = 100;
}  // namespace

RtpPacket::RtpPacket() { }

RtpPacket::~RtpPacket() { }

// RtpReader class.
RtpReader::RtpReader()
    : max_sequence_number_(0),
      wrap_around_count_(0),
      started_(false) {
}

RtpReader::~RtpReader() {
}

void RtpReader::Init(net::Socket* socket,
                     OnMessageCallback* on_message_callback) {
  on_message_callback_.reset(on_message_callback);
  SocketReaderBase::Init(socket);
}

void RtpReader::OnDataReceived(net::IOBuffer* buffer, int data_size) {
  RtpPacket* packet = new RtpPacket();
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

  // Reset |max_sequence_number_| after we've received first packet.
  if (!started_) {
    started_ = true;
    max_sequence_number_ = sequence_number;
  }

  int16 delta = sequence_number - max_sequence_number_;
  if (delta <= -kMaxMisorder || delta > kMaxDropout) {
    // TODO(sergeyu): Do we need to handle restarted trasmission?
    LOG(WARNING) << "Received RTP packet with invalid sequence number.";
    delete packet;
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

  on_message_callback_->Run(packet);
}

}  // namespace protocol
}  // namespace remoting

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_reader.h"

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"

namespace remoting {
namespace protocol {

RtpPacket::RtpPacket() {}
RtpPacket::~RtpPacket() {}

// RtpReader class.
RtpReader::RtpReader() {}
RtpReader::~RtpReader() {}

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

  on_message_callback_->Run(packet);
}

}  // namespace protocol
}  // namespace remoting

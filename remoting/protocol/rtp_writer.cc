// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_writer.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/rtp_utils.h"

namespace remoting {
namespace protocol {

namespace {
const uint8 kRtpPayloadTypePrivate = 96;
}  // namespace

RtpWriter::RtpWriter()
    : rtp_socket_(NULL),
      rtcp_socket_(NULL),
      last_packet_number_(0) {
}

RtpWriter::~RtpWriter() { }

// Initializes the writer. Must be called on the thread the sockets belong
// to.
void RtpWriter::Init(net::Socket* rtp_socket, net::Socket* rtcp_socket) {
  buffered_rtp_writer_ = new BufferedDatagramWriter();
  buffered_rtp_writer_->Init(rtp_socket, NULL);
  rtp_socket_ = rtp_socket;
  rtcp_socket_ = rtcp_socket;
}

void RtpWriter::SendPacket(uint32 timestamp, bool marker,
                           const Vp8Descriptor& vp8_descriptor,
                           const CompoundBuffer& payload) {
  RtpHeader header;
  header.padding = false;
  header.extension = false;
  header.sources = 0;
  header.marker = marker;
  header.payload_type = kRtpPayloadTypePrivate;
  header.timestamp = timestamp;

  // TODO(sergeyu): RTP requires that SSRC is chosen randomly by each
  // participant. There are only two participants in chromoting session,
  // so SSRC isn't useful. Implement it in future if neccessary.
  header.sync_source_id = 0;

  // TODO(sergeyu): Handle sequence number wrapping.
  header.sequence_number = last_packet_number_;
  ++last_packet_number_;

  int header_size = GetRtpHeaderSize(header);
  int vp8_descriptor_size = GetVp8DescriptorSize(vp8_descriptor);
  int payload_size = payload.total_bytes();
  int total_size = header_size + vp8_descriptor_size + payload_size;

  net::IOBufferWithSize* buffer = new net::IOBufferWithSize(total_size);

  // Pack header.
  PackRtpHeader(header, reinterpret_cast<uint8*>(buffer->data()),
                header_size);

  // Pack VP8 descriptor.
  PackVp8Descriptor(vp8_descriptor,
                    reinterpret_cast<uint8*>(buffer->data()) + header_size,
                    vp8_descriptor_size);

  // Copy payload to the buffer.
  payload.CopyTo(buffer->data() + header_size + vp8_descriptor_size,
                 payload_size);

  // And write the packet.
  buffered_rtp_writer_->Write(buffer);
}

int RtpWriter::GetPendingPackets() {
  return buffered_rtp_writer_->GetBufferChunks();
}

// Stop writing and drop pending data. Must be called from the same thread as
// Init().
void RtpWriter::Close() {
  buffered_rtp_writer_->Close();
}

}  // namespace protocol
}  // namespace remoting

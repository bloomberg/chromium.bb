// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_READER_H_
#define REMOTING_PROTOCOL_RTP_READER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/rtp_utils.h"
#include "remoting/protocol/socket_reader_base.h"

namespace remoting {
namespace protocol {

class RtpPacket {
 public:
  RtpPacket();
  ~RtpPacket();

  const RtpHeader& header() const { return header_; }
  RtpHeader* mutable_header() { return &header_; }

  const Vp8Descriptor& vp8_descriptor() const { return vp8_descriptor_; }
  Vp8Descriptor* mutable_vp8_descriptor() { return &vp8_descriptor_; }

  const CompoundBuffer& payload() const { return payload_; }
  CompoundBuffer* mutable_payload() { return &payload_; }

  uint32 extended_sequence_number() const {
    return extended_sequence_number_;
  }
  void set_extended_sequence_number(uint32 value) {
    extended_sequence_number_ = value;
  }

 private:
  RtpHeader header_;
  CompoundBuffer payload_;
  Vp8Descriptor vp8_descriptor_;
  uint32 extended_sequence_number_;
};

// RtpReader implements and RTP receiver. It reads packets from RTP
// socket, parses them, calculates extended sequence number and then
// passes them to a callback. It also collects statistics for RTCP
// receiver reports, but doesn't send any RTCP packets itself.
class RtpReader : public SocketReaderBase {
 public:
  RtpReader();
  virtual ~RtpReader();

  // The OnMessageCallback is called whenever a new message is received.
  // Ownership of the message is passed the callback.
  typedef base::Callback<void(const RtpPacket*)> OnMessageCallback;

  // Initialize the reader and start reading. Must be called on the thread
  // |socket| belongs to. The callback will be called when a new message is
  // received. RtpReader owns |on_message_callback|, doesn't own
  // |socket|.
  void Init(net::Socket* socket, const OnMessageCallback& on_message_callback);

  void GetReceiverReport(RtcpReceiverReport* report);

 protected:
  friend class RtpVideoReaderTest;

  virtual void OnDataReceived(net::IOBuffer* buffer, int data_size) OVERRIDE;

 private:
  OnMessageCallback on_message_callback_;

  uint16 max_sequence_number_;
  uint16 wrap_around_count_;
  int start_sequence_number_;
  int total_packets_received_;

  DISALLOW_COPY_AND_ASSIGN(RtpReader);
};

}  // namespace protocol
}  // namespace remoting


#endif  // REMOTING_PROTOCOL_RTP_READER_H_

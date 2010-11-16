// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_READER_H_
#define REMOTING_PROTOCOL_RTP_READER_H_

#include "base/scoped_ptr.h"
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

 private:
  RtpHeader header_;
  CompoundBuffer payload_;
  Vp8Descriptor vp8_descriptor_;
};

class RtpReader : public SocketReaderBase {
 public:
  RtpReader();
  ~RtpReader();

  // The OnMessageCallback is called whenever a new message is received.
  // Ownership of the message is passed the callback.
  typedef Callback1<const RtpPacket*>::Type OnMessageCallback;

  // Initialize the reader and start reading. Must be called on the thread
  // |socket| belongs to. The callback will be called when a new message is
  // received. RtpReader owns |on_message_callback|, doesn't own
  // |socket|.
  void Init(net::Socket* socket, OnMessageCallback* on_message_callback);

 protected:
  virtual void OnDataReceived(net::IOBuffer* buffer, int data_size);

 private:
  scoped_ptr<OnMessageCallback> on_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(RtpReader);
};

}  // namespace protocol
}  // namespace remoting


#endif  // REMOTING_PROTOCOL_RTP_READER_H_

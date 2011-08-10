// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_RTP_VIDEO_WRITER_H_

#include "remoting/protocol/rtp_writer.h"
#include "remoting/protocol/video_writer.h"

namespace remoting {
namespace protocol {

class Session;

class RtpVideoWriter : public VideoWriter {
 public:
  RtpVideoWriter();
  virtual ~RtpVideoWriter();

  // VideoWriter interface.
  virtual void Init(protocol::Session* session) OVERRIDE;
  virtual void Close() OVERRIDE;

  // VideoStub interface.
  virtual void ProcessVideoPacket(const VideoPacket* packet,
                                  Task* done) OVERRIDE;
  virtual int GetPendingPackets() OVERRIDE;

 private:
  RtpWriter rtp_writer_;

  DISALLOW_COPY_AND_ASSIGN(RtpVideoWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_VIDEO_WRITER_H_

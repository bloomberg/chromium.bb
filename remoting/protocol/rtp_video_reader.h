// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RTP_VIDEO_READER_H_
#define REMOTING_PROTOCOL_RTP_VIDEO_READER_H_

#include "remoting/protocol/rtp_reader.h"
#include "remoting/protocol/video_reader.h"

namespace remoting {

namespace protocol {
class Session;
}  // namespace protocol

class RtpVideoReader : public VideoReader {
 public:
  RtpVideoReader();
  virtual ~RtpVideoReader();

  // VideoReader interface.
  virtual void Init(protocol::Session* session, VideoStub* video_stub);
  virtual void Close();

 private:
  void OnRtpPacket(const RtpPacket& rtp_packet);

  RtpReader rtp_reader_;

  // The stub that processes all received packets.
  VideoStub* video_stub_;

  DISALLOW_COPY_AND_ASSIGN(RtpVideoReader);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_VIDEO_READER_H_

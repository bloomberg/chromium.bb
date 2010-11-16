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

class RtpVideoReader : public VideoReader {
 public:
  RtpVideoReader();
  virtual ~RtpVideoReader();

  // VideoReader interface.
  virtual void Init(protocol::Session* session, VideoStub* video_stub);
  virtual void Close();

 private:
  friend class RtpVideoReaderTest;

  typedef std::deque<const RtpPacket*> PacketsQueue;

  void OnRtpPacket(const RtpPacket* rtp_packet);
  void CheckFullPacket(PacketsQueue::iterator pos);
  void RebuildVideoPacket(PacketsQueue::iterator from,
                          PacketsQueue::iterator to);
  void ResetQueue();

  RtpReader rtp_reader_;

  PacketsQueue packets_queue_;
  uint32 last_sequence_number_;

  // The stub that processes all received packets.
  VideoStub* video_stub_;

  DISALLOW_COPY_AND_ASSIGN(RtpVideoReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RTP_VIDEO_READER_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoWriter is a generic interface for a video stream writer. RtpVideoWriter
// and ProtobufVideoWriter implement this interface for RTP and protobuf video
// streams. VideoWriter is used by ConnectionToClient to write into the video
// stream.

#ifndef REMOTING_PROTOCOL_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_VIDEO_WRITER_H_

#include "base/basictypes.h"
#include "remoting/proto/video.pb.h"

namespace remoting {

namespace protocol {
class Session;
}  // namespace protocol

class ChromotocolConfig;
class ChromotocolConnection;

// TODO(sergeyu): VideoWriter should implement VideoStub interface.
class VideoWriter {
 public:
  virtual ~VideoWriter();

  static VideoWriter* Create(const ChromotocolConfig* config);

  // Initializes the writer.
  virtual void Init(protocol::Session* session) = 0;

  // Sends the |packet|.
  virtual void SendPacket(const VideoPacket& packet) = 0;

  // Returns number of packets currently pending in the buffer.
  virtual int GetPendingPackets() = 0;

  virtual void Close() = 0;

 protected:
  VideoWriter() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoWriter);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_WRITER_H_

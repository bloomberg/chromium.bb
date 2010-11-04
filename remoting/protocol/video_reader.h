// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoReader is a generic interface for a video stream reader. RtpVideoReader
// and ProtobufVideoReader implement this interface for RTP and protobuf video
// streams. VideoReader is used by ConnectionToHost to read video stream.

#ifndef REMOTING_PROTOCOL_VIDEO_READER_H_
#define REMOTING_PROTOCOL_VIDEO_READER_H_

#include "base/callback.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {

namespace protocol {
class Session;
}  // namespace protocol

class ChromotocolConfig;
class ChromotocolConnection;

namespace protocol {

class VideoReader {
 public:
  static VideoReader* Create(const ChromotocolConfig* config);

  virtual ~VideoReader();

  // Initializies the reader. Doesn't take ownership of either |connection|
  // or |video_stub|.
  virtual void Init(protocol::Session* session,
                    VideoStub* video_stub) = 0;

  // Closes the reader. The stub should not be called after Close().
  virtual void Close() = 0;

 protected:
  VideoReader() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_READER_H_

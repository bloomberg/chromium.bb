// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoReader is a generic interface used by ConnectionToHost to read
// video stream. ProtobufVideoReader implements this interface for
// protobuf video stream.

#ifndef REMOTING_PROTOCOL_VIDEO_READER_H_
#define REMOTING_PROTOCOL_VIDEO_READER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

class Session;
class SessionConfig;

class VideoReader {
 public:
  // The callback is called when initialization is finished. The
  // parameter is set to true on success.
  typedef base::Callback<void(bool)> InitializedCallback;

  virtual ~VideoReader();

  static scoped_ptr<VideoReader> Create(const SessionConfig& config);

  // Initializies the reader. Doesn't take ownership of either |connection|
  // or |video_stub|.
  virtual void Init(Session* session,
                    VideoStub* video_stub,
                    const InitializedCallback& callback) = 0;
  virtual bool is_connected() = 0;

 protected:
  VideoReader() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_READER_H_

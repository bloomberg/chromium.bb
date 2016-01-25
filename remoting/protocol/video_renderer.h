// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_VIDEO_RENDERER_H_

namespace remoting {
namespace protocol {

class FrameConsumer;
class SessionConfig;
class VideoStub;

// VideoRenderer is responsible for decoding and displaying incoming video
// stream. This interface is used by ConnectionToHost implementations to
// render received video frames. ConnectionToHost may feed encoded frames to the
// VideoStub or decode them and pass decoded frames to the FrameConsumer.
//
// TODO(sergeyu): Reconsider this design.
class VideoRenderer {
 public:
  virtual ~VideoRenderer() {}

  // Configures the renderer with the supplied |config|. This must be called
  // exactly once before video data is supplied to the renderer.
  virtual void OnSessionConfig(const SessionConfig& config) = 0;

  // Returns the VideoStub interface of this renderer.
  virtual VideoStub* GetVideoStub() = 0;

  // Returns the FrameConsumer interface for this renderer.
  virtual FrameConsumer* GetFrameConsumer() = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_CLIENT_VIDEO_RENDERER_H_

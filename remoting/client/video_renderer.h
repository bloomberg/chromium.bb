// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_VIDEO_RENDERER_H_

namespace remoting {

namespace protocol {
class SessionConfig;
class VideoStub;
}  // namespace protocol;

// VideoRenderer is responsible for decoding and displaying incoming video
// stream.
class VideoRenderer {
 public:
  virtual ~VideoRenderer() {}

  // Configures the renderer with the supplied |config|. This must be called
  // exactly once before video data is supplied to the renderer.
  virtual void OnSessionConfig(const protocol::SessionConfig& config) = 0;

  // Returns the VideoStub interface of this renderer.
  virtual protocol::VideoStub* GetVideoStub() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_VIDEO_RENDERER_H_

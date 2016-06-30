// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_JNI_JNI_VIDEO_RENDERER_H_

#include <list>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/video_renderer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class ClientContext;
class ChromotingJniRuntime;
class JniClient;
class JniDisplayHandler;

// FrameConsumer and VideoRenderer implementation that draws onto a JNI direct
// byte buffer.
class JniVideoRenderer : public protocol::FrameConsumer,
                         public protocol::VideoRenderer {
 public:
  JniVideoRenderer(
      ChromotingJniRuntime* jni_runtime,
      base::WeakPtr<JniDisplayHandler> display);

  ~JniVideoRenderer() override;

  // FrameConsumer implementation.
  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;
  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 const base::Closure& done) override;
  PixelFormat GetPixelFormat() override;

  // JniVideoRenderer implementation.
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  protocol::VideoStub* GetVideoStub() override;
  protocol::FrameConsumer* GetFrameConsumer() override;
  bool Initialize(const ClientContext& client_context,
                  protocol::PerformanceTracker* perf_tracker) override;

 private:
  class Renderer;

  void OnFrameRendered(const base::Closure& done);

  // Used to obtain task runner references and make calls to Java methods.
  ChromotingJniRuntime* jni_runtime_;

  SoftwareVideoRenderer software_video_renderer_;

  // Renderer object used to render the frames on the display thread.
  std::unique_ptr<Renderer> renderer_;

  base::WeakPtrFactory<JniVideoRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JniVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_VIDEO_RENDERER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class VideoDecoder;

namespace protocol {
class FrameConsumer;
struct FrameStats;
class PerformanceTracker;
}  // namespace protocol

// Implementation of VideoRenderer interface that decodes frame on CPU (on a
// decode thread) and then passes decoded frames to a FrameConsumer.
class SoftwareVideoRenderer : public protocol::VideoRenderer,
                              public protocol::VideoStub {
 public:
  // The renderer can be created on any thread but afterwards all methods must
  // be called on the same thread.
  SoftwareVideoRenderer(protocol::FrameConsumer* consumer);

  // Deprecated constructor. TODO(yuweih): remove.
  // Constructs the renderer and initializes it immediately. Caller should not
  // call Initialize() after using this constructor.
  // All methods must be called on the same thread the renderer is created. The
  // |decode_task_runner_| is used to decode the video packets. |perf_tracker|
  // must outlive the renderer. |perf_tracker| may be nullptr, performance
  // tracking is disabled in that case.
  SoftwareVideoRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner,
      protocol::FrameConsumer* consumer,
      protocol::PerformanceTracker* perf_tracker);
  ~SoftwareVideoRenderer() override;

  // VideoRenderer interface.
  bool Initialize(const ClientContext& client_context,
                  protocol::PerformanceTracker* perf_tracker) override;
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  protocol::VideoStub* GetVideoStub() override;
  protocol::FrameConsumer* GetFrameConsumer() override;

  // protocol::VideoStub interface.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> packet,
                          const base::Closure& done) override;

 private:
  void RenderFrame(std::unique_ptr<protocol::FrameStats> stats,
                   const base::Closure& done,
                   std::unique_ptr<webrtc::DesktopFrame> frame);
  void OnFrameRendered(std::unique_ptr<protocol::FrameStats> stats,
                       const base::Closure& done);

  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;
  protocol::FrameConsumer* consumer_;
  protocol::PerformanceTracker* perf_tracker_ = nullptr;

  std::unique_ptr<VideoDecoder> decoder_;

  webrtc::DesktopSize source_size_;
  webrtc::DesktopVector source_dpi_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SoftwareVideoRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/video_renderer.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class FrameConsumer;
class VideoDecoder;

namespace protocol {
class PerformanceTracker;
}  // namespace protocol

// Implementation of VideoRenderer interface that decodes frame on CPU (on a
// decode thread) and then passes decoded frames to a FrameConsumer.
class SoftwareVideoRenderer : public VideoRenderer,
                              public protocol::VideoStub {
 public:
  // All methods must be called on the same thread the renderer is created. The
  // |decode_task_runner_| is used to decode the video packets. |perf_tracker|
  // must outlive the renderer. |perf_tracker| may be nullptr, performance
  // tracking is disabled in that case.
  SoftwareVideoRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner,
      FrameConsumer* consumer,
      protocol::PerformanceTracker* perf_tracker);
  ~SoftwareVideoRenderer() override;

  // VideoRenderer interface.
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  protocol::VideoStub* GetVideoStub() override;

  // protocol::VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done) override;

 private:
  void RenderFrame(int32_t frame_id,
                   const base::Closure& done,
                   scoped_ptr<webrtc::DesktopFrame> frame);
  void OnFrameRendered(int32_t frame_id, const base::Closure& done);

  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;
  FrameConsumer* consumer_;
  protocol::PerformanceTracker* perf_tracker_;

  scoped_ptr<VideoDecoder> decoder_;

  webrtc::DesktopSize source_size_;
  webrtc::DesktopVector source_dpi_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SoftwareVideoRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

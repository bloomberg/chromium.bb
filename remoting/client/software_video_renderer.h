// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/video_renderer.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc;

namespace remoting {

class ChromotingStats;
class FrameConsumer;
class VideoDecoder;

// Implementation of VideoRenderer interface that decodes frame on CPU (on a
// decode thread) and then passes decoded frames to a FrameConsumer.
class SoftwareVideoRenderer : public VideoRenderer,
                              public protocol::VideoStub {
 public:
  // Creates an update decoder on |main_task_runner_| and |decode_task_runner_|,
  // outputting to |consumer|. The |main_task_runner_| is responsible for
  // receiving and queueing packets. The |decode_task_runner_| is responsible
  // for decoding the video packets.
  SoftwareVideoRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner,
      FrameConsumer* consumer);
  ~SoftwareVideoRenderer() override;

  // VideoRenderer interface.
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  ChromotingStats* GetStats() override;
  protocol::VideoStub* GetVideoStub() override;

  // protocol::VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done) override;

 private:
  void RenderFrame(base::TimeTicks decode_start_time,
                 const base::Closure& done,
                 scoped_ptr<webrtc::DesktopFrame> frame);
  void OnFrameRendered(base::TimeTicks paint_start_time,
                       const base::Closure& done);

  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;
  FrameConsumer* consumer_;

  scoped_ptr<VideoDecoder> decoder_;

  webrtc::DesktopSize source_size_;
  webrtc::DesktopVector source_dpi_;

  ChromotingStats stats_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SoftwareVideoRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

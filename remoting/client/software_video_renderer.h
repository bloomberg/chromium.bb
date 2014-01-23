// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/frame_producer.h"
#include "remoting/client/video_renderer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ChromotingStats;

// Implementation of VideoRenderer interface that decodes frame on CPU (on a
// decode thread) and then passes decoded frames to a FrameConsumer.
// FrameProducer methods can be called on any thread. All other methods must be
// called on the main thread. Owned must ensure that this class outlives
// FrameConsumer (which calls FrameProducer interface).
class SoftwareVideoRenderer : public VideoRenderer,
                              public FrameProducer,
                              public base::NonThreadSafe {
 public:
  // Creates an update decoder on |main_task_runner_| and |decode_task_runner_|,
  // outputting to |consumer|. The |main_task_runner_| is responsible for
  // receiving and queueing packets. The |decode_task_runner_| is responsible
  // for decoding the video packets.
  // TODO(wez): Replace the ref-counted proxy with an owned FrameConsumer.
  SoftwareVideoRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner,
      scoped_refptr<FrameConsumerProxy> consumer);
  virtual ~SoftwareVideoRenderer();

  // VideoRenderer implementation.
  virtual void Initialize(const protocol::SessionConfig& config) OVERRIDE;
  virtual ChromotingStats* GetStats() OVERRIDE;
  virtual void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                  const base::Closure& done) OVERRIDE;

  // FrameProducer implementation. These methods may be called before we are
  // Initialize()d, or we know the source screen size. These methods may be
  // called on any thread.
  //
  // TODO(sergeyu): On Android a separate display thread is used for drawing.
  // FrameConsumer calls FrameProducer on that thread. Can we avoid having a
  // separate display thread? E.g. can we do everything on the decode thread?
  virtual void DrawBuffer(webrtc::DesktopFrame* buffer) OVERRIDE;
  virtual void InvalidateRegion(const webrtc::DesktopRegion& region) OVERRIDE;
  virtual void RequestReturnBuffers(const base::Closure& done) OVERRIDE;
  virtual void SetOutputSizeAndClip(
      const webrtc::DesktopSize& view_size,
      const webrtc::DesktopRect& clip_area) OVERRIDE;

 private:
  class Core;

  // Callback method when a VideoPacket is processed. |decode_start| contains
  // the timestamp when the packet will start to be processed.
  void OnPacketDone(base::Time decode_start, const base::Closure& done);

  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;
  scoped_ptr<Core> core_;

  ChromotingStats stats_;

  // Keep track of the most recent sequence number bounced back from the host.
  int64 latest_sequence_number_;

  base::WeakPtrFactory<SoftwareVideoRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

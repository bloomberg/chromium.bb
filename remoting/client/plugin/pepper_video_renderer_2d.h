// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_2D_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_2D_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/view.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/client/frame_consumer.h"
#include "remoting/client/plugin/pepper_video_renderer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace base {
class Time;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class FrameConsumerProxy;
class SoftwareVideoRenderer;

// Video renderer that wraps SoftwareVideoRenderer and displays it using Pepper
// 2D graphics API.
class PepperVideoRenderer2D : public PepperVideoRenderer,
                              public FrameConsumer,
                              public base::NonThreadSafe {
 public:
  PepperVideoRenderer2D();
  ~PepperVideoRenderer2D() override;

  // PepperVideoRenderer interface.
  bool Initialize(pp::Instance* instance,
                                const ClientContext& context,
                                EventHandler* event_handler) override;
  void OnViewChanged(const pp::View& view) override;
  void EnableDebugDirtyRegion(bool enable) override;

  // VideoRenderer interface.
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  ChromotingStats* GetStats() override;
  protocol::VideoStub* GetVideoStub() override;

 private:
  // FrameConsumer implementation.
  void ApplyBuffer(const webrtc::DesktopSize& view_size,
                   const webrtc::DesktopRect& clip_area,
                   webrtc::DesktopFrame* buffer,
                   const webrtc::DesktopRegion& region,
                   const webrtc::DesktopRegion& shape) override;
  void ReturnBuffer(webrtc::DesktopFrame* buffer) override;
  void SetSourceSize(const webrtc::DesktopSize& source_size,
                     const webrtc::DesktopVector& dpi) override;
  PixelFormat GetPixelFormat() override;

  // Helper to allocate buffers for the decoder.
  void AllocateBuffers();

  // Frees a frame buffer previously allocated by AllocateBuffer.
  void FreeBuffer(webrtc::DesktopFrame* buffer);

  // Renders the parts of |buffer| identified by |region| to the view.  If the
  // clip area of the view has changed since the buffer was generated then
  // FrameProducer is supplied the missed parts of |region|.  The FrameProducer
  // will be supplied a new buffer when FlushBuffer() completes.
  void FlushBuffer(const webrtc::DesktopRect& clip_area,
                   webrtc::DesktopFrame* buffer,
                   const webrtc::DesktopRegion& region);

  // Handles completion of FlushBuffer(), triggering a new buffer to be
  // returned to FrameProducer for rendering.
  void OnFlushDone(int result,
                   const base::Time& paint_start,
                   webrtc::DesktopFrame* buffer);

  // Parameters passed to Initialize().
  pp::Instance* instance_;
  EventHandler* event_handler_;

  pp::Graphics2D graphics2d_;

  scoped_refptr<FrameConsumerProxy> frame_consumer_proxy_;
  scoped_ptr<SoftwareVideoRenderer> software_video_renderer_;

  // List of allocated image buffers.
  std::list<webrtc::DesktopFrame*> buffers_;

  // Queued buffer to paint, with clip area and dirty region in device pixels.
  webrtc::DesktopFrame* merge_buffer_;
  webrtc::DesktopRect merge_clip_area_;
  webrtc::DesktopRegion merge_region_;

  // View size in Density Independent Pixels (DIPs).
  webrtc::DesktopSize dips_size_;

  // Scale factor from DIPs to device pixels.
  float dips_to_device_scale_;

  // View size in output pixels. This is the size at which FrameProducer must
  // render frames. It usually matches the DIPs size of the view, but may match
  // the size in device pixels when scaling is in effect, to reduce artefacts.
  webrtc::DesktopSize view_size_;

  // Scale factor from output pixels to device pixels.
  float dips_to_view_scale_;

  // Visible area of the view, in output pixels.
  webrtc::DesktopRect clip_area_;

  // Size of the most recent source frame in pixels.
  webrtc::DesktopSize source_size_;

  // Resolution of the most recent source frame dots-per-inch.
  webrtc::DesktopVector source_dpi_;

  // True if there is already a Flush() pending on the Graphics2D context.
  bool flush_pending_;

  // True after the first call to ApplyBuffer().
  bool frame_received_;

  // True if dirty regions are to be sent to |event_handler_| for debugging.
  bool debug_dirty_region_;

  pp::CompletionCallbackFactory<PepperVideoRenderer2D> callback_factory_;
  base::WeakPtrFactory<PepperVideoRenderer2D> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoRenderer2D);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_2D_H_

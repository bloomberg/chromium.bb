// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView for Pepper.  It is
// callable only on the Pepper thread.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include <list>

#include "base/compiler_specific.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/view.h"
#include "ppapi/cpp/point.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/client/frame_consumer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace base {
class Time;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class ChromotingInstance;
class ClientContext;
class FrameProducer;

class PepperView : public FrameConsumer {
 public:
  // Constructs a PepperView for the |instance|. The |instance| and |context|
  // must outlive this class.
  PepperView(ChromotingInstance* instance, ClientContext* context);
  virtual ~PepperView();

  // Allocates buffers and passes them to the FrameProducer to render into until
  // the maximum number of buffers are in-flight.
  void Initialize(FrameProducer* producer);

  // FrameConsumer implementation.
  virtual void ApplyBuffer(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           webrtc::DesktopFrame* buffer,
                           const webrtc::DesktopRegion& region,
                           const webrtc::DesktopRegion& shape) OVERRIDE;
  virtual void ReturnBuffer(webrtc::DesktopFrame* buffer) OVERRIDE;
  virtual void SetSourceSize(const webrtc::DesktopSize& source_size,
                             const webrtc::DesktopVector& dpi) OVERRIDE;
  virtual PixelFormat GetPixelFormat() OVERRIDE;

  // Updates the PepperView's size & clipping area, taking into account the
  // DIP-to-device scale factor.
  void SetView(const pp::View& view);

  // Returns the dimensions of the most recently displayed frame, in pixels.
  const webrtc::DesktopSize& get_source_size() const {
    return source_size_;
  }

 private:
  // Allocates a new frame buffer to supply to the FrameProducer to render into.
  // Returns NULL if the maximum number of buffers has already been allocated.
  webrtc::DesktopFrame* AllocateBuffer();

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

  // Reference to the creating plugin instance. Needed for interacting with
  // pepper.  Marking explicitly as const since it must be initialized at
  // object creation, and never change.
  ChromotingInstance* const instance_;

  // Context should be constant for the lifetime of the plugin.
  ClientContext* const context_;

  pp::Graphics2D graphics2d_;

  FrameProducer* producer_;

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

  // True after Initialize() has been called, until TearDown().
  bool is_initialized_;

  // True after the first call to ApplyBuffer().
  bool frame_received_;

  pp::CompletionCallbackFactory<PepperView> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

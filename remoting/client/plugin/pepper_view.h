// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is an implementation of the ChromotingView for Pepper.  It is
// callable only on the Pepper thread.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

#include <list>

#include "base/memory/weak_ptr.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/view.h"
#include "ppapi/cpp/point.h"
#include "remoting/client/frame_consumer.h"

namespace base {
class Time;
}  // namespace base

namespace remoting {

class ChromotingInstance;
class ClientContext;
class FrameProducer;

class PepperView : public FrameConsumer,
                   public base::SupportsWeakPtr<PepperView> {
 public:
  // Constructs a PepperView for the |instance|. The |instance|, |context|
  // and |producer| must outlive this class.
  PepperView(ChromotingInstance* instance,
             ClientContext* context,
             FrameProducer* producer);
  virtual ~PepperView();

  // FrameConsumer implementation.
  virtual void ApplyBuffer(const SkISize& view_size,
                           const SkIRect& clip_area,
                           pp::ImageData* buffer,
                           const SkRegion& region) OVERRIDE;
  virtual void ReturnBuffer(pp::ImageData* buffer) OVERRIDE;
  virtual void SetSourceSize(const SkISize& source_size,
                             const SkIPoint& dpi) OVERRIDE;

  // Updates the PepperView's size, clipping area and scale factor.
  void SetView(const pp::View& view);

  // Return the dimensions of the view and source in device pixels.
  const SkISize& get_view_size() const {
    return view_size_;
  }
  const SkISize& get_screen_size() const {
    return source_size_;
  }

  // Return the dimensions of the view in Density Independent Pixels (DIPs).
  // On high-DPI devices this will be smaller than the size in device pixels.
  const SkISize& get_view_size_dips() const {
    return view_size_dips_;
  }

 private:
  // This routine allocates an image buffer.
  pp::ImageData* AllocateBuffer();

  // This routine frees an image buffer allocated by AllocateBuffer().
  void FreeBuffer(pp::ImageData* buffer);

  // This routine makes sure that enough image buffers are in flight to keep
  // the decoding pipeline busy.
  void InitiateDrawing();

  // This routine applies the given image buffer to the screen taking into
  // account |clip_area| of the buffer and |region| describing valid parts
  // of the buffer.
  void FlushBuffer(const SkIRect& clip_area,
                   pp::ImageData* buffer,
                   const SkRegion& region);

  // This is a completion callback for FlushGraphics().
  void OnFlushDone(base::Time paint_start, pp::ImageData* buffer, int result);

  // Reference to the creating plugin instance. Needed for interacting with
  // pepper.  Marking explicitly as const since it must be initialized at
  // object creation, and never change.
  ChromotingInstance* const instance_;

  // Context should be constant for the lifetime of the plugin.
  ClientContext* const context_;

  pp::Graphics2D graphics2d_;

  FrameProducer* producer_;

  // List of allocated image buffers.
  std::list<pp::ImageData*> buffers_;

  // Queued buffer to paint, with clip area and dirty region in device pixels.
  pp::ImageData* merge_buffer_;
  SkIRect merge_clip_area_;
  SkRegion merge_region_;

  // View size, clip area and host dimensions, in device pixels.
  SkISize view_size_;
  SkIRect clip_area_;
  SkISize source_size_;

  // The DPI of the host screen.
  SkIPoint source_dpi_;

  // View size in Density-Independent pixels.
  SkISize view_size_dips_;

  // DIP-to-device pixel scale factor.
  float view_scale_;

  // True if there is already a Flush() pending on the Graphics2D context.
  bool flush_pending_;

  // True after Initialize() has been called, until TearDown().
  bool is_initialized_;

  // True after the first call to ApplyBuffer().
  bool frame_received_;

  DISALLOW_COPY_AND_ASSIGN(PepperView);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_H_

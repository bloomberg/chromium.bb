// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_

#include <IOSurface/IOSurfaceAPI.h>
#include <vector>

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#import "base/mac/scoped_nsobject.h"
#import "ui/accelerated_widget_mac/io_surface_layer.h"
#import "ui/accelerated_widget_mac/software_layer.h"
#include "ui/base/cocoa/remote_layer_api.h"
#endif  // __OBJC__

class SkCanvas;

namespace cc {
class SoftwareFrameData;
}

namespace ui {

class AcceleratedWidgetMac;

// A class through which an AcceleratedWidget may be bound to draw the contents
// of an NSView. An AcceleratedWidget may be bound to multiple different views
// throughout its lifetime (one at a time, though).
class AcceleratedWidgetMacNSView {
 public:
  virtual NSView* AcceleratedWidgetGetNSView() const = 0;
  virtual bool AcceleratedWidgetShouldIgnoreBackpressure() const = 0;
  virtual void AcceleratedWidgetSwapCompleted(
      const std::vector<ui::LatencyInfo>& latency_info) = 0;
  virtual void AcceleratedWidgetHitError() = 0;
};

#if defined(__OBJC__)

// AcceleratedWidgetMac owns a tree of CALayers. The widget may be passed
// to a ui::Compositor, which will cause, through its output surface, calls to
// GotAcceleratedFrame and GotSoftwareFrame. The CALayers may be installed
// in an NSView by setting the AcceleratedWidgetMacNSView for the helper.
class ACCELERATED_WIDGET_MAC_EXPORT AcceleratedWidgetMac
    : public IOSurfaceLayerClient {
 public:
  explicit AcceleratedWidgetMac(bool needs_gl_finish_workaround);
  virtual ~AcceleratedWidgetMac();

  gfx::AcceleratedWidget accelerated_widget() { return native_widget_; }

  void SetNSView(AcceleratedWidgetMacNSView* view);
  void ResetNSView();

  // Return true if the last frame swapped has a size in DIP of |dip_size|.
  bool HasFrameOfSize(const gfx::Size& dip_size) const;

  // Return the CGL renderer ID for the surface, if one is available.
  int GetRendererID() const;

  // Return true if the renderer should not be throttled by GPU back-pressure.
  bool IsRendererThrottlingDisabled() const;

  // Mark a bracket in which new frames are being pumped in a restricted nested
  // run loop.
  void BeginPumpingFrames();
  void EndPumpingFrames();

  void GotAcceleratedFrame(
      uint64 surface_handle,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::Size pixel_size,
      float scale_factor,
      const base::Closure& drawn_callback);

  void GotSoftwareFrame(float scale_factor, SkCanvas* canvas);

 private:
  // IOSurfaceLayerClient implementation:
  bool IOSurfaceLayerShouldAckImmediately() const override;
  void IOSurfaceLayerDidDrawFrame() override;
  void IOSurfaceLayerHitError() override;

  void GotAcceleratedCAContextFrame(
      CAContextID ca_context_id, gfx::Size pixel_size, float scale_factor);

  void GotAcceleratedIOSurfaceFrame(
      IOSurfaceID io_surface_id, gfx::Size pixel_size, float scale_factor);

  void AcknowledgeAcceleratedFrame();

  // Remove a layer from the heirarchy and destroy it. Because the accelerated
  // layer types may be replaced by a layer of the same type, the layer to
  // destroy is parameterized, and, if it is the current layer, the current
  // layer is reset.
  void DestroyCAContextLayer(
      base::scoped_nsobject<CALayerHost> ca_context_layer);
  void DestroyIOSurfaceLayer(
      base::scoped_nsobject<IOSurfaceLayer> io_surface_layer);
  void DestroySoftwareLayer();

  // The AcceleratedWidgetMacNSView that is using this as its internals.
  AcceleratedWidgetMacNSView* view_;

  // A phony NSView handle used to identify this.
  gfx::AcceleratedWidget native_widget_;

  // A flipped layer, which acts as the parent of the compositing and software
  // layers. This layer is flipped so that the we don't need to recompute the
  // origin for sub-layers when their position changes (this is impossible when
  // using remote layers, as their size change cannot be synchronized with the
  // window). This indirection is needed because flipping hosted layers (like
  // |background_layer_| of RenderWidgetHostViewCocoa) leads to unpredictable
  // behavior.
  base::scoped_nsobject<CALayer> flipped_layer_;

  // The accelerated CoreAnimation layer hosted by the GPU process.
  base::scoped_nsobject<CALayerHost> ca_context_layer_;

  // The locally drawn accelerated CoreAnimation layer.
  base::scoped_nsobject<IOSurfaceLayer> io_surface_layer_;

  // The locally drawn software layer.
  base::scoped_nsobject<SoftwareLayer> software_layer_;

  // If an accelerated frame has come in which has not yet been drawn and acked
  // then this is the latency info and the callback to make when the frame is
  // drawn. If there is no such frame then the callback is null.
  std::vector<ui::LatencyInfo> accelerated_latency_info_;
  base::Closure accelerated_frame_drawn_callback_;

  // The size in DIP of the last swap received from |compositor_|.
  gfx::Size last_swap_size_dip_;

  // Whether surfaces created by the widget should use the glFinish() workaround
  // after compositing.
  bool needs_gl_finish_workaround_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedWidgetMac);
};

#endif  // __OBJC__

ACCELERATED_WIDGET_MAC_EXPORT
void AcceleratedWidgetMacGotAcceleratedFrame(
    gfx::AcceleratedWidget widget, uint64 surface_handle,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor,
    const base::Closure& drawn_callback,
    bool* disable_throttling, int* renderer_id);

ACCELERATED_WIDGET_MAC_EXPORT
void AcceleratedWidgetMacGotSoftwareFrame(
    gfx::AcceleratedWidget widget, float scale_factor, SkCanvas* canvas);

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_

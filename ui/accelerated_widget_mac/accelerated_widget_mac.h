// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurface.h>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// A class through which an AcceleratedWidget may be bound to draw the contents
// of an NSView. An AcceleratedWidget may be bound to multiple different views
// throughout its lifetime (one at a time, though).
class AcceleratedWidgetMacNSView {
 public:
  virtual NSView* AcceleratedWidgetGetNSView() const = 0;
  virtual void AcceleratedWidgetGetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) const = 0;
  virtual void AcceleratedWidgetSwapCompleted() = 0;
};

// AcceleratedWidgetMac owns a tree of CALayers. The widget may be passed
// to a ui::Compositor, which will cause, through its output surface, calls to
// GotAcceleratedFrame and GotSoftwareFrame. The CALayers may be installed
// in an NSView by setting the AcceleratedWidgetMacNSView for the helper.
class ACCELERATED_WIDGET_MAC_EXPORT AcceleratedWidgetMac {
 public:
  AcceleratedWidgetMac();
  virtual ~AcceleratedWidgetMac();

  gfx::AcceleratedWidget accelerated_widget() { return native_widget_; }

  void SetNSView(AcceleratedWidgetMacNSView* view);
  void ResetNSView();

  // Return true if the last frame swapped has a size in DIP of |dip_size|.
  bool HasFrameOfSize(const gfx::Size& dip_size) const;

  // Populate the vsync parameters for the surface's display.
  void GetVSyncParameters(
      base::TimeTicks* timebase, base::TimeDelta* interval) const;

  static AcceleratedWidgetMac* Get(gfx::AcceleratedWidget widget);

  // Translate from a gfx::AcceleratedWidget to the NSView in which it will
  // appear. This may return nil if |widget| is invalid or is not currently
  // attached to an NSView.
  static NSView* GetNSView(gfx::AcceleratedWidget widget);

  void GotCALayerFrame(base::scoped_nsobject<CALayer> content_layer,
                       const gfx::Size& pixel_size,
                       float scale_factor);
  void GotIOSurfaceFrame(base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                         const gfx::Size& pixel_size,
                         float scale_factor);

 private:
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

  // A CALayer with content provided by the output surface.
  base::scoped_nsobject<CALayer> content_layer_;

  // A CALayer that has its content set to an IOSurface.
  base::scoped_nsobject<CALayer> io_surface_layer_;

  // The size in DIP of the last swap received from |compositor_|.
  gfx::Size last_swap_size_dip_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedWidgetMac);
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_H_

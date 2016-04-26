// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/scoped_cgl.h"

@interface CALayer (PrivateAPI)
- (void)setContentsChanged;
@end

namespace ui {
namespace {

typedef std::map<gfx::AcceleratedWidget,AcceleratedWidgetMac*>
    WidgetToHelperMap;
base::LazyInstance<WidgetToHelperMap> g_widget_to_helper_map;

AcceleratedWidgetMac* GetHelperFromAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  WidgetToHelperMap::const_iterator found =
      g_widget_to_helper_map.Pointer()->find(widget);
  // This can end up being accessed after the underlying widget has been
  // destroyed, but while the ui::Compositor is still being destroyed.
  // Return NULL in these cases.
  if (found == g_widget_to_helper_map.Pointer()->end())
    return NULL;
  return found->second;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMac

AcceleratedWidgetMac::AcceleratedWidgetMac() : view_(nullptr) {
  // Disable the fade-in animation as the layers are added.
  ScopedCAActionDisabler disabler;

  // Add a flipped transparent layer as a child, so that we don't need to
  // fiddle with the position of sub-layers -- they will always be at the
  // origin.
  flipped_layer_.reset([[CALayer alloc] init]);
  [flipped_layer_ setGeometryFlipped:YES];
  [flipped_layer_ setAnchorPoint:CGPointMake(0, 0)];
  [flipped_layer_
      setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];

  // Use a sequence number as the accelerated widget handle that we can use
  // to look up the internals structure.
  static intptr_t last_sequence_number = 0;
  last_sequence_number += 1;
  native_widget_ = reinterpret_cast<gfx::AcceleratedWidget>(
      last_sequence_number);
  g_widget_to_helper_map.Pointer()->insert(
      std::make_pair(native_widget_, this));
}

AcceleratedWidgetMac::~AcceleratedWidgetMac() {
  DCHECK(!view_);
  g_widget_to_helper_map.Pointer()->erase(native_widget_);
}

void AcceleratedWidgetMac::SetNSView(AcceleratedWidgetMacNSView* view) {
  // Disable the fade-in animation as the view is added.
  ScopedCAActionDisabler disabler;

  DCHECK(view && !view_);
  view_ = view;

  CALayer* background_layer = [view_->AcceleratedWidgetGetNSView() layer];
  DCHECK(background_layer);
  [flipped_layer_ setBounds:[background_layer bounds]];
  [background_layer addSublayer:flipped_layer_];
}

void AcceleratedWidgetMac::ResetNSView() {
  if (!view_)
    return;

  // Disable the fade-out animation as the view is removed.
  ScopedCAActionDisabler disabler;

  [flipped_layer_ removeFromSuperlayer];
  DestroyCAContextLayer(ca_context_layer_);
  DestroyLocalLayer();

  last_swap_size_dip_ = gfx::Size();
  view_ = NULL;
}

bool AcceleratedWidgetMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  return last_swap_size_dip_ == dip_size;
}

void AcceleratedWidgetMac::GetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) const {
  if (view_) {
    view_->AcceleratedWidgetGetVSyncParameters(timebase, interval);
  } else {
    *timebase = base::TimeTicks();
    *interval = base::TimeDelta();
  }
}

void AcceleratedWidgetMac::GotFrame(
    CAContextID ca_context_id,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor) {
  TRACE_EVENT0("ui", "AcceleratedWidgetMac::GotFrame");

  // If there is no view and therefore no superview to draw into, early-out.
  if (!view_) {
    TRACE_EVENT0("ui", "No associated NSView");
    return;
  }

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

  if (ca_context_id)
    GotCAContextFrame(ca_context_id, pixel_size, scale_factor);
  else
    GotIOSurfaceFrame(io_surface, pixel_size, scale_factor);

  view_->AcceleratedWidgetSwapCompleted();
}

void AcceleratedWidgetMac::GotCAContextFrame(CAContextID ca_context_id,
                                             const gfx::Size& pixel_size,
                                             float scale_factor) {
  TRACE_EVENT0("ui", "AcceleratedWidgetMac::GotCAContextFrame");

  // In the layer is replaced, keep the old one around until after the new one
  // is installed to avoid flashes.
  base::scoped_nsobject<CALayerHost> old_ca_context_layer =
      ca_context_layer_;

  // Create the layer to host the layer exported by the GPU process with this
  // particular CAContext ID.
  if ([ca_context_layer_ contextId] != ca_context_id) {
    TRACE_EVENT0("ui", "Creating a new CALayerHost");
    ca_context_layer_.reset([[CALayerHost alloc] init]);
    [ca_context_layer_ setContextId:ca_context_id];
    [ca_context_layer_
        setAutoresizingMask:kCALayerMaxXMargin|kCALayerMaxYMargin];
    [flipped_layer_ addSublayer:ca_context_layer_];
  }

  // If this replacing a same-type layer, remove it now that the new layer is
  // in the hierarchy.
  if (old_ca_context_layer != ca_context_layer_)
    DestroyCAContextLayer(old_ca_context_layer);

  // Remove any different-type layers that this is replacing.
  DestroyLocalLayer();
}

void AcceleratedWidgetMac::EnsureLocalLayer() {
  if (!local_layer_) {
    local_layer_.reset([[CALayer alloc] init]);
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [local_layer_ setContentsGravity:kCAGravityTopLeft];
    [local_layer_ setAnchorPoint:CGPointMake(0, 0)];
    [flipped_layer_ addSublayer:local_layer_];
  }
}

void AcceleratedWidgetMac::GotIOSurfaceFrame(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor) {
  TRACE_EVENT0("ui", "AcceleratedWidgetMac::GotIOSurfaceFrame");

  // If there is not a layer for local frames, create one.
  EnsureLocalLayer();

  id new_contents = static_cast<id>(io_surface.get());

  if (new_contents && new_contents == [local_layer_ contents]) {
    [local_layer_ setContentsChanged];
  } else {
    [local_layer_ setContents:new_contents];
  }

  [local_layer_ setBounds:CGRectMake(0, 0, pixel_size.width() / scale_factor,
                                     pixel_size.height() / scale_factor)];

  if ([local_layer_ contentsScale] != scale_factor)
    [local_layer_ setContentsScale:scale_factor];

  // Remove any different-type layers that this is replacing.
  DestroyCAContextLayer(ca_context_layer_);
}

void AcceleratedWidgetMac::DestroyCAContextLayer(
    base::scoped_nsobject<CALayerHost> ca_context_layer) {
  if (!ca_context_layer)
    return;
  [ca_context_layer removeFromSuperlayer];
  if (ca_context_layer == ca_context_layer_)
    ca_context_layer_.reset();
}

void AcceleratedWidgetMac::DestroyLocalLayer() {
  if (!local_layer_)
    return;
  [local_layer_ removeFromSuperlayer];
  local_layer_.reset();
}

void AcceleratedWidgetMacGotFrame(
    gfx::AcceleratedWidget widget,
    CAContextID ca_context_id,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor,
    base::TimeTicks* vsync_timebase,
    base::TimeDelta* vsync_interval) {
  if (vsync_timebase)
    *vsync_timebase = base::TimeTicks();
  if (vsync_interval)
    *vsync_interval = base::TimeDelta();

  AcceleratedWidgetMac* accelerated_widget_mac =
      GetHelperFromAcceleratedWidget(widget);

  if (accelerated_widget_mac) {
    accelerated_widget_mac->GotFrame(ca_context_id, io_surface, pixel_size,
                                     scale_factor);
    if (vsync_timebase && vsync_interval) {
      accelerated_widget_mac->GetVSyncParameters(vsync_timebase,
                                                 vsync_interval);
    }
  }
}

}  // namespace ui

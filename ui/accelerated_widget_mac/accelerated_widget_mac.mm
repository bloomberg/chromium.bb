// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
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

AcceleratedWidgetMac::AcceleratedWidgetMac(bool needs_gl_finish_workaround)
    : view_(NULL),
      needs_gl_finish_workaround_(needs_gl_finish_workaround) {
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

int AcceleratedWidgetMac::GetRendererID() const {
  return 0;
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

bool AcceleratedWidgetMac::IsRendererThrottlingDisabled() const {
  if (view_)
    return view_->AcceleratedWidgetShouldIgnoreBackpressure();
  return false;
}

void AcceleratedWidgetMac::BeginPumpingFrames() {
}

void AcceleratedWidgetMac::EndPumpingFrames() {
}

void AcceleratedWidgetMac::GotAcceleratedFrame(
    CAContextID ca_context_id,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const std::vector<ui::LatencyInfo>& latency_info,
    const gfx::Size& pixel_size,
    float scale_factor,
    const gfx::Rect& pixel_damage_rect,
    const base::Closure& drawn_callback) {
  // Record the surface and latency info to use when acknowledging this frame.
  DCHECK(accelerated_frame_drawn_callback_.is_null());
  accelerated_frame_drawn_callback_ = drawn_callback;
  accelerated_latency_info_.insert(accelerated_latency_info_.end(),
                                   latency_info.begin(), latency_info.end());

  // If there is no view and therefore no superview to draw into, early-out.
  if (!view_) {
    AcknowledgeAcceleratedFrame();
    return;
  }

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  if (ca_context_id)
    GotAcceleratedCAContextFrame(ca_context_id, pixel_size, scale_factor);
  else
    GotAcceleratedIOSurfaceFrame(io_surface, pixel_size, scale_factor);

  AcknowledgeAcceleratedFrame();
}

void AcceleratedWidgetMac::GotAcceleratedCAContextFrame(
    CAContextID ca_context_id,
    const gfx::Size& pixel_size,
    float scale_factor) {
  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

  // In the layer is replaced, keep the old one around until after the new one
  // is installed to avoid flashes.
  base::scoped_nsobject<CALayerHost> old_ca_context_layer =
      ca_context_layer_;

  // Create the layer to host the layer exported by the GPU process with this
  // particular CAContext ID.
  if ([ca_context_layer_ contextId] != ca_context_id) {
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

void AcceleratedWidgetMac::GotAcceleratedIOSurfaceFrame(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor) {
  GotIOSurfaceFrame(io_surface, pixel_size, scale_factor, false);
}

void AcceleratedWidgetMac::EnsureLocalLayer() {
  if (!local_layer_) {
    local_layer_.reset([[CALayer alloc] init]);
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [local_layer_ setContentsGravity:kCAGravityTopLeft];
    [flipped_layer_ addSublayer:local_layer_];
  }
}

void AcceleratedWidgetMac::GotIOSurfaceFrame(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor,
    bool flip_y) {
  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

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

  if ([local_layer_ respondsToSelector:(@selector(contentsScale))] &&
      [local_layer_ respondsToSelector:(@selector(setContentsScale:))] &&
      [local_layer_ contentsScale] != scale_factor) {
    DCHECK(base::mac::IsOSLionOrLater());
    [local_layer_ setContentsScale:scale_factor];
  }

  if (flip_y) {
    [local_layer_ setAnchorPoint:CGPointMake(0, 1)];
    [local_layer_ setAffineTransform:CGAffineTransformMakeScale(1.0, -1.0)];
  } else {
    [local_layer_ setAnchorPoint:CGPointMake(0, 0)];
    [local_layer_ setAffineTransform:CGAffineTransformMakeScale(1.0, 1.0)];
  }

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

void AcceleratedWidgetMac::AcknowledgeAcceleratedFrame() {
  if (accelerated_frame_drawn_callback_.is_null())
    return;
  accelerated_frame_drawn_callback_.Run();
  accelerated_frame_drawn_callback_.Reset();
  if (view_)
    view_->AcceleratedWidgetSwapCompleted(accelerated_latency_info_);
  accelerated_latency_info_.clear();
}

void AcceleratedWidgetMacGotAcceleratedFrame(
    gfx::AcceleratedWidget widget,
    CAContextID ca_context_id,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const std::vector<ui::LatencyInfo>& latency_info,
    const gfx::Size& pixel_size,
    float scale_factor,
    const gfx::Rect& pixel_damage_rect,
    const base::Closure& drawn_callback,
    bool* disable_throttling,
    int* renderer_id,
    base::TimeTicks* vsync_timebase,
    base::TimeDelta* vsync_interval) {
  AcceleratedWidgetMac* accelerated_widget_mac =
      GetHelperFromAcceleratedWidget(widget);
  if (accelerated_widget_mac) {
    accelerated_widget_mac->GotAcceleratedFrame(
        ca_context_id, io_surface, latency_info, pixel_size, scale_factor,
        pixel_damage_rect, drawn_callback);
    *disable_throttling =
        accelerated_widget_mac->IsRendererThrottlingDisabled();
    *renderer_id = accelerated_widget_mac->GetRendererID();
    accelerated_widget_mac->GetVSyncParameters(vsync_timebase, vsync_interval);
  } else {
    *disable_throttling = false;
    *renderer_id = 0;
    *vsync_timebase = base::TimeTicks();
    *vsync_interval = base::TimeDelta();
  }
}

void AcceleratedWidgetMacGotIOSurfaceFrame(
    gfx::AcceleratedWidget widget,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Size& pixel_size,
    float scale_factor,
    bool flip_y) {
  AcceleratedWidgetMac* accelerated_widget_mac =
      GetHelperFromAcceleratedWidget(widget);
  if (accelerated_widget_mac) {
    accelerated_widget_mac->GotIOSurfaceFrame(io_surface, pixel_size,
                                              scale_factor, flip_y);
  }
}

}  // namespace ui

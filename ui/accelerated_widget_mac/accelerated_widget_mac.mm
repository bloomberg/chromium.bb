// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/accelerated_widget_mac/io_surface_layer.h"
#include "ui/accelerated_widget_mac/surface_handle_types.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/scoped_cgl.h"

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
  static uintptr_t last_sequence_number = 0;
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
  DestroyIOSurfaceLayer(io_surface_layer_);
  DestroyCAContextLayer(ca_context_layer_);
  DestroySoftwareLayer();

  last_swap_size_dip_ = gfx::Size();
  view_ = NULL;
}

bool AcceleratedWidgetMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  return last_swap_size_dip_ == dip_size;
}

int AcceleratedWidgetMac::GetRendererID() const {
  if (io_surface_layer_)
    return [io_surface_layer_ rendererID];
  return 0;
}

bool AcceleratedWidgetMac::IsRendererThrottlingDisabled() const {
  if (view_)
    return view_->AcceleratedWidgetShouldIgnoreBackpressure();
  return false;
}

void AcceleratedWidgetMac::BeginPumpingFrames() {
  [io_surface_layer_ beginPumpingFrames];
}

void AcceleratedWidgetMac::EndPumpingFrames() {
  [io_surface_layer_ endPumpingFrames];
}

void AcceleratedWidgetMac::GotAcceleratedFrame(
    uint64 surface_handle,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor,
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

  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);
  switch (GetSurfaceHandleType(surface_handle)) {
    case kSurfaceHandleTypeIOSurface: {
      IOSurfaceID io_surface_id = IOSurfaceIDFromSurfaceHandle(surface_handle);
      GotAcceleratedIOSurfaceFrame(io_surface_id, pixel_size, scale_factor);
      break;
    }
    case kSurfaceHandleTypeCAContext: {
      CAContextID ca_context_id = CAContextIDFromSurfaceHandle(surface_handle);
      GotAcceleratedCAContextFrame(ca_context_id, pixel_size, scale_factor);
      break;
    }
    default:
      LOG(ERROR) << "Unrecognized accelerated frame type.";
      return;
  }
}

void AcceleratedWidgetMac::GotAcceleratedCAContextFrame(
    CAContextID ca_context_id,
    gfx::Size pixel_size,
    float scale_factor) {
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

  // Acknowledge the frame to unblock the compositor immediately (the GPU
  // process will do any required throttling).
  AcknowledgeAcceleratedFrame();

  // If this replacing a same-type layer, remove it now that the new layer is
  // in the hierarchy.
  if (old_ca_context_layer != ca_context_layer_)
    DestroyCAContextLayer(old_ca_context_layer);

  // Remove any different-type layers that this is replacing.
  DestroyIOSurfaceLayer(io_surface_layer_);
  DestroySoftwareLayer();
}

void AcceleratedWidgetMac::GotAcceleratedIOSurfaceFrame(
    IOSurfaceID io_surface_id,
    gfx::Size pixel_size,
    float scale_factor) {
  // In the layer is replaced, keep the old one around until after the new one
  // is installed to avoid flashes.
  base::scoped_nsobject<IOSurfaceLayer> old_io_surface_layer =
      io_surface_layer_;

  // Create or re-create an IOSurface layer if needed. If there already exists
  // a layer but it has the wrong scale factor or it was poisoned, re-create the
  // layer.
  bool needs_new_layer =
      !io_surface_layer_ ||
      [io_surface_layer_ hasBeenPoisoned] ||
      [io_surface_layer_ scaleFactor] != scale_factor;
  if (needs_new_layer) {
    io_surface_layer_.reset(
        [[IOSurfaceLayer alloc] initWithClient:this
                               withScaleFactor:scale_factor
                       needsGLFinishWorkaround:needs_gl_finish_workaround_]);
    if (io_surface_layer_)
      [flipped_layer_ addSublayer:io_surface_layer_];
    else
      LOG(ERROR) << "Failed to create IOSurfaceLayer";
  }

  // Open the provided IOSurface.
  if (io_surface_layer_) {
    bool result = [io_surface_layer_ gotFrameWithIOSurface:io_surface_id
                                             withPixelSize:pixel_size
                                           withScaleFactor:scale_factor];
    if (!result) {
      DestroyIOSurfaceLayer(io_surface_layer_);
      LOG(ERROR) << "Failed open IOSurface in IOSurfaceLayer";
    }
  }

  // Give a final complaint if anything with the layer's creation went wrong.
  // This frame will appear blank, the compositor will try to create another,
  // and maybe that will go better.
  if (!io_surface_layer_) {
    LOG(ERROR) << "IOSurfaceLayer is nil, tab will be blank";
    IOSurfaceLayerHitError();
  }

  // Make the CALayer draw and set its size appropriately.
  if (io_surface_layer_) {
    [io_surface_layer_ gotNewFrame];

    // Set the bounds of the accelerated layer to match the size of the frame.
    // If the bounds changed, force the content to be displayed immediately.
    CGRect new_layer_bounds = CGRectMake(
        0, 0, last_swap_size_dip_.width(), last_swap_size_dip_.height());
    bool bounds_changed = !CGRectEqualToRect(
        new_layer_bounds, [io_surface_layer_ bounds]);
    [io_surface_layer_ setBounds:new_layer_bounds];
    if (bounds_changed)
      [io_surface_layer_ setNeedsDisplayAndDisplayAndAck];
  }

  // If this replacing a same-type layer, remove it now that the new layer is
  // in the hierarchy.
  if (old_io_surface_layer != io_surface_layer_)
    DestroyIOSurfaceLayer(old_io_surface_layer);

  // Remove any different-type layers that this is replacing.
  DestroyCAContextLayer(ca_context_layer_);
  DestroySoftwareLayer();
}

void AcceleratedWidgetMac::GotSoftwareFrame(float scale_factor,
                                            SkCanvas* canvas) {
  if (!canvas || !view_)
    return;

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  // If there is not a layer for software frames, create one.
  if (!software_layer_) {
    software_layer_.reset([[SoftwareLayer alloc] init]);
    [flipped_layer_ addSublayer:software_layer_];
  }

  // Set the software layer to draw the provided canvas.
  SkImageInfo info;
  size_t row_bytes;
  const void* pixels = canvas->peekPixels(&info, &row_bytes);
  gfx::Size pixel_size(info.fWidth, info.fHeight);
  [software_layer_ setContentsToData:pixels
                        withRowBytes:row_bytes
                       withPixelSize:pixel_size
                     withScaleFactor:scale_factor];
  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

  // Remove any different-type layers that this is replacing.
  DestroyCAContextLayer(ca_context_layer_);
  DestroyIOSurfaceLayer(io_surface_layer_);
}

void AcceleratedWidgetMac::DestroyCAContextLayer(
    base::scoped_nsobject<CALayerHost> ca_context_layer) {
  if (!ca_context_layer)
    return;
  [ca_context_layer removeFromSuperlayer];
  if (ca_context_layer == ca_context_layer_)
    ca_context_layer_.reset();
}

void AcceleratedWidgetMac::DestroyIOSurfaceLayer(
    base::scoped_nsobject<IOSurfaceLayer> io_surface_layer) {
  if (!io_surface_layer)
    return;
  [io_surface_layer resetClient];
  [io_surface_layer removeFromSuperlayer];
  if (io_surface_layer == io_surface_layer_)
    io_surface_layer_.reset();
}

void AcceleratedWidgetMac::DestroySoftwareLayer() {
  if (!software_layer_)
    return;
  [software_layer_ removeFromSuperlayer];
  software_layer_.reset();
}

bool AcceleratedWidgetMac::IOSurfaceLayerShouldAckImmediately() const {
  // If there is no view then the accelerated layer is not in the view
  // hierarchy and will never draw.
  if (!view_)
    return true;
  return view_->AcceleratedWidgetShouldIgnoreBackpressure();
}

void AcceleratedWidgetMac::IOSurfaceLayerDidDrawFrame() {
  AcknowledgeAcceleratedFrame();
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

void AcceleratedWidgetMac::IOSurfaceLayerHitError() {
  // Perform all acks that would have been done if the frame had succeeded, to
  // un-block the compositor and renderer.
  AcknowledgeAcceleratedFrame();

  // Poison the context being used and request a mulligan.
  [io_surface_layer_ poisonContextAndSharegroup];

  if (view_)
    view_->AcceleratedWidgetHitError();
}

void AcceleratedWidgetMacGotAcceleratedFrame(
    gfx::AcceleratedWidget widget, uint64 surface_handle,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor,
    const base::Closure& drawn_callback,
    bool* disable_throttling, int* renderer_id) {
  AcceleratedWidgetMac* accelerated_widget_mac =
      GetHelperFromAcceleratedWidget(widget);
  if (accelerated_widget_mac) {
    accelerated_widget_mac->GotAcceleratedFrame(
        surface_handle, latency_info, pixel_size, scale_factor, drawn_callback);
    *disable_throttling =
        accelerated_widget_mac->IsRendererThrottlingDisabled();
    *renderer_id = accelerated_widget_mac->GetRendererID();
  } else {
    *disable_throttling = false;
    *renderer_id = 0;
  }
}

void AcceleratedWidgetMacGotSoftwareFrame(
    gfx::AcceleratedWidget widget, float scale_factor, SkCanvas* canvas) {
  AcceleratedWidgetMac* accelerated_widget_mac =
      GetHelperFromAcceleratedWidget(widget);
  if (accelerated_widget_mac)
    accelerated_widget_mac->GotSoftwareFrame(scale_factor, canvas);
}

}  // namespace ui

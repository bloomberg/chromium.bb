// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"

#import <AVFoundation/AVFoundation.h>

#include "base/mac/mac_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/accelerated_widget_mac/availability_macros.h"
#include "ui/base/cocoa/animation_utils.h"

namespace ui {

namespace {
const uint64_t kFramesBeforeFlushingLowPowerLayer = 15;
}

CALayerTreeCoordinator::CALayerTreeCoordinator(
    bool allow_remote_layers,
    bool allow_av_sample_buffer_display_layer)
    : allow_remote_layers_(allow_remote_layers),
      allow_av_sample_buffer_display_layer_(
          allow_av_sample_buffer_display_layer) {
  if (allow_remote_layers_) {
    root_ca_layer_.reset([[CALayer alloc] init]);
    [root_ca_layer_ setGeometryFlipped:YES];
    [root_ca_layer_ setOpaque:YES];

    if (allow_av_sample_buffer_display_layer_) {
      fullscreen_low_power_layer_.reset(
          [[AVSampleBufferDisplayLayer109 alloc] init]);
    }
  }
}

CALayerTreeCoordinator::~CALayerTreeCoordinator() {}

void CALayerTreeCoordinator::Resize(const gfx::Size& pixel_size,
                                    float scale_factor) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
}

CARendererLayerTree* CALayerTreeCoordinator::GetPendingCARendererLayerTree() {
  if (!pending_ca_renderer_layer_tree_)
    pending_ca_renderer_layer_tree_.reset(new CARendererLayerTree(
        allow_av_sample_buffer_display_layer_, false));
  return pending_ca_renderer_layer_tree_.get();
}

void CALayerTreeCoordinator::CommitPendingTreesToCA(
    const gfx::Rect& pixel_damage_rect,
    bool* fullscreen_low_power_layer_valid) {
  *fullscreen_low_power_layer_valid = false;

  // Update the CALayer hierarchy.
  ScopedCAActionDisabler disabler;
  if (pending_ca_renderer_layer_tree_) {
    pending_ca_renderer_layer_tree_->CommitScheduledCALayers(
        root_ca_layer_.get(), std::move(current_ca_renderer_layer_tree_),
        scale_factor_);
    *fullscreen_low_power_layer_valid =
        pending_ca_renderer_layer_tree_->CommitFullscreenLowPowerLayer(
            fullscreen_low_power_layer_);
    current_ca_renderer_layer_tree_.swap(pending_ca_renderer_layer_tree_);
  } else {
    TRACE_EVENT0("gpu", "Blank frame: No overlays or CALayers");
    [root_ca_layer_ setSublayers:nil];
    current_ca_renderer_layer_tree_.reset();
  }

  // It is necessary to leave the last image up for a few extra frames to allow
  // a smooth switch between the normal and low-power NSWindows.
  if (*fullscreen_low_power_layer_valid)
    frames_since_low_power_layer_was_valid_ = 0;
  else
    frames_since_low_power_layer_was_valid_ += 1;
  if (frames_since_low_power_layer_was_valid_ ==
      kFramesBeforeFlushingLowPowerLayer)
    [fullscreen_low_power_layer_ flushAndRemoveImage];

  // Reset all state for the next frame.
  pending_ca_renderer_layer_tree_.reset();
}

CALayer* CALayerTreeCoordinator::GetCALayerForDisplay() const {
  DCHECK(allow_remote_layers_);
  return root_ca_layer_.get();
}

CALayer* CALayerTreeCoordinator::GetFullscreenLowPowerLayerForDisplay() const {
  return fullscreen_low_power_layer_.get();
}

IOSurfaceRef CALayerTreeCoordinator::GetIOSurfaceForDisplay() {
  DCHECK(!allow_remote_layers_);
  if (!current_ca_renderer_layer_tree_)
    return nullptr;
  return current_ca_renderer_layer_tree_->GetContentIOSurface();
}

}  // namespace ui

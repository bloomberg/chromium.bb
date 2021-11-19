// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create() {
  return base::WrapRefCounted(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : typeface_(SkTypeface::MakeFromName("Arial", SkFontStyle())) {
  if (!typeface_) {
    typeface_ = SkTypeface::MakeFromName("monospace", SkFontStyle::Bold());
  }
  DCHECK(typeface_.get());
  SetIsDrawable(true);
  UpdateDrawsContent(HasDrawableContent());
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer() = default;

void HeadsUpDisplayLayer::UpdateLocationAndSize(
    const gfx::Size& device_viewport,
    float device_scale_factor) {
  DCHECK(IsMutationAllowed());
  gfx::Size device_viewport_in_layout_pixels =
      gfx::Size(device_viewport.width() / device_scale_factor,
                device_viewport.height() / device_scale_factor);

  gfx::Size bounds;

  // If the HUD is not displaying full-viewport rects (e.g., it is showing the
  // Frame Rendering Stats), use a fixed size.
  constexpr int kDefaultHUDSize = 256;
  bounds.SetSize(kDefaultHUDSize, kDefaultHUDSize);

  if (layer_tree_host()->GetDebugState().ShowDebugRects()) {
    bounds = device_viewport_in_layout_pixels;
  } else if (layer_tree_host()->GetDebugState().show_web_vital_metrics ||
             layer_tree_host()->GetDebugState().show_smoothness_metrics) {
    // If the HUD is used to display performance metrics (which is on the right
    // hand side_, make sure the bounds has the correct width, with a fixed
    // height.
    bounds.set_width(device_viewport_in_layout_pixels.width());
    // Increase HUD layer height to make sure all the metrics are showing.
    bounds.set_height(kDefaultHUDSize * 2);
  }

  SetBounds(bounds);
}

bool HeadsUpDisplayLayer::HasDrawableContent() const {
  return true;
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id());
}

const std::vector<gfx::Rect>& HeadsUpDisplayLayer::LayoutShiftRects() const {
  return layout_shift_rects_;
}

void HeadsUpDisplayLayer::SetLayoutShiftRects(
    const std::vector<gfx::Rect>& rects) {
  DCHECK(IsMutationAllowed());
  layout_shift_rects_ = rects;
}

void HeadsUpDisplayLayer::UpdateWebVitalMetrics(
    std::unique_ptr<WebVitalMetrics> web_vital_metrics) {
  DCHECK(IsMutationAllowed());
  web_vital_metrics_ = std::move(web_vital_metrics);
}

void HeadsUpDisplayLayer::PushPropertiesTo(LayerImpl* layer,
                                           const CommitState& commit_state) {
  Layer::PushPropertiesTo(layer, commit_state);
  TRACE_EVENT0("cc", "HeadsUpDisplayLayer::PushPropertiesTo");
  HeadsUpDisplayLayerImpl* layer_impl =
      static_cast<HeadsUpDisplayLayerImpl*>(layer);

  layer_impl->SetHUDTypeface(typeface_);
  layer_impl->SetLayoutShiftRects(layout_shift_rects_);
  layout_shift_rects_.clear();
  if (web_vital_metrics_ && web_vital_metrics_->HasValue())
    layer_impl->SetWebVitalMetrics(std::move(web_vital_metrics_));
}

}  // namespace cc

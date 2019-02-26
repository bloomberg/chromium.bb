// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/ephemeral_tab_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "content/public/browser/android/compositor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"

namespace android {
// static
scoped_refptr<EphemeralTabLayer> EphemeralTabLayer::Create(
    ui::ResourceManager* resource_manager) {
  return base::WrapRefCounted(new EphemeralTabLayer(resource_manager));
}

void EphemeralTabLayer::SetProperties(
    int progress_bar_background_resource_id,
    int progress_bar_resource_id,
    float dp_to_px,
    const scoped_refptr<cc::Layer>& content_layer,
    float panel_x,
    float panel_y,
    float panel_width,
    float panel_height,
    float bar_margin_side,
    float bar_height,
    float text_opacity,
    bool bar_border_visible,
    float bar_border_height,
    bool bar_shadow_visible,
    float bar_shadow_opacity,
    bool progress_bar_visible,
    float progress_bar_height,
    float progress_bar_opacity,
    int progress_bar_completion) {
  // Round values to avoid pixel gap between layers.
  bar_height = floor(bar_height);
  float bar_top = 0.f;
  float bar_bottom = bar_top + bar_height;
  bool should_render_progress_bar =
      progress_bar_visible && progress_bar_opacity > 0.f;

  OverlayPanelLayer::SetProperties(
      dp_to_px, content_layer, bar_height, panel_x, panel_y, panel_width,
      panel_height, bar_margin_side, bar_height, 0.0f, text_opacity,
      bar_border_visible, bar_border_height, bar_shadow_visible,
      bar_shadow_opacity, 1.0f);

  // ---------------------------------------------------------------------------
  // Progress Bar
  // ---------------------------------------------------------------------------

  if (should_render_progress_bar) {
    ui::NinePatchResource* progress_bar_background_resource =
        ui::NinePatchResource::From(resource_manager_->GetResource(
            ui::ANDROID_RESOURCE_TYPE_STATIC,
            progress_bar_background_resource_id));
    ui::NinePatchResource* progress_bar_resource =
        ui::NinePatchResource::From(resource_manager_->GetResource(
            ui::ANDROID_RESOURCE_TYPE_STATIC, progress_bar_resource_id));

    DCHECK(progress_bar_background_resource);
    DCHECK(progress_bar_resource);

    // Progress Bar Background
    if (progress_bar_background_->parent() != layer_)
      layer_->AddChild(progress_bar_background_);

    float progress_bar_y = bar_bottom - progress_bar_height;
    gfx::Size progress_bar_background_size(panel_width, progress_bar_height);

    progress_bar_background_->SetUIResourceId(
        progress_bar_background_resource->ui_resource()->id());
    progress_bar_background_->SetBorder(
        progress_bar_background_resource->Border(progress_bar_background_size));
    progress_bar_background_->SetAperture(
        progress_bar_background_resource->aperture());
    progress_bar_background_->SetBounds(progress_bar_background_size);
    progress_bar_background_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_background_->SetOpacity(progress_bar_opacity);

    // Progress Bar
    if (progress_bar_->parent() != layer_)
      layer_->AddChild(progress_bar_);

    float progress_bar_width =
        floor(panel_width * progress_bar_completion / 100.f);
    gfx::Size progress_bar_size(progress_bar_width, progress_bar_height);
    progress_bar_->SetUIResourceId(progress_bar_resource->ui_resource()->id());
    progress_bar_->SetBorder(progress_bar_resource->Border(progress_bar_size));
    progress_bar_->SetAperture(progress_bar_resource->aperture());
    progress_bar_->SetBounds(progress_bar_size);
    progress_bar_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_->SetOpacity(progress_bar_opacity);
  } else {
    // Removes Progress Bar and its Background from the Layer Tree.
    if (progress_bar_background_.get() && progress_bar_background_->parent())
      progress_bar_background_->RemoveFromParent();

    if (progress_bar_.get() && progress_bar_->parent())
      progress_bar_->RemoveFromParent();
  }
}

EphemeralTabLayer::EphemeralTabLayer(ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      progress_bar_(cc::NinePatchLayer::Create()),
      progress_bar_background_(cc::NinePatchLayer::Create()) {
  progress_bar_background_->SetIsDrawable(true);
  progress_bar_background_->SetFillCenter(true);
  progress_bar_->SetIsDrawable(true);
  progress_bar_->SetFillCenter(true);
}

EphemeralTabLayer::~EphemeralTabLayer() {}

}  //  namespace android

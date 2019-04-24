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
    int title_view_resource_id,
    int caption_view_resource_id,
    jfloat caption_animation_percentage,
    jfloat text_layer_min_height,
    jfloat title_caption_spacing,
    jboolean caption_visible,
    int progress_bar_background_resource_id,
    int progress_bar_resource_id,
    float dp_to_px,
    const scoped_refptr<cc::Layer>& content_layer,
    float panel_x,
    float panel_y,
    float panel_width,
    float panel_height,
    int bar_background_color,
    float bar_margin_side,
    float bar_height,
    bool bar_border_visible,
    float bar_border_height,
    bool bar_shadow_visible,
    float bar_shadow_opacity,
    int icon_color,
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

  // Title needs no rendering in the base layer as it can be rendered
  // together with caption below. Make it invisible.
  float title_opacity = 0.f;
  OverlayPanelLayer::SetProperties(
      dp_to_px, content_layer, bar_height, panel_x, panel_y, panel_width,
      panel_height, bar_background_color, bar_margin_side, bar_height, 0.0f,
      title_opacity, bar_border_visible, bar_border_height, bar_shadow_visible,
      bar_shadow_opacity, icon_color, 1.0f /* icon opacity */);

  SetupTextLayer(bar_top, bar_height, text_layer_min_height,
                 caption_view_resource_id, caption_animation_percentage,
                 caption_visible, title_view_resource_id,
                 title_caption_spacing);

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

void EphemeralTabLayer::SetupTextLayer(float bar_top,
                                       float bar_height,
                                       float text_layer_min_height,
                                       int caption_resource_id,
                                       float animation_percentage,
                                       bool caption_visible,
                                       int title_resource_id,
                                       float title_caption_spacing) {
  // ---------------------------------------------------------------------------
  // Setup the Drawing Hierarchy
  // ---------------------------------------------------------------------------

  DCHECK(text_layer_.get());
  DCHECK(caption_.get());
  DCHECK(title_.get());

  // Title
  ui::Resource* title_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, title_resource_id);
  if (title_resource) {
    title_->SetUIResourceId(title_resource->ui_resource()->id());
    title_->SetBounds(title_resource->size());
  }

  // Caption
  ui::Resource* caption_resource = nullptr;
  if (caption_visible) {
    // Grabs the dynamic Search Caption resource so we can get a snapshot.
    caption_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_DYNAMIC, caption_resource_id);
  }

  if (animation_percentage != 0.f) {
    if (caption_->parent() != text_layer_) {
      text_layer_->AddChild(caption_);
    }
    if (caption_resource) {
      caption_->SetUIResourceId(caption_resource->ui_resource()->id());
      caption_->SetBounds(caption_resource->size());
    }
  } else if (caption_->parent()) {
    caption_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Calculate Text Layer Size
  // ---------------------------------------------------------------------------
  // The caption_ may not have had its resource set by this point, if so
  // the bounds will be zero and everything will still work.
  float title_height = title_->bounds().height();
  float caption_height = caption_->bounds().height();

  float layer_height =
      std::max(text_layer_min_height,
               title_height + caption_height + title_caption_spacing);
  float layer_width =
      std::max(title_->bounds().width(), caption_->bounds().width());

  float layer_top = bar_top + (bar_height - layer_height) / 2;
  text_layer_->SetBounds(gfx::Size(layer_width, layer_height));
  text_layer_->SetPosition(gfx::PointF(0.f, layer_top));
  text_layer_->SetMasksToBounds(true);

  // ---------------------------------------------------------------------------
  // Layout Text Layer
  // ---------------------------------------------------------------------------
  // ---Top of Panel Bar---  <- bar_top
  //
  // ---Top of Text Layer--- <- layer_top
  //                         } remaining_height / 2
  // Title                   } title_height
  //                         } title_caption_spacing
  // Caption                 } caption_height
  //                         } remaining_height / 2
  // --Bottom of Text Layer-
  //
  // --Bottom of Panel Bar-
  // If the Caption is not visible the Title is centered in this space, when
  // the Caption becomes visible it is animated sliding up into it's position
  // with the spacings determined by UI.

  // If there is no caption, just vertically center the title.
  float title_top = (layer_height - title_height) / 2;

  // If we aren't displaying the caption we're done.
  if (animation_percentage == 0.f || !caption_resource) {
    title_->SetPosition(gfx::PointF(0.f, title_top));
    return;
  }

  // Calculate the positions for the Title and Caption when the Caption
  // animation is complete.
  float remaining_height =
      layer_height - title_height - title_caption_spacing - caption_height;

  float title_top_end = remaining_height / 2;
  float caption_top_end = title_top_end + title_height + title_caption_spacing;

  // Interpolate between the animation start and end positions (short cut
  // if the animation is at the end or start).
  title_top = title_top * (1.f - animation_percentage) +
              title_top_end * animation_percentage;

  // The Caption starts off the bottom of the Text Layer.
  float caption_top = layer_height * (1.f - animation_percentage) +
                      caption_top_end * animation_percentage;

  title_->SetPosition(gfx::PointF(0.f, title_top));
  caption_->SetPosition(gfx::PointF(0.f, caption_top));
}

EphemeralTabLayer::EphemeralTabLayer(ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      title_(cc::UIResourceLayer::Create()),
      caption_(cc::UIResourceLayer::Create()),
      text_layer_(cc::UIResourceLayer::Create()),
      progress_bar_(cc::NinePatchLayer::Create()),
      progress_bar_background_(cc::NinePatchLayer::Create()) {
  progress_bar_background_->SetIsDrawable(true);
  progress_bar_background_->SetFillCenter(true);
  progress_bar_->SetIsDrawable(true);
  progress_bar_->SetFillCenter(true);

  // Content layer
  text_layer_->SetIsDrawable(true);

  title_->SetIsDrawable(true);
  caption_->SetIsDrawable(true);

  AddBarTextLayer(text_layer_);
  text_layer_->AddChild(title_);
}

EphemeralTabLayer::~EphemeralTabLayer() {}

}  //  namespace android

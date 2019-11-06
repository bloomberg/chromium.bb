// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/ephemeral_tab_layer.h"

#include "base/task/cancelable_task_tracker.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace {

void OnLocalFaviconAvailable(
    scoped_refptr<cc::UIResourceLayer> layer,
    scoped_refptr<cc::UIResourceLayer> icon_layer,
    const float dp_to_px,
    const float panel_width,
    const float bar_height,
    const float padding,
    const favicon_base::FaviconRawBitmapResult& result) {
  if (!result.is_valid())
    return;
  SkBitmap favicon_bitmap;
  gfx::PNGCodec::Decode(result.bitmap_data->front(), result.bitmap_data->size(),
                        &favicon_bitmap);
  if (favicon_bitmap.isNull())
    return;
  const float icon_width =
      android::OverlayPanelLayer::kDefaultIconWidthDp * dp_to_px;
  favicon_bitmap.setImmutable();
  layer->SetBitmap(favicon_bitmap);
  layer->SetBounds(gfx::Size(icon_width, icon_width));

  bool is_rtl = l10n_util::IsLayoutRtl();
  float icon_x = is_rtl ? panel_width - icon_width - padding : padding;
  float icon_y = (bar_height - layer->bounds().height()) / 2;
  icon_layer->SetIsDrawable(false);
  layer->SetIsDrawable(true);
  layer->SetPosition(gfx::PointF(icon_x, icon_y));
}

}  // namespace

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
    float bar_margin_top,
    float bar_height,
    bool bar_border_visible,
    float bar_border_height,
    bool bar_shadow_visible,
    float bar_shadow_opacity,
    int icon_color,
    int drag_handlebar_color,
    bool progress_bar_visible,
    float progress_bar_height,
    float progress_bar_opacity,
    int progress_bar_completion) {
  // Round values to avoid pixel gap between layers.
  bar_height = floor(bar_height);
  float bar_top = 0.f;
  float bar_bottom = bar_top + bar_height;

  float title_opacity = 0.f;
  OverlayPanelLayer::SetProperties(
      dp_to_px, content_layer, bar_height, panel_x, panel_y, panel_width,
      panel_height, bar_background_color, bar_margin_side, bar_margin_top,
      bar_height, 0.0f, title_opacity, bar_border_visible, bar_border_height,
      bar_shadow_visible, bar_shadow_opacity, icon_color, drag_handlebar_color,
      1.0f /* icon opacity */);

  SetupTextLayer(bar_top, bar_height, text_layer_min_height,
                 caption_view_resource_id, caption_animation_percentage,
                 caption_visible, title_view_resource_id,
                 title_caption_spacing);

  OverlayPanelLayer::SetProgressBar(
      progress_bar_background_resource_id, progress_bar_resource_id,
      progress_bar_visible, bar_bottom, progress_bar_height,
      progress_bar_opacity, progress_bar_completion, panel_width);
  dp_to_px_ = dp_to_px;
  panel_width_ = panel_width;
  bar_height_ = bar_height;
  bar_margin_side_ = bar_margin_side;
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
  float title_height = title_->bounds().height();
  float caption_height = caption_visible ? caption_->bounds().height() : 0.f;

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
  //      Title              } title_height
  // --Bottom of Text Layer-
  //
  // --Bottom of Panel Bar-

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

void EphemeralTabLayer::GetLocalFaviconImageForURL(Profile* profile,
                                                   const std::string& url,
                                                   int size) {
  panel_icon_->SetIsDrawable(true);
  favicon_layer_->SetIsDrawable(false);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(favicon_service);
  if (!favicon_service)
    return;

  favicon_base::FaviconRawBitmapCallback callback_runner =
      base::BindOnce(&OnLocalFaviconAvailable, favicon_layer_, panel_icon_,
                     dp_to_px_, panel_width_, bar_height_, bar_margin_side_);

  // Set |fallback_to_host|=true so the favicon database will fall back to
  // matching only the hostname to have the best chance of finding a favicon.
  const bool fallback_to_host = true;
  favicon_service->GetRawFaviconForPageURL(
      GURL(url),
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
       favicon_base::IconType::kTouchPrecomposedIcon,
       favicon_base::IconType::kWebManifestIcon},
      size, fallback_to_host, std::move(callback_runner),
      cancelable_task_tracker_.get());
}

EphemeralTabLayer::EphemeralTabLayer(ui::ResourceManager* resource_manager)
    : OverlayPanelLayer(resource_manager),
      title_(cc::UIResourceLayer::Create()),
      caption_(cc::UIResourceLayer::Create()),
      favicon_layer_(cc::UIResourceLayer::Create()),
      text_layer_(cc::UIResourceLayer::Create()) {
  // Content layer
  text_layer_->SetIsDrawable(true);
  title_->SetIsDrawable(true);
  caption_->SetIsDrawable(true);

  AddBarTextLayer(text_layer_);
  text_layer_->AddChild(title_);

  favicon_layer_->SetIsDrawable(true);
  layer_->AddChild(favicon_layer_);
  cancelable_task_tracker_.reset(new base::CancelableTaskTracker());
}

EphemeralTabLayer::~EphemeralTabLayer() {}

}  //  namespace android

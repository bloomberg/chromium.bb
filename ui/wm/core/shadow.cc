// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow.h"

#include "base/lazy_instance.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace wm {

namespace {

// The opacity used for active shadow when animating between
// inactive/active shadow.
const float kInactiveShadowAnimationOpacity = 0.2f;

// Rounded corners are overdrawn on top of the window's content layer,
// we need to exclude them from the occlusion area.
const int kRoundedCornerRadius = 2;

// Duration for opacity animation in milliseconds.
const int kShadowAnimationDurationMs = 100;

struct ShadowDetails {
  // Description of the shadows.
  gfx::ShadowValues values;
  // Cached ninebox image based on |values|.
  gfx::ImageSkia ninebox_image;
};

// Map from elevation to a cached shadow.
using ShadowDetailsMap = std::map<int, ShadowDetails>;
base::LazyInstance<ShadowDetailsMap> g_shadow_cache = LAZY_INSTANCE_INITIALIZER;

const ShadowDetails& GetDetailsForElevation(int elevation) {
  auto iter = g_shadow_cache.Get().find(elevation);
  if (iter != g_shadow_cache.Get().end())
    return iter->second;

  auto insertion =
      g_shadow_cache.Get().insert(std::make_pair(elevation, ShadowDetails()));
  DCHECK(insertion.second);
  ShadowDetails* shadow = &insertion.first->second;
  // To match the CSS notion of blur (spread outside the bounding box) to the
  // Skia notion of blur (spread outside and inside the bounding box), we have
  // to double the designer-provided blur values.
  const int kBlurCorrection = 2;
  // "Key shadow": y offset is elevation and blur is twice the elevation.
  shadow->values.emplace_back(gfx::Vector2d(0, elevation),
                              kBlurCorrection * elevation * 2,
                              SkColorSetA(SK_ColorBLACK, 0x3d));
  // "Ambient shadow": no offset and blur matches the elevation.
  shadow->values.emplace_back(gfx::Vector2d(), kBlurCorrection * elevation,
                              SkColorSetA(SK_ColorBLACK, 0x1f));
  // To see what this looks like for elevation 24, try this CSS:
  //   box-shadow: 0 24px 48px rgba(0, 0, 0, .24),
  //               0 0 24px rgba(0, 0, 0, .12);
  shadow->ninebox_image = gfx::ImageSkiaOperations::CreateShadowNinebox(
      shadow->values, kRoundedCornerRadius);
  return *shadow;
}

}  // namespace

Shadow::Shadow() {}

Shadow::~Shadow() {}

void Shadow::Init(Style style) {
  style_ = style;

  layer_.reset(new ui::Layer(ui::LAYER_NOT_DRAWN));
  shadow_layer_.reset(new ui::Layer(ui::LAYER_NINE_PATCH));
  layer()->Add(shadow_layer_.get());

  UpdateImagesForStyle();
  shadow_layer_->set_name("Shadow");
  shadow_layer_->SetVisible(true);
  shadow_layer_->SetFillsBoundsOpaquely(false);
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  content_bounds_ = content_bounds;
  UpdateLayerBounds();
}

void Shadow::SetStyle(Style style) {
  if (style_ == style)
    return;

  Style old_style = style_;
  style_ = style;

  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  // If we're switching to or from the small style, don't bother with
  // animations.
  if (style == STYLE_SMALL || old_style == STYLE_SMALL) {
    UpdateImagesForStyle();
    // Make sure the shadow is fully opaque.
    shadow_layer_->SetOpacity(1.0f);
    return;
  }

  // If we're becoming active, switch images now.  Because the inactive image
  // has a very low opacity the switch isn't noticeable and this approach
  // allows us to use only a single set of shadow images at a time.
  if (style == STYLE_ACTIVE) {
    UpdateImagesForStyle();
    // Opacity was baked into inactive image, start opacity low to match.
    shadow_layer_->SetOpacity(kInactiveShadowAnimationOpacity);
  }

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(shadow_layer_->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    switch (style_) {
      case STYLE_ACTIVE:
        // Animate the active shadow from kInactiveShadowAnimationOpacity to
        // 1.0f.
        shadow_layer_->SetOpacity(1.0f);
        break;
      case STYLE_INACTIVE:
        // The opacity will be reset to 1.0f when animation is completed.
        shadow_layer_->SetOpacity(kInactiveShadowAnimationOpacity);
        break;
      default:
        NOTREACHED() << "Unhandled style " << style_;
        break;
    }
  }
}

void Shadow::OnImplicitAnimationsCompleted() {
  // If we just finished going inactive, switch images.  This doesn't cause
  // a visual pop because the inactive image opacity is so low.
  if (style_ == STYLE_INACTIVE) {
    UpdateImagesForStyle();
    // Opacity is baked into inactive image, so set fully opaque.
    shadow_layer_->SetOpacity(1.0f);
  }
}

void Shadow::UpdateImagesForStyle() {
  const ShadowDetails& details = GetDetailsForElevation(ElevationForStyle());
  shadow_layer_->UpdateNinePatchLayerImage(details.ninebox_image);
  // The ninebox grid is defined in terms of the image size. The shadow blurs in
  // both inward and outward directions from the edge of the contents, so the
  // aperture goes further inside the image than the shadow margins (which
  // represent exterior blur).
  gfx::Rect aperture(details.ninebox_image.size());
  gfx::Insets blur_region = gfx::ShadowValue::GetBlurRegion(details.values) +
                            gfx::Insets(kRoundedCornerRadius);
  aperture.Inset(blur_region);
  shadow_layer_->UpdateNinePatchLayerAperture(aperture);
  UpdateLayerBounds();
}

void Shadow::UpdateLayerBounds() {
  const ShadowDetails& details = GetDetailsForElevation(ElevationForStyle());
  // Shadow margins are negative, so this expands outwards from
  // |content_bounds_|.
  const gfx::Insets margins = gfx::ShadowValue::GetMargin(details.values);
  gfx::Rect layer_bounds = content_bounds_;
  layer_bounds.Inset(margins);
  layer()->SetBounds(layer_bounds);
  const gfx::Rect shadow_layer_bounds(layer_bounds.size());
  shadow_layer_->SetBounds(shadow_layer_bounds);

  // Occlude the region inside the bounding box. Occlusion uses shadow layer
  // space. See nine_patch_layer.h for more context on what's going on here.
  gfx::Rect occlusion_bounds = shadow_layer_bounds;
  occlusion_bounds.Inset(-margins + gfx::Insets(kRoundedCornerRadius));
  shadow_layer_->UpdateNinePatchOcclusion(occlusion_bounds);

  // The border is more or less the same inset as the aperture, but can be no
  // larger than the shadow layer. When the shadow layer is too small, shrink
  // the dimensions proportionally.
  gfx::Insets blur_region = gfx::ShadowValue::GetBlurRegion(details.values) +
                            gfx::Insets(kRoundedCornerRadius);
  int border_w = std::min(blur_region.width(), shadow_layer_bounds.width());
  int border_x = border_w * blur_region.left() / blur_region.width();
  int border_h = std::min(blur_region.height(), shadow_layer_bounds.height());
  int border_y = border_h * blur_region.top() / blur_region.height();
  shadow_layer_->UpdateNinePatchLayerBorder(
      gfx::Rect(border_x, border_y, border_w, border_h));
}

int Shadow::ElevationForStyle() {
  switch (style_) {
    case STYLE_ACTIVE:
      return 24;
    case STYLE_INACTIVE:
      return 8;
    case STYLE_SMALL:
      return 6;
  }
  NOTREACHED();
  return 0;
}

}  // namespace wm

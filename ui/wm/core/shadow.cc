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
  RecreateShadowLayer();
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  // When the window moves but doesn't change size, this is a no-op. (The
  // origin stays the same in this case.)
  if (content_bounds == content_bounds_)
    return;

  content_bounds_ = content_bounds;
  UpdateLayerBounds();
}

void Shadow::SetStyle(Style style) {
  if (style_ == style)
    return;

  style_ = style;

  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  // The old shadow layer is the new fading out layer.
  DCHECK(shadow_layer_);
  fading_layer_ = std::move(shadow_layer_);
  RecreateShadowLayer();
  shadow_layer_->SetOpacity(0.f);

  {
    // Observe the fade out animation so we can clean up the layer when done.
    ui::ScopedLayerAnimationSettings settings(fading_layer_->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    fading_layer_->SetOpacity(0.f);
  }

  {
    // We don't care to observe this one.
    ui::ScopedLayerAnimationSettings settings(shadow_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    shadow_layer_->SetOpacity(1.f);
  }
}

void Shadow::OnImplicitAnimationsCompleted() {
  fading_layer_.reset();
  // The size needed for layer() may be smaller now that |fading_layer_| is
  // removed.
  UpdateLayerBounds();
}

void Shadow::RecreateShadowLayer() {
  shadow_layer_.reset(new ui::Layer(ui::LAYER_NINE_PATCH));
  shadow_layer_->set_name("Shadow");
  shadow_layer_->SetVisible(true);
  shadow_layer_->SetFillsBoundsOpaquely(false);
  layer()->Add(shadow_layer_.get());

  const ShadowDetails& details = GetDetailsForElevation(ElevationForStyle());
  shadow_layer_->UpdateNinePatchLayerImage(details.ninebox_image);
  UpdateLayerBounds();
}

void Shadow::UpdateLayerBounds() {
  const ShadowDetails& details = GetDetailsForElevation(ElevationForStyle());
  // Shadow margins are negative, so this expands outwards from
  // |content_bounds_|.
  const gfx::Insets margins = gfx::ShadowValue::GetMargin(details.values);
  gfx::Rect new_layer_bounds = content_bounds_;
  new_layer_bounds.Inset(margins);
  gfx::Rect shadow_layer_bounds(new_layer_bounds.size());

  // When there's an old shadow fading out, the bounds of layer() have to be
  // big enough to encompass both shadows.
  if (fading_layer_) {
    const gfx::Rect old_layer_bounds = layer()->bounds();
    gfx::Rect combined_layer_bounds = old_layer_bounds;
    combined_layer_bounds.Union(new_layer_bounds);
    layer()->SetBounds(combined_layer_bounds);

    // If this is reached via SetContentBounds, we might hypothetically need
    // to change the size of the fading layer, but the fade is so fast it's
    // not really an issue.
    gfx::Rect fading_layer_bounds(fading_layer_->bounds());
    fading_layer_bounds.Offset(old_layer_bounds.origin() -
                               combined_layer_bounds.origin());
    fading_layer_->SetBounds(fading_layer_bounds);

    shadow_layer_bounds.Offset(new_layer_bounds.origin() -
                               combined_layer_bounds.origin());
  } else {
    layer()->SetBounds(new_layer_bounds);
  }

  shadow_layer_->SetBounds(shadow_layer_bounds);

  // Occlude the region inside the bounding box. Occlusion uses shadow layer
  // space. See nine_patch_layer.h for more context on what's going on here.
  gfx::Rect occlusion_bounds(shadow_layer_bounds.size());
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

  // The ninebox grid is defined in terms of the image size. The shadow blurs in
  // both inward and outward directions from the edge of the contents, so the
  // aperture goes further inside the image than the shadow margins (which
  // represent exterior blur).
  gfx::Rect aperture(details.ninebox_image.size());
  // The insets for the aperture are nominally |blur_region| but we need to
  // resize them if the contents are too small.
  // TODO(estade): by cutting out parts of ninebox, we lose the smooth
  // horizontal or vertical transition. This isn't very noticeable, but we may
  // need to address it by using a separate shadow layer for each ShadowValue,
  // by adjusting the shadow for very small windows, or other means.
  aperture.Inset(gfx::Insets(border_y, border_x, border_h - border_y,
                             border_w - border_x));
  shadow_layer_->UpdateNinePatchLayerAperture(aperture);
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

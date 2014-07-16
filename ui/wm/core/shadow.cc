// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow.h"

#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace {

// Shadow opacity for different styles.
const float kActiveShadowOpacity = 1.0f;
const float kInactiveShadowOpacity = 0.2f;
const float kSmallShadowOpacity = 1.0f;

// Shadow aperture for different styles.
// Note that this may be greater than interior inset to allow shadows with
// curved corners that extend inwards beyond a window's borders.
const int kActiveInteriorAperture = 134;
const int kInactiveInteriorAperture = 134;
const int kSmallInteriorAperture = 9;

// Interior inset for different styles.
const int kActiveInteriorInset = 64;
const int kInactiveInteriorInset = 64;
const int kSmallInteriorInset = 4;

// Duration for opacity animation in milliseconds.
const int kShadowAnimationDurationMs = 100;

float GetOpacityForStyle(wm::Shadow::Style style) {
  switch (style) {
    case wm::Shadow::STYLE_ACTIVE:
      return kActiveShadowOpacity;
    case wm::Shadow::STYLE_INACTIVE:
      return kInactiveShadowOpacity;
    case wm::Shadow::STYLE_SMALL:
      return kSmallShadowOpacity;
  }
  return 1.0f;
}

int GetShadowApertureForStyle(wm::Shadow::Style style) {
  switch (style) {
    case wm::Shadow::STYLE_ACTIVE:
      return kActiveInteriorAperture;
    case wm::Shadow::STYLE_INACTIVE:
      return kInactiveInteriorAperture;
    case wm::Shadow::STYLE_SMALL:
      return kSmallInteriorAperture;
  }
  return 0;
}

int GetInteriorInsetForStyle(wm::Shadow::Style style) {
  switch (style) {
    case wm::Shadow::STYLE_ACTIVE:
      return kActiveInteriorInset;
    case wm::Shadow::STYLE_INACTIVE:
      return kInactiveInteriorInset;
    case wm::Shadow::STYLE_SMALL:
      return kSmallInteriorInset;
  }
  return 0;
}

}  // namespace

namespace wm {

Shadow::Shadow() : style_(STYLE_ACTIVE), interior_inset_(0) {
}

Shadow::~Shadow() {
}

void Shadow::Init(Style style) {
  style_ = style;

  layer_.reset(new ui::Layer(ui::LAYER_NINE_PATCH));
  UpdateImagesForStyle();
  layer()->set_name("Shadow");
  layer()->SetVisible(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(GetOpacityForStyle(style_));
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
    layer()->SetOpacity(GetOpacityForStyle(style));
    return;
  }

  // If we're becoming active, switch images now.  Because the inactive image
  // has a very low opacity the switch isn't noticeable and this approach
  // allows us to use only a single set of shadow images at a time.
  if (style == STYLE_ACTIVE) {
    UpdateImagesForStyle();
    // Opacity was baked into inactive image, start opacity low to match.
    layer()->SetOpacity(kInactiveShadowOpacity);
  }

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
    switch (style_) {
      case STYLE_ACTIVE:
        layer()->SetOpacity(kActiveShadowOpacity);
        break;
      case STYLE_INACTIVE:
        layer()->SetOpacity(kInactiveShadowOpacity);
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
    layer()->SetOpacity(1.0f);
  }
}

void Shadow::UpdateImagesForStyle() {
  ResourceBundle& res = ResourceBundle::GetSharedInstance();
  gfx::Image image;
  switch (style_) {
    case STYLE_ACTIVE:
      image = res.GetImageNamed(IDR_AURA_SHADOW_ACTIVE);
      break;
    case STYLE_INACTIVE:
      image = res.GetImageNamed(IDR_AURA_SHADOW_INACTIVE);
      break;
    case STYLE_SMALL:
      image = res.GetImageNamed(IDR_WINDOW_BUBBLE_SHADOW_SMALL);
      break;
    default:
      NOTREACHED() << "Unhandled style " << style_;
      break;
  }

  // Calculate shadow aperture for style.
  int shadow_aperture = GetShadowApertureForStyle(style_);
  gfx::Rect aperture(shadow_aperture,
                     shadow_aperture,
                     image.Width() - shadow_aperture * 2,
                     image.Height() - shadow_aperture * 2);

  // Update nine-patch layer with new bitmap and aperture.
  layer()->UpdateNinePatchLayerBitmap(image.AsBitmap(), aperture);

  // Update interior inset for style.
  interior_inset_ = GetInteriorInsetForStyle(style_);

  // Image sizes may have changed.
  UpdateLayerBounds();
}

void Shadow::UpdateLayerBounds() {
  // Update bounds based on content bounds and interior inset.
  gfx::Rect layer_bounds = content_bounds_;
  layer_bounds.Inset(-interior_inset_, -interior_inset_);
  layer()->SetBounds(layer_bounds);

  // Calculate shadow border for style. Note that |border| is in layer space
  // and it cannot exceed the bounds of the layer.
  int shadow_aperture = GetShadowApertureForStyle(style_);
  int border_width = std::min(shadow_aperture * 2, layer_bounds.width());
  int border_height = std::min(shadow_aperture * 2, layer_bounds.height());
  gfx::Rect border(border_width / 2,
                   border_height / 2,
                   border_width,
                   border_height);
  layer()->UpdateNinePatchLayerBorder(border);
}

}  // namespace wm

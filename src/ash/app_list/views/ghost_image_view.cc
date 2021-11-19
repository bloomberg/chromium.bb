// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/ghost_image_view.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

constexpr int kGhostCircleStrokeWidth = 2;
constexpr int kGhostColorOpacity = 0x4D;  // 30% opacity.
constexpr base::TimeDelta kGhostFadeInOutLength = base::Milliseconds(180);
constexpr gfx::Tween::Type kGhostTween = gfx::Tween::FAST_OUT_SLOW_IN;

}  // namespace

GhostImageView::GhostImageView(GridIndex index)
    : is_hiding_(false), index_(index) {}

GhostImageView::~GhostImageView() {
  StopObservingImplicitAnimations();
}

void GhostImageView::Init(const AppListConfig* app_list_config,
                          const gfx::Rect& drop_target_bounds) {
  DCHECK(app_list_config);
  corner_radius_ = app_list_config->grid_focus_corner_radius();

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.0f);
  SetBoundsRect(drop_target_bounds);
}

void GhostImageView::FadeOut() {
  if (is_hiding_)
    return;
  is_hiding_ = true;
  DoAnimation(true /* fade out */);
}

void GhostImageView::FadeIn() {
  DoAnimation(false /* fade in */);
}

void GhostImageView::SetTransitionOffset(
    const gfx::Vector2d& transition_offset) {
  SetPosition(bounds().origin() + transition_offset);
}

void GhostImageView::DoAnimation(bool hide) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(kGhostFadeInOutLength);
  animation.SetTweenType(kGhostTween);

  if (hide) {
    animation.AddObserver(this);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(0.0f);
    return;
  }
  layer()->SetOpacity(1.0f);
}

void GhostImageView::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  // TODO(crbug.com/1255369): Get the highlight color from the ash color
  // provider once it is implemented.
  flags.setColor(AshColorProvider::Get()->IsDarkModeEnabled()
                     ? gfx::kGoogleGrey200
                     : gfx::kGoogleGrey700);
  flags.setAlpha(kGhostColorOpacity);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kGhostCircleStrokeWidth);
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(gfx::Insets(kGhostCircleStrokeWidth / 2));
  canvas->DrawRoundRect(gfx::RectF(bounds), corner_radius_, flags);
}

void GhostImageView::OnImplicitAnimationsCompleted() {
  // Delete this GhostImageView when the fade out animation is done.
  delete this;
}

BEGIN_METADATA(GhostImageView, views::View)
END_METADATA

}  // namespace ash

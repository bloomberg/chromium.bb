// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/arrow_button_view.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

// Arrow icon size.
constexpr int kArrowIconSizeDp = 20;
constexpr int kArrowIconBackroundRadius = 25;

constexpr const int kBorderForFocusRingDp = 3;

// How long does a single step of the loading animation take - i.e., the time it
// takes for the arc to grow from a point to a full circle.
constexpr base::TimeDelta kLoadingAnimationStepDuration =
    base::TimeDelta::FromSeconds(2);

void PaintLoadingArc(gfx::Canvas* canvas,
                     const gfx::Rect& bounds,
                     double loading_fraction) {
  gfx::Rect oval = bounds;
  // Inset to make sure the whole arc is inside the visible rect.
  oval.Inset(/*horizontal=*/1, /*vertical=*/1);

  SkPath path;
  path.arcTo(RectToSkRect(oval), /*startAngle=*/-90,
             /*sweepAngle=*/360 * loading_fraction, /*forceMoveTo=*/true);

  cc::PaintFlags flags;
  // Use the same color as the arrow icon.
  flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

}  // namespace

ArrowButtonView::ArrowButtonView(PressedCallback callback, int size)
    : LoginButton(std::move(callback)) {
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kBorderForFocusRingDp)));
  SetPreferredSize(gfx::Size(size + 2 * kBorderForFocusRingDp,
                             size + 2 * kBorderForFocusRingDp));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Layer rendering is needed for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  focus_ring()->SetPathGenerator(
      std::make_unique<views::FixedSizeCircleHighlightPathGenerator>(
          kArrowIconBackroundRadius));
}

ArrowButtonView::~ArrowButtonView() = default;

void ArrowButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  const gfx::Rect rect(GetContentsBounds());

  // Draw background.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), rect.width() / 2, flags);

  // Draw arrow icon.
  views::ImageButton::PaintButtonContents(canvas);

  // Draw the arc of the loading animation.
  if (loading_animation_)
    PaintLoadingArc(canvas, rect, loading_animation_->GetCurrentValue());
}

void ArrowButtonView::OnThemeChanged() {
  LoginButton::OnThemeChanged();
  AshColorProvider::Get()->DecorateIconButton(
      this, kLockScreenArrowIcon, /*toggled_=*/false, kArrowIconSizeDp);
}

void ArrowButtonView::EnableLoadingAnimation(bool enabled) {
  if (!enabled) {
    if (!loading_animation_)
      return;
    loading_animation_.reset();
    SchedulePaint();
    return;
  }

  if (loading_animation_)
    return;

  // Use MultiAnimation in order to have a continuously running analog of
  // LinearAnimation.
  loading_animation_ =
      std::make_unique<gfx::MultiAnimation>(gfx::MultiAnimation::Parts{
          gfx::MultiAnimation::Part(kLoadingAnimationStepDuration,
                                    gfx::Tween::LINEAR),
      });
  loading_animation_->set_delegate(&loading_animation_delegate_);
  loading_animation_->Start();
}

ArrowButtonView::LoadingAnimationDelegate::LoadingAnimationDelegate(
    ArrowButtonView* owner)
    : owner_(owner) {}

ArrowButtonView::LoadingAnimationDelegate::~LoadingAnimationDelegate() =
    default;

void ArrowButtonView::LoadingAnimationDelegate::AnimationProgressed(
    const gfx::Animation* /*animation*/) {
  owner_->SchedulePaint();
}

BEGIN_METADATA(ArrowButtonView, LoginButton)
END_METADATA

}  // namespace ash

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"

#include "ash/assistant/util/animation_util.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kDotCount = 3;
constexpr float kDotLargeSizeDip = 9.f;
constexpr float kDotSmallSizeDip = 6.f;
constexpr int kSpacingDip = 4;

// Animation.
constexpr base::TimeDelta kAnimationOffsetDuration =
    base::TimeDelta::FromMilliseconds(216);
constexpr base::TimeDelta kAnimationPauseDuration =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kAnimationScaleUpDuration =
    base::TimeDelta::FromMilliseconds(266);
constexpr base::TimeDelta kAnimationScaleDownDuration =
    base::TimeDelta::FromMilliseconds(450);

// Transformation.
constexpr float kScaleFactor = kDotLargeSizeDip / kDotSmallSizeDip;
constexpr float kTranslationDip = -(kDotLargeSizeDip - kDotSmallSizeDip) / 2.f;

// DotBackground ---------------------------------------------------------------

class DotBackground : public views::Background {
 public:
  DotBackground() = default;
  ~DotBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleGrey300);

    const gfx::Rect& b = view->GetContentsBounds();
    const gfx::Point center = gfx::Point(b.width() / 2, b.height() / 2);
    const int radius = std::min(b.width() / 2, b.height() / 2);

    canvas->DrawCircle(center, radius, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DotBackground);
};

}  // namespace

// AssistantProgressIndicator --------------------------------------------------

AssistantProgressIndicator::AssistantProgressIndicator() {
  InitLayout();
}

AssistantProgressIndicator::~AssistantProgressIndicator() = default;

void AssistantProgressIndicator::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), kSpacingDip));

  // Initialize dots.
  for (int i = 0; i < kDotCount; ++i) {
    views::View* dot_view = new views::View();
    dot_view->SetBackground(std::make_unique<DotBackground>());
    dot_view->SetPreferredSize(gfx::Size(kDotSmallSizeDip, kDotSmallSizeDip));

    // Dots will animate on their own layers.
    dot_view->SetPaintToLayer();
    dot_view->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(dot_view);
  }
}

void AssistantProgressIndicator::AddedToWidget() {
  VisibilityChanged(/*starting_from=*/this, /*is_visible=*/visible());
}

void AssistantProgressIndicator::RemovedFromWidget() {
  VisibilityChanged(/*starting_from=*/this, /*is_visible=*/false);
}

void AssistantProgressIndicator::OnLayerOpacityChanged(
    ui::PropertyChangeReason reason) {
  VisibilityChanged(/*starting_from=*/this,
                    /*is_visible=*/layer()->opacity() > 0.f);
}

void AssistantProgressIndicator::VisibilityChanged(views::View* starting_from,
                                                   bool is_visible) {
  if (is_visible == is_visible_)
    return;

  is_visible_ = is_visible;

  if (!is_visible_) {
    // Stop all animations.
    for (int i = 0; i < child_count(); ++i) {
      child_at(i)->layer()->GetAnimator()->StopAnimating();
    }
    return;
  }

  using namespace assistant::util;

  // The animation performs scaling on the child views. In order to give the
  // illusion that scaling is being performed about the center of the view as
  // the transformation origin, we also need to perform a translation.
  gfx::Transform transform;
  transform.Translate(kTranslationDip, kTranslationDip);
  transform.Scale(kScaleFactor, kScaleFactor);

  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);

    if (i > 0) {
      // Schedule the animations to start after an offset.
      view->layer()->GetAnimator()->SchedulePauseForProperties(
          i * kAnimationOffsetDuration,
          ui::LayerAnimationElement::AnimatableProperty::TRANSFORM);
    }

    // Schedule transformation animation.
    view->layer()->GetAnimator()->ScheduleAnimation(
        CreateLayerAnimationSequence(
            // Animate scale up.
            CreateTransformElement(transform, kAnimationScaleUpDuration),
            // Animate scale down.
            CreateTransformElement(gfx::Transform(),
                                   kAnimationScaleDownDuration),
            // Pause before next iteration.
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                kAnimationPauseDuration),
            // Animation parameters.
            {.is_cyclic = true}));
  }
}

}  // namespace ash
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/animated_icon_view.h"

#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"

namespace views {

AnimatedIconView::AnimatedIconView(const gfx::VectorIcon& icon)
    : icon_(icon),
      color_(gfx::kPlaceholderColor),
      duration_(gfx::GetDurationOfAnimation(icon)) {
  UpdateStaticImage();
}

AnimatedIconView::~AnimatedIconView() {}

void AnimatedIconView::SetColor(SkColor color) {
  if (color_ != color) {
    color_ = color;
    UpdateStaticImage();
  }
}

void AnimatedIconView::Animate(State target) {
  SetState(target);
  if (!IsAnimating())
    GetWidget()->GetCompositor()->AddAnimationObserver(this);
  start_time_ = base::TimeTicks::Now();
}

void AnimatedIconView::SetState(State state) {
  state_ = state;
  UpdateStaticImage();
}

void AnimatedIconView::OnPaint(gfx::Canvas* canvas) {
  if (!IsAnimating()) {
    views::ImageView::OnPaint(canvas);
    return;
  }

  auto timestamp = base::TimeTicks::Now();
  base::TimeDelta elapsed = timestamp - start_time_;
  if (state_ == END)
    elapsed = start_time_ + duration_ - timestamp;

  canvas->Translate(GetImageBounds().OffsetFromOrigin());
  gfx::PaintVectorIcon(canvas, icon_, color_, elapsed);
}

void AnimatedIconView::OnAnimationStep(base::TimeTicks timestamp) {
  base::TimeDelta elapsed = timestamp - start_time_;
  if (elapsed > duration_) {
    GetWidget()->GetCompositor()->RemoveAnimationObserver(this);
    start_time_ = base::TimeTicks();
  }

  SchedulePaint();
}

void AnimatedIconView::OnCompositingShuttingDown(ui::Compositor* compositor) {}

bool AnimatedIconView::IsAnimating() const {
  return start_time_ != base::TimeTicks();
}

void AnimatedIconView::UpdateStaticImage() {
  gfx::IconDescription description(
      icon_, 0, color_, state_ == START ? base::TimeDelta() : duration_,
      gfx::kNoneIcon);
  SetImage(gfx::CreateVectorIcon(description));
}

}  // namespace views

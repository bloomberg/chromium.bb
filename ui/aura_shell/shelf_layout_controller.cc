// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shelf_layout_controller.h"

#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/views/widget/widget.h"

namespace aura_shell {
namespace internal {

namespace {

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

ShelfLayoutController::ShelfLayoutController(views::Widget* launcher,
                                             views::Widget* status)
    : animating_(false),
      visible_(true),
      max_height_(-1),
      launcher_(launcher),
      status_(status) {
  gfx::Rect launcher_bounds = launcher->GetWindowScreenBounds();
  gfx::Rect status_bounds = status->GetWindowScreenBounds();
  max_height_ = std::max(launcher_bounds.height(), status_bounds.height());
  GetLayer(launcher)->GetAnimator()->AddObserver(this);
}

ShelfLayoutController::~ShelfLayoutController() {
  GetLayer(launcher_)->GetAnimator()->RemoveObserver(this);
}

void ShelfLayoutController::LayoutShelf() {
  StopAnimating();
  TargetBounds target_bounds;
  float target_opacity = visible_ ? 1.0f : 0.0f;
  CalculateTargetBounds(visible_, &target_bounds);
  GetLayer(launcher_)->SetOpacity(target_opacity);
  GetLayer(status_)->SetOpacity(target_opacity);
  launcher_->SetBounds(target_bounds.launcher_bounds);
  status_->SetBounds(target_bounds.status_bounds);
  aura::Desktop::GetInstance()->screen()->set_work_area_insets(
      target_bounds.work_area_insets);
}

void ShelfLayoutController::SetVisible(bool visible) {
  bool current_visibility = animating_ ? !visible_ : visible_;
  if (visible == current_visibility)
    return;  // Nothing changed.

  StopAnimating();

  TargetBounds target_bounds;
  float target_opacity = visible ? 1.0f : 0.0f;
  CalculateTargetBounds(visible, &target_bounds);
  AnimateWidgetTo(launcher_, target_bounds.launcher_bounds, target_opacity);
  AnimateWidgetTo(status_, target_bounds.status_bounds, target_opacity);
  animating_ = true;
  // |visible_| is updated once the animation completes.
}

void ShelfLayoutController::StopAnimating() {
  if (animating_) {
    animating_ = false;
    visible_ = !visible_;
  }
  GetLayer(launcher_)->GetAnimator()->StopAnimating();
}

void ShelfLayoutController::CalculateTargetBounds(bool visible,
                                                  TargetBounds* target_bounds) {
  const gfx::Rect& available_bounds(aura::Desktop::GetInstance()->bounds());
  int y = available_bounds.bottom() - (visible ? max_height_ : 0);
  gfx::Rect status_bounds(status_->GetWindowScreenBounds());
  target_bounds->status_bounds = gfx::Rect(
      available_bounds.right() - status_bounds.width(),
      y + (max_height_ - status_bounds.height()) / 2,
      status_bounds.width(), status_bounds.height());
  gfx::Rect launcher_bounds(launcher_->GetWindowScreenBounds());
  target_bounds->launcher_bounds = gfx::Rect(
      available_bounds.x(), y + (max_height_ - launcher_bounds.height()) / 2,
      available_bounds.width() - status_bounds.width(),
      launcher_bounds.height());
  if (visible)
    target_bounds->work_area_insets = gfx::Insets(0, 0, max_height_, 0);
}

void ShelfLayoutController::AnimateWidgetTo(views::Widget* widget,
                                            const gfx::Rect& target_bounds,
                                            float target_opacity) {
  ui::Layer* layer = GetLayer(widget);
  ui::LayerAnimator::ScopedSettings animation_setter(layer->GetAnimator());
  widget->SetBounds(target_bounds);
  layer->SetOpacity(target_opacity);
}

void ShelfLayoutController::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* sequence) {
  if (!animating_)
    return;
  animating_ = false;
  visible_ = !visible_;
  TargetBounds target_bounds;
  CalculateTargetBounds(visible_, &target_bounds);
  aura::Desktop::GetInstance()->screen()->set_work_area_insets(
      target_bounds.work_area_insets);
}

}  // internal
}  // aura_shell

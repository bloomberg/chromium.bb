// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_view.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Animating in will start (after resize stage) when animation value is greater
// than this value.
constexpr double kAnimatingInStartValue = 0.5;

// Animating out will end (before resize stage) when animation value is less
// than this value.
constexpr double kAnimatingOutEndValue = 0.5;

}  // namespace

void IconizedLabel::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (custom_accessible_name_.empty())
    return Label::GetAccessibleNodeData(node_data);

  node_data->role = ax::mojom::Role::kStaticText;
  node_data->SetName(custom_accessible_name_);
}

TrayItemView::TrayItemView(Shelf* shelf)
    : views::AnimationDelegateViews(this),
      shelf_(shelf),
      label_(NULL),
      image_view_(NULL) {
  DCHECK(shelf_);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

TrayItemView::~TrayItemView() = default;

void TrayItemView::CreateLabel() {
  label_ = new IconizedLabel;
  AddChildView(label_);
  PreferredSizeChanged();
}

void TrayItemView::CreateImageView() {
  image_view_ = new views::ImageView;
  AddChildView(image_view_);
  PreferredSizeChanged();
}

void TrayItemView::SetVisible(bool set_visible) {
  if (!GetWidget() ||
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    views::View::SetVisible(set_visible);
    return;
  }

  // Do not invoke animation when visibility is not changing.
  if (set_visible == GetVisible())
    return;

  target_visible_ = set_visible;

  if (!animation_) {
    animation_ = std::make_unique<gfx::SlideAnimation>(this);
    animation_->SetTweenType(gfx::Tween::LINEAR);
    animation_->Reset(GetVisible() ? 1.0 : 0.0);
  }

  if (target_visible_) {
    animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(400));
    animation_->Show();
    AnimationProgressed(animation_.get());
    views::View::SetVisible(true);
    layer()->SetOpacity(0.f);
  } else {
    animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(100));
    animation_->Hide();
    AnimationProgressed(animation_.get());
  }
}

bool TrayItemView::IsHorizontalAlignment() const {
  return shelf_->IsHorizontalAlignment();
}

gfx::Size TrayItemView::CalculatePreferredSize() const {
  DCHECK_EQ(1u, children().size());
  gfx::Size size = views::View::CalculatePreferredSize();
  if (image_view_) {
    size = gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize);
  }

  if (!animation_.get() || !animation_->is_animating() ||
      !InResizeAnimation(animation_->GetCurrentValue())) {
    return size;
  }

  double progress = gfx::Tween::CalculateValue(
      gfx::Tween::FAST_OUT_SLOW_IN,
      GetResizeProgressFromAnimationProgress(animation_->GetCurrentValue()));
  if (shelf_->IsHorizontalAlignment()) {
    size.set_width(std::max(1, static_cast<int>(size.width() * progress)));
  } else {
    size.set_height(std::max(1, static_cast<int>(size.height() * progress)));
  }
  return size;
}

int TrayItemView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

const char* TrayItemView::GetClassName() const {
  return "TrayItemView";
}

void TrayItemView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void TrayItemView::AnimationProgressed(const gfx::Animation* animation) {
  // Should not animate during resize stage.
  if (InResizeAnimation(animation->GetCurrentValue())) {
    PreferredSizeChanged();
    return;
  }

  double scale_progress =
      GetItemScaleProgressFromAnimationProgress(animation->GetCurrentValue());
  layer()->SetOpacity(scale_progress);

  // Only scale when animating icon in.
  if (target_visible_ && use_scale_in_animation_) {
    scale_progress = gfx::Tween::CalculateValue(gfx::Tween::LINEAR_OUT_SLOW_IN,
                                                scale_progress);
    gfx::Transform transform;
    transform.Translate(
        gfx::Tween::DoubleValueBetween(scale_progress,
                                       static_cast<double>(width()) / 2, 0.),
        gfx::Tween::DoubleValueBetween(scale_progress,
                                       static_cast<double>(height()) / 2, 0.));
    transform.Scale(scale_progress, scale_progress);
    layer()->SetTransform(transform);
  }

  // Container size might not fully transition to full size (the resize progress
  // value converted from animation progress might not be 1 after resize
  // animation). This call makes sure that it is fully resized.
  PreferredSizeChanged();
}

void TrayItemView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() < 0.1)
    views::View::SetVisible(false);
}

void TrayItemView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

bool TrayItemView::InResizeAnimation(double animation_value) const {
  // Animation should be delayed for the first part of animating in and last
  // part of animating out, allowing item resize happen before item animating in
  // and after item animating out.
  return ((target_visible_ && animation_value <= kAnimatingInStartValue) ||
          (!target_visible_ && animation_value <= kAnimatingOutEndValue));
}

double TrayItemView::GetResizeProgressFromAnimationProgress(
    double animation_value) const {
  DCHECK(InResizeAnimation(animation_value));
  // When animating in, convert value from [0,kAnimatingInStartValue] to [0,1].
  if (target_visible_)
    return animation_value * (1 / kAnimatingInStartValue);

  // When animating out, convert value from [kAnimatingOutEndValue,0] to [1,0].
  return animation_value * (1 / kAnimatingOutEndValue);
}

double TrayItemView::GetItemScaleProgressFromAnimationProgress(
    double animation_value) const {
  DCHECK(!InResizeAnimation(animation_value));
  // When animating in, convert value from [kAnimatingInStartValue,1] to [0,1].
  if (target_visible_) {
    return (animation_value - kAnimatingInStartValue) *
           (1 / (1 - kAnimatingInStartValue));
  }

  // When animating out, convert value from [1,kAnimatingOutEndValue] to [1,0].
  return (animation_value - kAnimatingOutEndValue) *
         (1 / (1 - kAnimatingOutEndValue));
}

}  // namespace ash

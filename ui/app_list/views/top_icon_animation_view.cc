// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/top_icon_animation_view.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"

namespace app_list {

TopIconAnimationView::TopIconAnimationView(const gfx::ImageSkia& icon,
                                           const gfx::Rect& scaled_rect,
                                           bool open_folder)
    : icon_size_(kGridIconDimension, kGridIconDimension),
      icon_(new views::ImageView),
      scaled_rect_(scaled_rect),
      open_folder_(open_folder) {
  DCHECK(!icon.isNull());
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon,
      skia::ImageOperations::RESIZE_BEST, icon_size_));
  icon_->SetImage(resized);
  AddChildView(icon_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

TopIconAnimationView::~TopIconAnimationView() {
  // Required due to RequiresNotificationWhenAnimatorDestroyed() returning true.
  // See ui::LayerAnimationObserver for details.
  StopObservingImplicitAnimations();
}

void TopIconAnimationView::AddObserver(TopIconAnimationObserver* observer) {
  observers_.AddObserver(observer);
}

void TopIconAnimationView::RemoveObserver(TopIconAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TopIconAnimationView::TransformView() {
  // This view will delete itself on animation completion.
  set_owned_by_client();

  // Transform used for scaling down the icon and move it back inside to the
  // original folder icon. The transform's origin is this view's origin.
  gfx::Transform transform;
  transform.Translate(scaled_rect_.x() - bounds().x(),
                      scaled_rect_.y() - bounds().y());
  transform.Scale(
      static_cast<double>(scaled_rect_.width()) / bounds().width(),
      static_cast<double>(scaled_rect_.height()) / bounds().height());

  if (open_folder_) {
    // Transform to a scaled down icon inside the original folder icon.
    layer()->SetTransform(transform);
  }

  // Animate the icon to its target location and scale when opening or
  // closing a folder.
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.AddObserver(this);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFolderTransitionInDurationMs));
  layer()->SetTransform(open_folder_ ? gfx::Transform() : transform);
}

gfx::Size TopIconAnimationView::CalculatePreferredSize() const {
  return icon_size_;
}

void TopIconAnimationView::Layout() {
  icon_->SetBoundsRect(GetContentsBounds());
}

void TopIconAnimationView::OnImplicitAnimationsCompleted() {
  SetVisible(false);
  for (auto& observer : observers_)
    observer.OnTopIconAnimationsComplete();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

bool TopIconAnimationView::RequiresNotificationWhenAnimatorDestroyed() const {
  return true;
}

}  // namespace app_list

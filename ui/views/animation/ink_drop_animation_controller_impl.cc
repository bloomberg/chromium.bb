// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/ink_drop_host.h"

namespace views {

InkDropAnimationControllerImpl::InkDropAnimationControllerImpl(
    InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host) {}

InkDropAnimationControllerImpl::~InkDropAnimationControllerImpl() {
  if (ink_drop_animation_) {
    // TODO(bruthig): Change this to be called when the ink drop becomes hidden.
    // See www.crbug.com/522175.
    ink_drop_host_->RemoveInkDropLayer(ink_drop_animation_->root_layer());
  }
}

InkDropState InkDropAnimationControllerImpl::GetInkDropState() const {
  if (!ink_drop_animation_)
    return InkDropState::HIDDEN;
  return ink_drop_animation_->ink_drop_state();
}

void InkDropAnimationControllerImpl::AnimateToState(InkDropState state) {
  if (!ink_drop_animation_)
    CreateInkDropAnimation();
  ink_drop_animation_->AnimateToState(state);
}

gfx::Size InkDropAnimationControllerImpl::GetInkDropLargeSize() const {
  return ink_drop_large_size_;
}

void InkDropAnimationControllerImpl::SetInkDropSize(const gfx::Size& large_size,
                                                    int large_corner_radius,
                                                    const gfx::Size& small_size,
                                                    int small_corner_radius) {
  // TODO(bruthig): Fix the ink drop animations to work for non-square sizes.
  DCHECK_EQ(large_size.width(), large_size.height())
      << "The ink drop animation does not currently support non-square sizes.";
  DCHECK_EQ(small_size.width(), small_size.height())
      << "The ink drop animation does not currently support non-square sizes.";
  ink_drop_large_size_ = large_size;
  ink_drop_large_corner_radius_ = large_corner_radius;
  ink_drop_small_size_ = small_size;
  ink_drop_small_corner_radius_ = small_corner_radius;
  ink_drop_animation_.reset();
}

void InkDropAnimationControllerImpl::SetInkDropCenter(
    const gfx::Point& center_point) {
  ink_drop_center_ = center_point;
  if (ink_drop_animation_)
    ink_drop_animation_->SetCenterPoint(ink_drop_center_);
}

void InkDropAnimationControllerImpl::CreateInkDropAnimation() {
  if (ink_drop_animation_)
    ink_drop_host_->RemoveInkDropLayer(ink_drop_animation_->root_layer());

  // TODO(bruthig): It will be expensive to maintain InkDropAnimation instances
  // when they are not actually being used. Consider creating InkDropAnimations
  // on an as-needed basis and if construction is also expensive then consider
  // creating an InkDropAnimationPool. See www.crbug.com/522175.
  ink_drop_animation_.reset(new InkDropAnimation(
      ink_drop_large_size_, ink_drop_large_corner_radius_, ink_drop_small_size_,
      ink_drop_small_corner_radius_));

  ink_drop_animation_->SetCenterPoint(ink_drop_center_);
  ink_drop_host_->AddInkDropLayer(ink_drop_animation_->root_layer());
}

}  // namespace views

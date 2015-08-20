// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/ink_drop_host.h"

namespace views {

InkDropAnimationControllerImpl::InkDropAnimationControllerImpl(
    InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host),
      ink_drop_animation_(new InkDropAnimation()) {
  // TODO(bruthig): Change this to only be called before the ink drop becomes
  // active. See www.crbug.com/522175.
  ink_drop_host_->AddInkDropLayer(ink_drop_animation_->root_layer());
}

InkDropAnimationControllerImpl::~InkDropAnimationControllerImpl() {
  // TODO(bruthig): Change this to be called when the ink drop becomes hidden.
  // See www.crbug.com/522175.
  ink_drop_host_->RemoveInkDropLayer(ink_drop_animation_->root_layer());
}

void InkDropAnimationControllerImpl::AnimateToState(InkDropState state) {
  ink_drop_animation_->AnimateToState(state);
}

void InkDropAnimationControllerImpl::SetInkDropSize(const gfx::Size& size) {
  ink_drop_animation_->SetInkDropSize(size);
}

gfx::Rect InkDropAnimationControllerImpl::GetInkDropBounds() const {
  return ink_drop_animation_->GetInkDropBounds();
}

void InkDropAnimationControllerImpl::SetInkDropBounds(const gfx::Rect& bounds) {
  ink_drop_animation_->SetInkDropBounds(bounds);
}

}  // namespace views

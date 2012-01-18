// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer_animation_observer.h"

namespace ui {

ImplicitAnimationObserver::ImplicitAnimationObserver()
    : active_(false),
      animation_count_(0) {
}

ImplicitAnimationObserver::~ImplicitAnimationObserver() {}

void ImplicitAnimationObserver::SetActive(bool active) {
  active_ = active;
  CheckCompleted();
}

void ImplicitAnimationObserver::OnLayerAnimationEnded(
    const LayerAnimationSequence* sequence) {
  animation_count_--;
  CheckCompleted();
}

void ImplicitAnimationObserver::OnLayerAnimationAborted(
    const LayerAnimationSequence* sequence) {
  animation_count_--;
  CheckCompleted();
}

void ImplicitAnimationObserver::OnLayerAnimationScheduled(
      const LayerAnimationSequence* sequence) {
  animation_count_++;
}

void ImplicitAnimationObserver::CheckCompleted() {
  if (active_ && animation_count_ == 0)
    OnImplicitAnimationsCompleted();
}

}  // namespace ui

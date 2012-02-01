// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer_animation_observer.h"

#include "ui/gfx/compositor/layer_animation_sequence.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// LayerAnimationObserver

bool LayerAnimationObserver::RequiresNotificationWhenAnimatorDestroyed() const {
  return false;
}

LayerAnimationObserver::LayerAnimationObserver() {
}

LayerAnimationObserver::~LayerAnimationObserver() {
  while (!attached_sequences_.empty()) {
    LayerAnimationSequence* sequence = *attached_sequences_.begin();
    sequence->RemoveObserver(this);
  }
}

void LayerAnimationObserver::AttachedToSequence(
    LayerAnimationSequence* sequence) {
  DCHECK(attached_sequences_.find(sequence) == attached_sequences_.end());
  attached_sequences_.insert(sequence);
}

void LayerAnimationObserver::DetachedFromSequence(
    LayerAnimationSequence* sequence) {
  if (attached_sequences_.find(sequence) != attached_sequences_.end())
    attached_sequences_.erase(sequence);
}

////////////////////////////////////////////////////////////////////////////////
// ImplicitAnimationObserver

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

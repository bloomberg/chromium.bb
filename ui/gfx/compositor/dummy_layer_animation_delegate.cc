// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/dummy_layer_animation_delegate.h"

namespace ui {

DummyLayerAnimationDelegate::DummyLayerAnimationDelegate() : opacity_(1.0f) {
}

DummyLayerAnimationDelegate::DummyLayerAnimationDelegate(
    const LayerAnimationDelegate& other)
    : bounds_(other.GetBoundsForAnimation()),
      transform_(other.GetTransformForAnimation()),
      opacity_(other.GetOpacityForAnimation()) {
}

DummyLayerAnimationDelegate::~DummyLayerAnimationDelegate() {
}

void DummyLayerAnimationDelegate::SetBoundsFromAnimation(
    const gfx::Rect& bounds) {
  bounds_ = bounds;
}

void DummyLayerAnimationDelegate::SetTransformFromAnimation(
    const Transform& transform) {
  transform_ = transform;
}

void DummyLayerAnimationDelegate::SetOpacityFromAnimation(float opacity) {
  opacity_ = opacity;
}

void DummyLayerAnimationDelegate::ScheduleDrawForAnimation() {
}

const gfx::Rect& DummyLayerAnimationDelegate::GetBoundsForAnimation() const {
  return bounds_;
}

const Transform& DummyLayerAnimationDelegate::GetTransformForAnimation() const {
  return transform_;
}

float DummyLayerAnimationDelegate::GetOpacityForAnimation() const {
  return opacity_;
}

void DummyLayerAnimationDelegate::OnLayerAnimationEnded(
    LayerAnimationSequence* sequence) {
}

}  // namespace ui

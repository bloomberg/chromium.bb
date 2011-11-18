// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test/test_layer_animation_delegate.h"

namespace ui {

TestLayerAnimationDelegate::TestLayerAnimationDelegate() : opacity_(1.0f) {
}

TestLayerAnimationDelegate::TestLayerAnimationDelegate(
    const LayerAnimationDelegate& other)
    : bounds_(other.GetBoundsForAnimation()),
      transform_(other.GetTransformForAnimation()),
      opacity_(other.GetOpacityForAnimation()) {
}

TestLayerAnimationDelegate::~TestLayerAnimationDelegate() {
}

void TestLayerAnimationDelegate::SetBoundsFromAnimation(
    const gfx::Rect& bounds) {
  bounds_ = bounds;
}

void TestLayerAnimationDelegate::SetTransformFromAnimation(
    const Transform& transform) {
  transform_ = transform;
}

void TestLayerAnimationDelegate::SetOpacityFromAnimation(float opacity) {
  opacity_ = opacity;
}

void TestLayerAnimationDelegate::ScheduleDrawForAnimation() {
}

const gfx::Rect& TestLayerAnimationDelegate::GetBoundsForAnimation() const {
  return bounds_;
}

const Transform& TestLayerAnimationDelegate::GetTransformForAnimation() const {
  return transform_;
}

float TestLayerAnimationDelegate::GetOpacityForAnimation() const {
  return opacity_;
}

}  // namespace ui

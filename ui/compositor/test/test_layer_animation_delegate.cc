// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_layer_animation_delegate.h"

namespace ui {

TestLayerAnimationDelegate::TestLayerAnimationDelegate()
    : opacity_(1.0f),
      visibility_(true),
      brightness_(0.0f),
      grayscale_(0.0f) {
}

TestLayerAnimationDelegate::TestLayerAnimationDelegate(
    const LayerAnimationDelegate& other)
    : bounds_(other.GetBoundsForAnimation()),
      transform_(other.GetTransformForAnimation()),
      opacity_(other.GetOpacityForAnimation()),
      visibility_(other.GetVisibilityForAnimation()) {
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

void TestLayerAnimationDelegate::SetVisibilityFromAnimation(bool visibility) {
  visibility_ = visibility;
}

void TestLayerAnimationDelegate::SetBrightnessFromAnimation(float brightness) {
  brightness_ = brightness;
}

void TestLayerAnimationDelegate::SetGrayscaleFromAnimation(float grayscale) {
  grayscale_ = grayscale;
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

bool TestLayerAnimationDelegate::GetVisibilityForAnimation() const {
  return visibility_;
}

float TestLayerAnimationDelegate::GetBrightnessForAnimation() const {
  return brightness_;
}

float TestLayerAnimationDelegate::GetGrayscaleForAnimation() const {
  return grayscale_;
}

}  // namespace ui

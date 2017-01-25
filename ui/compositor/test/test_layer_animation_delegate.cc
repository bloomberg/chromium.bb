// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"

namespace ui {

TestLayerThreadedAnimationDelegate::TestLayerThreadedAnimationDelegate() {}

TestLayerThreadedAnimationDelegate::~TestLayerThreadedAnimationDelegate() {}

TestLayerAnimationDelegate::TestLayerAnimationDelegate()
    : opacity_(1.0f),
      visibility_(true),
      brightness_(0.0f),
      grayscale_(0.0f),
      color_(SK_ColorBLACK) {
  CreateCcLayer();
}

TestLayerAnimationDelegate::TestLayerAnimationDelegate(
    const LayerAnimationDelegate& other)
    : bounds_(other.GetBoundsForAnimation()),
      transform_(other.GetTransformForAnimation()),
      opacity_(other.GetOpacityForAnimation()),
      visibility_(other.GetVisibilityForAnimation()),
      color_(SK_ColorBLACK) {
  CreateCcLayer();
}

TestLayerAnimationDelegate::TestLayerAnimationDelegate(
    const TestLayerAnimationDelegate& other) = default;

TestLayerAnimationDelegate::~TestLayerAnimationDelegate() {
}

void TestLayerAnimationDelegate::SetBoundsFromAnimation(
    const gfx::Rect& bounds) {
  bounds_ = bounds;
}

void TestLayerAnimationDelegate::SetTransformFromAnimation(
    const gfx::Transform& transform) {
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

void TestLayerAnimationDelegate::SetColorFromAnimation(SkColor color) {
  color_ = color;
}

void TestLayerAnimationDelegate::ScheduleDrawForAnimation() {
}

const gfx::Rect& TestLayerAnimationDelegate::GetBoundsForAnimation() const {
  return bounds_;
}

gfx::Transform TestLayerAnimationDelegate::GetTransformForAnimation() const {
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

SkColor TestLayerAnimationDelegate::GetColorForAnimation() const {
  return color_;
}

float TestLayerAnimationDelegate::GetDeviceScaleFactor() const {
  return 1.0f;
}

LayerAnimatorCollection*
TestLayerAnimationDelegate::GetLayerAnimatorCollection() {
  return NULL;
}

cc::Layer* TestLayerAnimationDelegate::GetCcLayer() const {
  return cc_layer_.get();
}

LayerThreadedAnimationDelegate*
TestLayerAnimationDelegate::GetThreadedAnimationDelegate() {
  return &threaded_delegate_;
}

int TestLayerAnimationDelegate::GetFrameNumber() const {
  return 0;
}

float TestLayerAnimationDelegate::GetRefreshRate() const {
  return 60.0;
}


void TestLayerAnimationDelegate::CreateCcLayer() {
  cc_layer_ = cc::Layer::Create();
}

void TestLayerThreadedAnimationDelegate::AddThreadedAnimation(
    std::unique_ptr<cc::Animation> animation) {}

void TestLayerThreadedAnimationDelegate::RemoveThreadedAnimation(
    int animation_id) {}

}  // namespace ui

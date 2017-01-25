// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_
#define UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_

#include "base/compiler_specific.h"
#include "cc/layers/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_threaded_animation_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

class TestLayerThreadedAnimationDelegate
    : public LayerThreadedAnimationDelegate {
 public:
  TestLayerThreadedAnimationDelegate();
  ~TestLayerThreadedAnimationDelegate() override;

  // Implementation of LayerThreadedAnimationDelegate
  void AddThreadedAnimation(std::unique_ptr<cc::Animation> animation) override;
  void RemoveThreadedAnimation(int animation_id) override;
};

class TestLayerAnimationDelegate : public LayerAnimationDelegate {
 public:
  TestLayerAnimationDelegate();
  explicit TestLayerAnimationDelegate(const LayerAnimationDelegate& other);
  TestLayerAnimationDelegate(const TestLayerAnimationDelegate& other);
  ~TestLayerAnimationDelegate() override;

  // Implementation of LayerAnimationDelegate
  void SetBoundsFromAnimation(const gfx::Rect& bounds) override;
  void SetTransformFromAnimation(const gfx::Transform& transform) override;
  void SetOpacityFromAnimation(float opacity) override;
  void SetVisibilityFromAnimation(bool visibility) override;
  void SetBrightnessFromAnimation(float brightness) override;
  void SetGrayscaleFromAnimation(float grayscale) override;
  void SetColorFromAnimation(SkColor color) override;
  void ScheduleDrawForAnimation() override;
  const gfx::Rect& GetBoundsForAnimation() const override;
  gfx::Transform GetTransformForAnimation() const override;
  float GetOpacityForAnimation() const override;
  bool GetVisibilityForAnimation() const override;
  float GetBrightnessForAnimation() const override;
  float GetGrayscaleForAnimation() const override;
  SkColor GetColorForAnimation() const override;
  float GetDeviceScaleFactor() const override;
  LayerAnimatorCollection* GetLayerAnimatorCollection() override;
  cc::Layer* GetCcLayer() const override;
  LayerThreadedAnimationDelegate* GetThreadedAnimationDelegate() override;
  int GetFrameNumber() const override;
  float GetRefreshRate() const override;

 private:
  void CreateCcLayer();

  TestLayerThreadedAnimationDelegate threaded_delegate_;

  gfx::Rect bounds_;
  gfx::Transform transform_;
  float opacity_;
  bool visibility_;
  float brightness_;
  float grayscale_;
  SkColor color_;
  scoped_refptr<cc::Layer> cc_layer_;

  // Allow copy and assign.
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_

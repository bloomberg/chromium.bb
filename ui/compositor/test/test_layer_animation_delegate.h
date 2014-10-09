// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_
#define UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_

#include "base/compiler_specific.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

class TestLayerAnimationDelegate : public LayerAnimationDelegate {
 public:
  TestLayerAnimationDelegate();
  explicit TestLayerAnimationDelegate(const LayerAnimationDelegate& other);
  virtual ~TestLayerAnimationDelegate();

  // Implementation of LayerAnimationDelegate
  virtual void SetBoundsFromAnimation(const gfx::Rect& bounds) override;
  virtual void SetTransformFromAnimation(
      const gfx::Transform& transform) override;
  virtual void SetOpacityFromAnimation(float opacity) override;
  virtual void SetVisibilityFromAnimation(bool visibility) override;
  virtual void SetBrightnessFromAnimation(float brightness) override;
  virtual void SetGrayscaleFromAnimation(float grayscale) override;
  virtual void SetColorFromAnimation(SkColor color) override;
  virtual void ScheduleDrawForAnimation() override;
  virtual const gfx::Rect& GetBoundsForAnimation() const override;
  virtual gfx::Transform GetTransformForAnimation() const override;
  virtual float GetOpacityForAnimation() const override;
  virtual bool GetVisibilityForAnimation() const override;
  virtual float GetBrightnessForAnimation() const override;
  virtual float GetGrayscaleForAnimation() const override;
  virtual SkColor GetColorForAnimation() const override;
  virtual float GetDeviceScaleFactor() const override;
  virtual void AddThreadedAnimation(
      scoped_ptr<cc::Animation> animation) override;
  virtual void RemoveThreadedAnimation(int animation_id) override;
  virtual LayerAnimatorCollection* GetLayerAnimatorCollection() override;

 private:
  gfx::Rect bounds_;
  gfx::Transform transform_;
  float opacity_;
  bool visibility_;
  float brightness_;
  float grayscale_;
  SkColor color_;

  // Allow copy and assign.
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_

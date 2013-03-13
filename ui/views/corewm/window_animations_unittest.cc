// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/window_animations.h"

#include "base/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using aura::Window;
using ui::Layer;

namespace views {
namespace corewm {

class WindowAnimationsTest : public aura::test::AuraTestBase {
 public:
  WindowAnimationsTest() {}

  virtual void TearDown() OVERRIDE {
    AuraTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowAnimationsTest);
};

TEST_F(WindowAnimationsTest, HideShowBrightnessGrayscaleAnimation) {
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Hiding.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE);
  AnimateOnChildWindowVisibilityChanged(window.get(), false);
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());

  // Showing.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE);
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());

  // Stays shown.
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      window->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() +
                base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());
}

TEST_F(WindowAnimationsTest, LayerTargetVisibility) {
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));

  // Layer target visibility changes according to Show/Hide.
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  window->Hide();
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
}

TEST_F(WindowAnimationsTest, CrossFadeToBounds) {
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  scoped_ptr<Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));
  window->SetBounds(gfx::Rect(5, 10, 320, 240));
  window->Show();

  Layer* old_layer = window->layer();
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());

  // Cross fade to a larger size, as in a maximize animation.
  CrossFadeToBounds(window.get(), gfx::Rect(0, 0, 640, 480));
  // Window's layer has been replaced.
  EXPECT_NE(old_layer, window->layer());
  // Original layer stays opaque and stretches to new size.
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("5,10 320x240", old_layer->bounds().ToString());
  gfx::Transform grow_transform;
  grow_transform.Translate(-5.f, -10.f);
  grow_transform.Scale(640.f / 320.f, 480.f / 240.f);
  EXPECT_EQ(grow_transform, old_layer->GetTargetTransform());
  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->GetTargetTransform());

  // Run the animations to completion.
  static_cast<ui::AnimationContainerElement*>(old_layer->GetAnimator())->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
  static_cast<ui::AnimationContainerElement*>(window->layer()->GetAnimator())->
      Step(base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));

  // Cross fade to a smaller size, as in a restore animation.
  old_layer = window->layer();
  CrossFadeToBounds(window.get(), gfx::Rect(5, 10, 320, 240));
  // Again, window layer has been replaced.
  EXPECT_NE(old_layer, window->layer());
  // Original layer fades out and stretches down to new size.
  EXPECT_EQ(0.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("0,0 640x480", old_layer->bounds().ToString());
  gfx::Transform shrink_transform;
  shrink_transform.Translate(5.f, 10.f);
  shrink_transform.Scale(320.f / 640.f, 240.f / 480.f);
  EXPECT_EQ(shrink_transform, old_layer->GetTargetTransform());
  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->GetTargetTransform());

  static_cast<ui::AnimationContainerElement*>(old_layer->GetAnimator())->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
  static_cast<ui::AnimationContainerElement*>(window->layer()->GetAnimator())->
      Step(base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

}  // namespace internal
}  // namespace ash

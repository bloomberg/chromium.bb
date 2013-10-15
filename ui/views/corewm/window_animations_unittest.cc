// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/window_animations.h"

#include "base/time/time.h"
#include "ui/aura/client/animation_host.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/gfx/vector2d.h"

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

TEST_F(WindowAnimationsTest, LayerTargetVisibility_AnimateShow) {
  // Tests if opacity and transform are reset when only show animation is
  // enabled.  See also LayerTargetVisibility_AnimateHide.
  // Since the window is not visible after Hide() is called, opacity and
  // transform shouldn't matter in case of ANIMATE_SHOW, but we reset them
  // to keep consistency.

  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithId(0, NULL));
  SetWindowVisibilityAnimationTransition(window.get(), ANIMATE_SHOW);

  // Layer target visibility and opacity change according to Show/Hide.
  window->Show();
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1, window->layer()->opacity());

  window->Hide();
  AnimateOnChildWindowVisibilityChanged(window.get(), false);
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(0, window->layer()->opacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->transform());

  window->Show();
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1, window->layer()->opacity());
}

TEST_F(WindowAnimationsTest, LayerTargetVisibility_AnimateHide) {
  // Tests if opacity and transform are reset when only hide animation is
  // enabled.  Hide animation changes opacity and transform in addition to
  // visibility, so we need to reset not only visibility but also opacity
  // and transform to show the window.

  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithId(0, NULL));
  SetWindowVisibilityAnimationTransition(window.get(), ANIMATE_HIDE);

  // Layer target visibility and opacity change according to Show/Hide.
  window->Show();
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1, window->layer()->opacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->transform());

  window->Hide();
  AnimateOnChildWindowVisibilityChanged(window.get(), false);
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(0, window->layer()->opacity());

  window->Show();
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1, window->layer()->opacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->transform());
}

// A simple AnimationHost implementation for the NotifyHideCompleted test.
class NotifyHideCompletedAnimationHost : public aura::client::AnimationHost {
 public:
  NotifyHideCompletedAnimationHost() : hide_completed_(false) {}
  virtual ~NotifyHideCompletedAnimationHost() {}

  // Overridden from TestWindowDelegate:
  virtual void OnWindowHidingAnimationCompleted() OVERRIDE {
    hide_completed_ = true;
  }

  virtual void SetHostTransitionOffsets(
      const gfx::Vector2d& top_left,
      const gfx::Vector2d& bottom_right) OVERRIDE {}

  bool hide_completed() const { return hide_completed_; }

 private:
  bool hide_completed_;

  DISALLOW_COPY_AND_ASSIGN(NotifyHideCompletedAnimationHost);
};

TEST_F(WindowAnimationsTest, NotifyHideCompleted) {
  NotifyHideCompletedAnimationHost animation_host;
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithId(0, NULL));
  aura::client::SetAnimationHost(window.get(), &animation_host);
  views::corewm::SetWindowVisibilityAnimationType(
      window.get(), WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_TRUE(window->layer()->visible());

  EXPECT_FALSE(animation_host.hide_completed());
  AnimateOnChildWindowVisibilityChanged(window.get(), false);
  EXPECT_TRUE(animation_host.hide_completed());
}

}  // namespace corewm
}  // namespace views

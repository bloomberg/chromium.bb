// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_tracker.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/interpolated_transform.h"

namespace aura {

namespace {

constexpr base::TimeDelta kTransitionDuration = base::TimeDelta::FromSeconds(3);

enum class WindowOcclusionChangedExpectation {
  // Expect OnWindowOcclusionChanged() to be called with true as argument.
  OCCLUDED,

  // Expect OnWindowOcclusionChanged() to be called with false as argument.
  NOT_OCCLUDED,

  // Don't expect OnWindowOcclusionChanged() to be called.
  NO_CALL,
};

class MockWindowDelegate : public test::ColorTestWindowDelegate {
 public:
  MockWindowDelegate() : test::ColorTestWindowDelegate(SK_ColorWHITE) {}
  ~MockWindowDelegate() override { EXPECT_FALSE(is_expecting_call()); }

  void set_window(Window* window) { window_ = window; }

  void set_expectation(WindowOcclusionChangedExpectation expectation) {
    expectation_ = expectation;
  }

  bool is_expecting_call() const {
    return expectation_ != WindowOcclusionChangedExpectation::NO_CALL;
  }

  void OnWindowOcclusionChanged(bool occluded) override {
    ASSERT_TRUE(window_);
    if (expectation_ == WindowOcclusionChangedExpectation::OCCLUDED) {
      EXPECT_TRUE(occluded);
      EXPECT_EQ(Window::OcclusionState::OCCLUDED, window_->occlusion_state());
    } else if (expectation_ ==
               WindowOcclusionChangedExpectation::NOT_OCCLUDED) {
      EXPECT_FALSE(occluded);
      EXPECT_EQ(Window::OcclusionState::NOT_OCCLUDED,
                window_->occlusion_state());
    } else {
      ADD_FAILURE() << "Unexpected call to OnWindowOcclusionChanged.";
    }
    expectation_ = WindowOcclusionChangedExpectation::NO_CALL;
  }

 private:
  WindowOcclusionChangedExpectation expectation_ =
      WindowOcclusionChangedExpectation::NO_CALL;
  Window* window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockWindowDelegate);
};

class WindowOcclusionTrackerTest : public test::AuraTestBase {
 public:
  WindowOcclusionTrackerTest() = default;

  Window* CreateTrackedWindow(MockWindowDelegate* delegate,
                              const gfx::Rect& bounds,
                              Window* parent = nullptr,
                              bool transparent = false) {
    Window* window = new Window(delegate);
    delegate->set_window(window);
    window->SetType(client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetTransparent(transparent);
    window->SetBounds(bounds);
    window->Show();
    parent = parent ? parent : root_window();
    parent->AddChild(window);
    WindowOcclusionTracker::Track(window);
    return window;
  }

  Window* CreateUntrackedWindow(const gfx::Rect& bounds,
                                Window* parent = nullptr) {
    Window* window = test::CreateTestWindow(SK_ColorWHITE, 1, bounds,
                                            parent ? parent : root_window());
    return window;
  }

 private:
  test::EnvTestHelper env_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionTrackerTest);
};

}  // namespace

// Verify that the non-overlapping windows are notified are not occluded.
// _____  _____
// |    | |    |
// |____| |____|
TEST_F(WindowOcclusionTrackerTest, NonOverlappingWindows) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(15, 0, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that partially overlapping windows are not occluded.
// ______
// |__|  |
// |_____|
TEST_F(WindowOcclusionTrackerTest, PartiallyOverlappingWindow) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that a window whose bounds are covered by a hidden window is not
// occluded. Also, verify that calling Show() on the hidden window causes
// occlusion states to be recomputed.
//  __....     ... = hidden window
// |__|  .
// .......
TEST_F(WindowOcclusionTrackerTest, HiddenWindowCoversWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded and expect window a to be
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 15, 15));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Hide window b. Expect window a to be non-occluded and window b to be
  // occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_b->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Show window b. Expect window a to be occluded and window b to be non-
  // occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_b->Show();
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that a window whose bounds are covered by a semi-transparent window is
// not occluded. Also, verify that that when the opacity of a window changes,
// occlusion states are updated.
//  __....     ... = semi-transparent window
// |__|  .
// .......
TEST_F(WindowOcclusionTrackerTest, SemiTransparentWindowCoversWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded and expect window a to be
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 15, 15));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Change the opacity of window b to 0.5f. Expect both windows to be non-
  // occluded.
  EXPECT_FALSE(delegate_a->is_expecting_call());
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_b->layer()->SetOpacity(0.5f);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Change the opacity of window b back to 1.0f. Expect window a to be
  // occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_b->layer()->SetOpacity(1.0f);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Same as previous test, but the occlusion state of the semi-transparent is not
// tracked.
TEST_F(WindowOcclusionTrackerTest, SemiTransparentUntrackedWindowCoversWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create untracked window b. Expect window a to be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 15, 15));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Change the opacity of window b to 0.5f. Expect both windows to be non-
  // occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_b->layer()->SetOpacity(0.5f);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Change the opacity of window b back to 1.0f. Expect window a to be
  // occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_b->layer()->SetOpacity(1.0f);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that one window whose bounds are covered by a set of two opaque
// windows is occluded.
//  ______
// |  |  |  <-- these two windows cover another window
// |__|__|
TEST_F(WindowOcclusionTrackerTest, TwoWindowsOccludeOneWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b with bounds that partially cover window a. Expect both
  // windows to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c with bounds that cover the portion of window a that isn't
  // already covered by window b. Expect window a to be occluded and window a/b
  // to be non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(5, 0, 5, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that when the bounds of a child window do not cover the bounds of a
// parent window, both windows are non-occluded.
TEST_F(WindowOcclusionTrackerTest, ChildDoesNotOccludeParent) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b with window a as parent. The bounds of window b do not
  // fully cover window a. Expect both windows to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that when the bounds of a child window cover the bounds of a parent
// window, the parent is occluded and the child is non-occluded. Also, verify
// that when the parent of a window changes, occlusion states are updated.
TEST_F(WindowOcclusionTrackerTest, ChildOccludesParent) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b with window a as parent. The bounds of window b fully cover
  // window a. Expect window a to be occluded but not window b.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b =
      CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10), window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c whose bounds don't overlap existing windows.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(15, 0, 10, 10));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Change the parent of window b from window a to window c. Expect window a to
  // be non-occluded and window c to be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_c->AddChild(window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that when the stacking order of windows change, occlusion states are
// updated.
TEST_F(WindowOcclusionTrackerTest, StackingChanged) {
  // Create three windows that have the same bounds. Expect window on top of the
  // stack to be non-occluded and other windows to be occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Move window a on top of the stack. Expect it to be non-occluded and expect
  // window c to be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that when the stacking order of two transparent window changes, the
// occlusion states of their children is updated. The goal of this test is to
// ensure that the fact that the windows whose stacking order change are
// transparent doesn't prevent occlusion states from being recomputed.
TEST_F(WindowOcclusionTrackerTest, TransparentParentStackingChanged) {
  // Create window a which is transparent. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which has the same bounds as its parent window a. Expect
  // window b to be non-occluded and window a to be occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10), window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c which is transparent and has the same bounds as window a
  // and window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Create window d which has the same bounds as its parent window c. Expect
  // window d to be non-occluded and all other windows to be occluded.
  MockWindowDelegate* delegate_d = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_d->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_d, gfx::Rect(0, 0, 10, 10), window_c);
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
  EXPECT_FALSE(delegate_d->is_expecting_call());

  // Move window a on top of the stack. Expect its child window b to be non-
  // occluded and all other windows to be occluded.
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  delegate_d->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_d->is_expecting_call());
}

// Verify that when StackChildAtTop() is called on a window whose occlusion
// state is not tracked, the occlusion state of tracked siblings is updated.
TEST_F(WindowOcclusionTrackerTest, UntrackedWindowStackingChanged) {
  Window* window_a = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));

  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Stack window a on top of window b. Expect window b to be occluded.
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that occlusion states are updated when the bounds of a window change.
TEST_F(WindowOcclusionTrackerTest, BoundsChanged) {
  // Create two non-overlapping windows. Expect them to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Move window b on top of window a. Expect window a to be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_b->SetBounds(window_a->bounds());
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that when the bounds of a window are animated, occlusion states are
// updated at the beginning and at the end of the animation, but not during the
// animation. At the beginning of the animation, the window animated window
// should be considered non-occluded and should not occlude other windows. The
// animated window starts occluded.
TEST_F(WindowOcclusionTrackerTest, OccludedWindowBoundsAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  window_b->layer()->SetAnimator(test_controller.animator());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Start animating the bounds of window b so that it moves on top of window a.
  // Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_b->SetBounds(window_a->bounds());
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window b should occlude window a at the end of the animation.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_b->layer()->SetAnimator(nullptr);
}

// Same as the previous test, but the animated window starts non-occluded.
TEST_F(WindowOcclusionTrackerTest, NonOccludedWindowBoundsAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
  window_c->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window c so that it moves on top of window a.
  // Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_c->SetBounds(window_a->bounds());
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window c should occlude window a at the end of the animation.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_c->layer()->SetAnimator(nullptr);
}

// Verify that occlusion states are updated when the bounds of a transparent
// window with opaque children change.
TEST_F(WindowOcclusionTrackerTest, TransparentParentBoundsChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a and is transparent. Expect
  // it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c which has window b as parent and doesn't occlude any
  // window.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5), window_b);
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Move window b so that window c occludes window a. Expect window a to be
  // occluded and other windows to be non-occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_b->SetBounds(gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that occlusion states are updated when the bounds of a window whose
// occlusion state is not tracked change.
TEST_F(WindowOcclusionTrackerTest, UntrackedWindowBoundsChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. It should not occlude window a.
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 10, 5, 5));

  // Move window b on top of window a. Expect window a to be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_b->SetBounds(window_a->bounds());
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that occlusion states are updated when the transform of a window
// changes.
TEST_F(WindowOcclusionTrackerTest, TransformChanged) {
  // Create two non-overlapping windows. Expect them to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Scale and translate window b so that it covers window a. Expect window a to
  // be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  gfx::Transform transform;
  transform.Translate(0.0f, -10.0f);
  transform.Scale(2.0f, 2.0f);
  window_b->SetTransform(transform);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that when the transform of a window is animated, occlusion states are
// updated at the beginning and at the end of the animation, but not during the
// animation. At the beginning of the animation, the window animated window
// should be considered non-occluded and should not occlude other windows. The
// animated window starts occluded.
TEST_F(WindowOcclusionTrackerTest, OccludedWindowTransformAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  window_b->layer()->SetAnimator(test_controller.animator());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Start animating the transform of window b so that it moves on top of window
  // a. Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  auto transform = std::make_unique<ui::InterpolatedScale>(
      gfx::Point3F(1, 1, 1), gfx::Point3F(2.0f, 2.0f, 1));
  transform->SetChild(std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(), gfx::PointF(-0.0f, -10.0f)));
  test_controller.animator()->StartAnimation(new ui::LayerAnimationSequence(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(transform), kTransitionDuration)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window b should occlude window a at the end of the animation.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_b->layer()->SetAnimator(nullptr);
}

// Same as the previous test, but the animated window starts non-occluded.
TEST_F(WindowOcclusionTrackerTest, NonOccludedWindowTransformAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 20, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 20, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
  window_c->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window c so that it moves on top of window a.
  // Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  auto transform = std::make_unique<ui::InterpolatedScale>(
      gfx::Point3F(1, 1, 1), gfx::Point3F(2.0f, 2.0f, 1));
  transform->SetChild(std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(), gfx::PointF(-0.0f, -20.0f)));
  test_controller.animator()->StartAnimation(new ui::LayerAnimationSequence(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(transform), kTransitionDuration)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window c should occlude window a at the end of the animation.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_c->layer()->SetAnimator(nullptr);
}

// Verify that occlusion states are updated when the transform of a transparent
// window with opaque children change.
TEST_F(WindowOcclusionTrackerTest, TransparentParentTransformChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a and is transparent. Expect
  // it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c which has window b as parent and doesn't occlude any
  // window.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5), window_b);
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Scale and translate window b so that window c occludes window a. Expect
  // window a to be occluded and other windows to be non-occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  gfx::Transform transform;
  transform.Translate(0.0f, -10.0f);
  transform.Scale(2.0f, 2.0f);
  window_b->SetTransform(transform);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that occlusion states are updated when the transform of a window whose
// occlusion state is not tracked changes.
TEST_F(WindowOcclusionTrackerTest, UntrackedWindowTransformChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. It should not occlude window a.
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 10, 5, 5));

  // Scale and translate window b so that it occludes window a. Expect window a
  // to be occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  gfx::Transform transform;
  transform.Translate(0.0f, -10.0f);
  transform.Scale(2.0f, 2.0f);
  window_b->SetTransform(transform);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that deleting an untracked window which covers a tracked window causes
// the tracked window to be non-occluded.
TEST_F(WindowOcclusionTrackerTest, DeleteUntrackedWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which occludes window a.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Delete window b. Expect a to be non-occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  delete window_b;
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that removing an untracked window which covers a tracked window causes
// the tracked window to be non-occluded.
TEST_F(WindowOcclusionTrackerTest, RemoveUntrackedWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which occludes window a.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Delete window b. Expect a to be non-occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  root_window()->RemoveChild(window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  delete window_b;
}

// Verify that when a tracked window is removed and re-added to a root,
// occlusion states are still tracked.
TEST_F(WindowOcclusionTrackerTest, RemoveAndAddTrackedToRoot) {
  Window* window_a = CreateUntrackedWindow(gfx::Rect(0, 0, 1, 1));
  CreateUntrackedWindow(gfx::Rect(0, 0, 10, 10), window_a);

  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Remove window c from its root.
  root_window()->RemoveChild(window_c);

  // Add window c back under its root.
  root_window()->AddChild(window_c);

  // Create untracked window d which covers window a. Expect window a to be
  // occluded.
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  Window* window_d = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Move window d so that it doesn't cover window c.
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_d->SetBounds(gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Stack window a on top of window c. Expect window c to be non-occluded. This
  // won't work if WindowOcclusionTracked didn't register as an observer of
  // window a when window c was made a child of root_window().
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

namespace {

class ResizeWindowObserver : public WindowObserver {
 public:
  ResizeWindowObserver(Window* window_to_resize)
      : window_to_resize_(window_to_resize) {}

  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    window_to_resize_->SetBounds(window->bounds());
  }

 private:
  Window* const window_to_resize_;

  DISALLOW_COPY_AND_ASSIGN(ResizeWindowObserver);
  ;
};

}  // namespace

// Verify that when the bounds of a child window are updated in response to the
// bounds of a parent window being updated, occlusion states are updated once.
TEST_F(WindowOcclusionTrackerTest, ResizeChildFromObserver) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b, which is a child of window a. Expect it to be non-
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b =
      CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create an observer that will resize window b when window a is resized.
  ResizeWindowObserver resize_window_observer(window_b);
  window_a->AddObserver(&resize_window_observer);

  // Resize window a. Expect window b to be resized so that window a is
  // occluded.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  window_a->SetBounds(gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a->RemoveObserver(&resize_window_observer);
}

// Verify that the bounds of windows are changed multiple times within the scope
// of a ScopedPauseOcclusionTracking, occlusion states are updated once at the
// end of the scope.
TEST_F(WindowOcclusionTrackerTest, ScopedPauseOcclusionTracking) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a. Expect it to be non-
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Change bounds multiple times. At the end of the scope, expect window a to
  // be occluded.
  {
    WindowOcclusionTracker::ScopedPauseOcclusionTracking
        pause_occlusion_tracking;
    window_b->SetBounds(window_a->bounds());
    window_a->SetBounds(gfx::Rect(0, 10, 5, 5));
    window_b->SetBounds(window_a->bounds());

    delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Same as the previous test, but with nested ScopedPauseOcclusionTracking.
TEST_F(WindowOcclusionTrackerTest, NestedScopedPauseOcclusionTracking) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a. Expect it to be non-
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Change bounds multiple times. At the end of the scope, expect window a to
  // be occluded.
  {
    WindowOcclusionTracker::ScopedPauseOcclusionTracking
        pause_occlusion_tracking_a;

    {
      WindowOcclusionTracker::ScopedPauseOcclusionTracking
          pause_occlusion_tracking_b;
      window_b->SetBounds(window_a->bounds());
    }
    {
      WindowOcclusionTracker::ScopedPauseOcclusionTracking
          pause_occlusion_tracking_c;
      window_a->SetBounds(gfx::Rect(0, 10, 5, 5));
    }
    {
      WindowOcclusionTracker::ScopedPauseOcclusionTracking
          pause_occlusion_tracking_d;
      window_b->SetBounds(window_a->bounds());
    }

    delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that bounds are computed correctly when a hierarchy of windows have
// transforms.
TEST_F(WindowOcclusionTrackerTest, HierarchyOfTransforms) {
  gfx::Transform scale_2x_transform;
  scale_2x_transform.Scale(2.0f, 2.0f);

  // Effective bounds: x = 2, y = 2, height = 10, width = 10
  Window* window_a = CreateUntrackedWindow(gfx::Rect(2, 2, 5, 5));
  window_a->SetTransform(scale_2x_transform);

  // Effective bounds: x = 4, y = 4, height = 4, width = 4
  Window* window_b = CreateUntrackedWindow(gfx::Rect(1, 1, 2, 2), window_a);

  // Effective bounds: x = 34, y = 36, height = 8, width = 10
  CreateUntrackedWindow(gfx::Rect(15, 16, 4, 5), window_b);

  MockWindowDelegate* delegate_d = new MockWindowDelegate();
  delegate_d->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_d = CreateTrackedWindow(delegate_d, gfx::Rect(34, 36, 8, 10));
  EXPECT_FALSE(delegate_d->is_expecting_call());

  delegate_d->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  root_window()->StackChildAtBottom(window_d);
  EXPECT_FALSE(delegate_d->is_expecting_call());

  delegate_d->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_d->SetBounds(gfx::Rect(35, 36, 8, 10));
  EXPECT_FALSE(delegate_d->is_expecting_call());

  // |window_d| should remain non-occluded with the following bounds changes.
  window_d->SetBounds(gfx::Rect(33, 36, 8, 10));
  window_d->SetBounds(gfx::Rect(34, 37, 8, 10));
  window_d->SetBounds(gfx::Rect(34, 35, 8, 10));
}

// Verify that clipping is taken into account when computing occlusion.
TEST_F(WindowOcclusionTrackerTest, Clipping) {
  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  window_b->layer()->SetMasksToBounds(true);

  // Create window c which has window b as parent. Expect it to occlude window
  // b, but not window a since it's clipped.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_c->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 10, 10), window_b);
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that the DCHECK(!WindowIsAnimated(window)) in
// WindowOcclusionTracker::OnWindowDestroyed() doesn't fire if a window is
// destroyed with an incomplete animation (~Window should complete the animation
// and the window should be removed from |animated_windows_| before
// OnWindowDestroyed() is called).
TEST_F(WindowOcclusionTrackerTest, DestroyWindowWithPendingAnimation) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());
  window->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window.
  window->SetBounds(gfx::Rect(10, 10, 5, 5));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_TRUE(test_controller.animator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));

  // Destroy the window. Expect no DCHECK failure.
  delete window;
}

// Verify that an animated window stops being considered as animated when its
// layer is recreated.
TEST_F(WindowOcclusionTrackerTest, RecreateLayerOfAnimatedWindow) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 2 windows. Window b occludes window a.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(2, 2, 1, 1));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  window_a->layer()->SetAnimator(test_controller.animator());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  delegate_b->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Start animating the bounds of window a. Window a should be non-occluded
  // when the animation starts.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  window_a->SetBounds(gfx::Rect(6, 6, 1, 1));
  test_controller.Step(kTransitionDuration / 2);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Recreate the layer of window b. Expect this to behave the same as if the
  // animation was abandoned.
  delegate_a->set_expectation(WindowOcclusionChangedExpectation::OCCLUDED);
  std::unique_ptr<ui::Layer> old_layer = window_a->RecreateLayer();
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a->layer()->SetAnimator(nullptr);
}

namespace {

class ObserverChangingWindowBounds : public WindowObserver {
 public:
  ObserverChangingWindowBounds() = default;

  // WindowObserver:
  void OnWindowParentChanged(Window* window, Window* parent) override {
    window->SetBounds(gfx::Rect(1, 2, 3, 4));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverChangingWindowBounds);
};

}  // namespace

// Verify that no crash occurs if a tracked window is modified by an observer
// after it has been added to a new root but before WindowOcclusionTracker has
// been notified.
TEST_F(WindowOcclusionTrackerTest, ChangeTrackedWindowBeforeObserveAddToRoot) {
  // Create a window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());

  // Remove the window from its root.
  root_window()->RemoveChild(window);

  // Add an observer that changes the bounds of |window| when it gets a new
  // parent.
  ObserverChangingWindowBounds observer;
  window->AddObserver(&observer);

  // Re-add the window to its root. Expect no crash when |observer| changes the
  // bounds.
  root_window()->AddChild(window);

  window->RemoveObserver(&observer);
}

namespace {

class ObserverDestroyingWindowOnAnimationEnded
    : public ui::LayerAnimationObserver {
 public:
  ObserverDestroyingWindowOnAnimationEnded(Window* window) : window_(window) {}

  ~ObserverDestroyingWindowOnAnimationEnded() override {
    EXPECT_FALSE(window_);
  }

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    EXPECT_TRUE(window_);
    delete window_;
    window_ = nullptr;
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ObserverDestroyingWindowOnAnimationEnded);
};

}  // namespace

// Verify that no crash occurs if a LayerAnimationObserver destroys a tracked
// window before WindowOcclusionTracker is notified that the animation ended.
TEST_F(WindowOcclusionTrackerTest,
       DestroyTrackedWindowFromLayerAnimationObserver) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create a window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  Window* window = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());
  window->layer()->SetAnimator(test_controller.animator());

  // Add a LayerAnimationObserver that destroys the window when an animation
  // ends.
  ObserverDestroyingWindowOnAnimationEnded observer(window);
  window->layer()->GetAnimator()->AddObserver(&observer);

  // Start animating the opacity of the window.
  window->layer()->SetOpacity(0.5f);

  // Complete the animation. Expect no crash.
  window->layer()->GetAnimator()->StopAnimating();
}

// Verify that no crash occurs if an animation completes on a non-tracked
// window's layer after the window has been removed from a root with a tracked
// window and deleted.
TEST_F(WindowOcclusionTrackerTest,
       DeleteNonTrackedAnimatedWindowRemovedFromTrackedRoot) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create a tracked window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(WindowOcclusionChangedExpectation::NOT_OCCLUDED);
  CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());

  // Create a non-tracked window and add an observer that deletes it when its
  // stops being animated.
  Window* window = CreateUntrackedWindow(gfx::Rect(10, 0, 10, 10));
  window->layer()->SetAnimator(test_controller.animator());
  ObserverDestroyingWindowOnAnimationEnded observer(window);
  window->layer()->GetAnimator()->AddObserver(&observer);

  // Animate the window. WindowOcclusionTracker should add itself as an observer
  // of its LayerAnimator (after |observer|).
  window->layer()->SetOpacity(0.5f);

  // Remove the non-tracked window from its root. WindowOcclusionTracker should
  // remove the window from its list of animated windows and stop observing it
  // and its LayerAnimator.
  root_window()->RemoveChild(window);

  // Complete animations on the window. |observer| will delete the window when
  // it is notified that animations are complete. Expect that
  // WindowOcclusionTracker will not try to access |window| after that (if it
  // does, the test will crash).
  window->layer()->GetAnimator()->StopAnimating();
}

}  // namespace aura

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bounds_animator.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/views/view.h"

using gfx::Animation;
using gfx::SlideAnimation;
using gfx::TestAnimationDelegate;

namespace views {
namespace {

class OwnedDelegate : public gfx::AnimationDelegate {
 public:
  OwnedDelegate() = default;

  ~OwnedDelegate() override { deleted_ = true; }

  static bool GetAndClearDeleted() {
    bool value = deleted_;
    deleted_ = false;
    return value;
  }

  static bool GetAndClearCanceled() {
    bool value = canceled_;
    canceled_ = false;
    return value;
  }

  // Overridden from gfx::AnimationDelegate:
  void AnimationCanceled(const Animation* animation) override {
    canceled_ = true;
  }

 private:
  static bool deleted_;
  static bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(OwnedDelegate);
};

// static
bool OwnedDelegate::deleted_ = false;
bool OwnedDelegate::canceled_ = false;

class TestView : public View {
 public:
  TestView() = default;

  void OnDidSchedulePaint(const gfx::Rect& r) override {
    ++repaint_count_;

    if (dirty_rect_.IsEmpty())
      dirty_rect_ = r;
    else
      dirty_rect_.Union(r);
  }

  const gfx::Rect& dirty_rect() const { return dirty_rect_; }

  void set_repaint_count(int val) { repaint_count_ = val; }
  int repaint_count() const { return repaint_count_; }

 private:
  gfx::Rect dirty_rect_;
  int repaint_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

class RTLAnimationTestDelegate : public gfx::AnimationDelegate {
 public:
  RTLAnimationTestDelegate(const gfx::Rect& start,
                           const gfx::Rect& target,
                           View* view,
                           base::RepeatingClosure quit_closure)
      : start_(start),
        target_(target),
        view_(view),
        quit_closure_(std::move(quit_closure)) {}
  ~RTLAnimationTestDelegate() override = default;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const Animation* animation) override {
    gfx::Transform transform = view_->GetTransform();
    ASSERT_TRUE(!transform.IsIdentity());

    // In this test, assume that |parent| is root view.
    View* parent = view_->parent();

    const gfx::Rect start_rect_in_screen = parent->GetMirroredRect(start_);
    const gfx::Rect target_rect_in_screen = parent->GetMirroredRect(target_);

    gfx::RectF current_bounds_in_screen(
        parent->GetMirroredRect(view_->bounds()));
    transform.TransformRect(&current_bounds_in_screen);

    // Verify that |view_|'s current bounds in screen are valid.
    EXPECT_GE(current_bounds_in_screen.x(),
              std::min(start_rect_in_screen.x(), target_rect_in_screen.x()));
    EXPECT_LE(
        current_bounds_in_screen.right(),
        std::max(start_rect_in_screen.right(), target_rect_in_screen.right()));

    quit_closure_.Run();
  }

  // Animation initial bounds.
  gfx::Rect start_;

  // Animation target bounds.
  gfx::Rect target_;

  // view to be animated.
  View* view_;

  base::RepeatingClosure quit_closure_;
};

}  // namespace

class BoundsAnimatorTest : public testing::Test {
 public:
  BoundsAnimatorTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        child_(new TestView()) {
    parent_.AddChildView(child_);
    RecreateAnimator(/*use_transforms=*/false);
  }

  TestView* parent() { return &parent_; }
  TestView* child() { return child_; }
  BoundsAnimator* animator() { return animator_.get(); }

 protected:
  void RecreateAnimator(bool use_transforms) {
    animator_ = std::make_unique<BoundsAnimator>(&parent_, use_transforms);
    animator_->SetAnimationDuration(base::TimeDelta::FromMilliseconds(10));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  TestView parent_;
  TestView* child_;  // Owned by |parent_|.
  std::unique_ptr<BoundsAnimator> animator_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimatorTest);
};

// Checks animate view to.
TEST_F(BoundsAnimatorTest, AnimateViewTo) {
  gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(),
                                   std::make_unique<TestAnimationDelegate>());

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Run the message loop; the delegate exits the loop when the animation is
  // done.
  base::RunLoop().Run();

  // Make sure the bounds match of the view that was animated match.
  EXPECT_EQ(target_bounds, child()->bounds());

  // |child| shouldn't be animating anymore.
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // The parent should have been told to repaint as the animation progressed.
  // The resulting rect is the union of the original and target bounds.
  EXPECT_EQ(gfx::UnionRects(target_bounds, initial_bounds),
            parent()->dirty_rect());
}

// Make sure that removing/deleting a child view while animating stops the
// view's animation and will not result in a crash.
TEST_F(BoundsAnimatorTest, DeleteWhileAnimating) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Make sure that animation is removed upon deletion.
  delete child();
  EXPECT_FALSE(animator()->GetAnimationForView(child()));
  EXPECT_FALSE(animator()->IsAnimating(child()));
}

// Make sure an AnimationDelegate is deleted when canceled.
TEST_F(BoundsAnimatorTest, DeleteDelegateOnCancel) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  animator()->Cancel();

  // The animator should no longer be animating.
  EXPECT_FALSE(animator()->IsAnimating());
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // The cancel should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
}

// Make sure an AnimationDelegate is deleted when another animation is
// scheduled.
TEST_F(BoundsAnimatorTest, DeleteDelegateOnNewAnimate) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));

  // Starting a new animation should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
}

// Makes sure StopAnimating works.
TEST_F(BoundsAnimatorTest, StopAnimating) {
  std::unique_ptr<OwnedDelegate> delegate(std::make_unique<OwnedDelegate>());

  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  animator()->StopAnimatingView(child());

  // Shouldn't be animating now.
  EXPECT_FALSE(animator()->IsAnimating());
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // Stopping should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
}

// Tests using the transforms option.
TEST_F(BoundsAnimatorTest, UseTransformsAnimateViewTo) {
  RecreateAnimator(/*use_transforms=*/true);

  gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);

  child()->set_repaint_count(0);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(),
                                   std::make_unique<TestAnimationDelegate>());

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Run the message loop; the delegate exits the loop when the animation is
  // done.
  base::RunLoop().Run();

  // Make sure the bounds match of the view that was animated match and the
  // layer is destroyed.
  EXPECT_EQ(target_bounds, child()->bounds());
  EXPECT_FALSE(child()->layer());

  // |child| shouldn't be animating anymore.
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // Schedule a longer animation. The number of repaints should be the same as
  // with the short animation.
  const base::TimeDelta long_duration = base::TimeDelta::FromMilliseconds(2000);
  const int repaint_count = child()->repaint_count();
  animator()->SetAnimationDuration(long_duration);
  child()->set_repaint_count(0);
  animator()->AnimateViewTo(child(), initial_bounds);
  animator()->SetAnimationDelegate(child(),
                                   std::make_unique<TestAnimationDelegate>());
  task_environment_.FastForwardBy(long_duration);
  base::RunLoop().Run();
  EXPECT_EQ(repaint_count, child()->repaint_count());
}

// Tests that the transforms option does not crash when a view's bounds start
// off empty.
TEST_F(BoundsAnimatorTest, UseTransformsAnimateViewToEmptySrc) {
  RecreateAnimator(/*use_transforms=*/true);

  gfx::Rect initial_bounds(0, 0, 0, 0);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);

  child()->set_repaint_count(0);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(),
                                   std::make_unique<TestAnimationDelegate>());

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Run the message loop; the delegate exits the loop when the animation is
  // done.
  base::RunLoop().Run();
  EXPECT_EQ(target_bounds, child()->bounds());
}

// Tests that when using the transform option on the bounds animator, cancelling
// the animation part way results in the correct bounds applied.
TEST_F(BoundsAnimatorTest, UseTransformsCancelAnimation) {
  RecreateAnimator(/*use_transforms=*/true);

  gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);

  const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(200);
  animator()->SetAnimationDuration(duration);
  // Use a linear tween so we can estimate the expected bounds.
  animator()->set_tween_type(gfx::Tween::LINEAR);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(),
                                   std::make_unique<TestAnimationDelegate>());
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Stop halfway and cancel. The child should have its bounds updated to
  // exactly halfway between |initial_bounds| and |target_bounds|.
  const gfx::Rect expected_bounds(5, 5, 15, 15);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(initial_bounds, child()->bounds());
  animator()->Cancel();
  EXPECT_EQ(expected_bounds, child()->bounds());
}

// Verify that the bounds animation which updates the transform of views work
// as expected under RTL (https://crbug.com/1067033).
TEST_F(BoundsAnimatorTest, VerifyBoundsAnimatorUnderRTL) {
  // Enable RTL.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");

  RecreateAnimator(/*use_transform=*/true);
  parent()->SetBounds(0, 0, 40, 40);

  const gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  const gfx::Rect target_bounds(10, 10, 10, 10);

  const base::TimeDelta animation_duration =
      base::TimeDelta::FromMilliseconds(10);
  animator()->SetAnimationDuration(animation_duration);
  child()->set_repaint_count(0);
  animator()->AnimateViewTo(child(), target_bounds);
  base::RunLoop run_loop;
  animator()->SetAnimationDelegate(
      child(),
      std::make_unique<RTLAnimationTestDelegate>(
          initial_bounds, target_bounds, child(), run_loop.QuitClosure()));

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  run_loop.Run();
  EXPECT_FALSE(animator()->IsAnimating(child()));
}

}  // namespace views

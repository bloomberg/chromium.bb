// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/test_animation_delegate.h"

#if defined(OS_WIN)
#include "base/test/scoped_task_environment.h"
#include "base/win/windows_version.h"
#endif

namespace gfx {

class AnimationTest: public testing::Test {
 protected:
  AnimationTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

namespace {

///////////////////////////////////////////////////////////////////////////////
// RunAnimation

class RunAnimation : public LinearAnimation {
 public:
  RunAnimation(int frame_rate, AnimationDelegate* delegate)
      : LinearAnimation(delegate, frame_rate) {}

  void AnimateToState(double state) override {
    EXPECT_LE(0.0, state);
    EXPECT_GE(1.0, state);
  }
};

///////////////////////////////////////////////////////////////////////////////
// CancelAnimation

class CancelAnimation : public LinearAnimation {
 public:
  CancelAnimation(int duration, int frame_rate, AnimationDelegate* delegate)
      : LinearAnimation(duration, frame_rate, delegate) {
  }

  void AnimateToState(double state) override {
    if (state >= 0.5)
      Stop();
  }
};

///////////////////////////////////////////////////////////////////////////////
// EndAnimation

class EndAnimation : public LinearAnimation {
 public:
  EndAnimation(int duration, int frame_rate, AnimationDelegate* delegate)
      : LinearAnimation(duration, frame_rate, delegate) {
  }

  void AnimateToState(double state) override {
    if (state >= 0.5)
      End();
  }
};

///////////////////////////////////////////////////////////////////////////////
// DeletingAnimationDelegate

// AnimationDelegate implementation that deletes the animation in ended.
class DeletingAnimationDelegate : public AnimationDelegate {
 public:
  void AnimationEnded(const Animation* animation) override {
    delete animation;
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LinearCase

TEST_F(AnimationTest, RunCase) {
  TestAnimationDelegate ad;
  RunAnimation a1(150, &ad);
  a1.SetDuration(2000);
  a1.Start();
  base::RunLoop().Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_FALSE(ad.canceled());
}

TEST_F(AnimationTest, CancelCase) {
  TestAnimationDelegate ad;
  CancelAnimation a2(2000, 150, &ad);
  a2.Start();
  base::RunLoop().Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_TRUE(ad.canceled());
}

// Lets an animation run, invoking End part way through and make sure we get the
// right delegate methods invoked.
TEST_F(AnimationTest, EndCase) {
  TestAnimationDelegate ad;
  EndAnimation a2(2000, 150, &ad);
  a2.Start();
  base::RunLoop().Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_FALSE(ad.canceled());
}

// Runs an animation with a delegate that deletes the animation in end.
TEST_F(AnimationTest, DeleteFromEnd) {
  DeletingAnimationDelegate delegate;
  RunAnimation* animation = new RunAnimation(150, &delegate);
  animation->Start();
  base::RunLoop().Run();
  // delegate should have deleted animation.
}

TEST_F(AnimationTest, ShouldRenderRichAnimation) {
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    BOOL result;
    ASSERT_NE(
        0, ::SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &result, 0));
    // ShouldRenderRichAnimation() should check the SPI_GETCLIENTAREAANIMATION
    // value on Vista.
    EXPECT_EQ(!!result, Animation::ShouldRenderRichAnimation());
  } else {
    // On XP, the function should check the SM_REMOTESESSION value.
    EXPECT_EQ(!::GetSystemMetrics(SM_REMOTESESSION),
              Animation::ShouldRenderRichAnimation());
  }
#else
  EXPECT_TRUE(Animation::ShouldRenderRichAnimation());
#endif
}

// Test that current value is always 0 after Start() is called.
TEST_F(AnimationTest, StartState) {
  LinearAnimation animation(100, 60, NULL);
  EXPECT_EQ(0.0, animation.GetCurrentValue());
  animation.Start();
  EXPECT_EQ(0.0, animation.GetCurrentValue());
  animation.End();
  EXPECT_EQ(1.0, animation.GetCurrentValue());
  animation.Start();
  EXPECT_EQ(0.0, animation.GetCurrentValue());
}

}  // namespace gfx

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/test_animation_delegate.h"

namespace ui {

class SlideAnimationTest: public testing::Test {
 private:
  MessageLoopForUI message_loop_;
};

// Tests that delegate is not notified when animation is running and is deleted.
// (Such a scenario would cause problems for BoundsAnimator).
TEST_F(SlideAnimationTest, DontNotifyOnDelete) {
  TestAnimationDelegate delegate;
  scoped_ptr<SlideAnimation> animation(new SlideAnimation(&delegate));

  // Start the animation.
  animation->Show();

  // Delete the animation.
  animation.reset();

  // Make sure the delegate wasn't notified.
  EXPECT_FALSE(delegate.finished());
  EXPECT_FALSE(delegate.canceled());
}

}  // namespace ui

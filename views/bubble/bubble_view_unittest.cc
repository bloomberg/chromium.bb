// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "third_party/skia/include/core/SkColor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/slide_animation.h"
#include "views/bubble/bubble_border.h"
#include "views/bubble/bubble_delegate.h"
#include "views/bubble/bubble_view.h"
#include "views/widget/widget.h"

namespace views {

namespace {

class TestBubbleDelegate : public BubbleDelegateView {
 public:
  explicit TestBubbleDelegate(Widget *frame): BubbleDelegateView(frame) {}
  SkColor GetFrameBackgroundColor() { return SK_ColorGREEN; }
  gfx::Rect GetBounds() { return gfx::Rect(10, 10, 200, 200); }
  BubbleBorder::ArrowLocation GetFrameArrowLocation() {
    return BubbleBorder::LEFT_BOTTOM;
  }
};

class TestAnimationDelegate : public ui::AnimationDelegate {
 public:
  TestAnimationDelegate():animation_progressed_(0), animation_ended_(0) {}
  void AnimationProgressed(const ui::Animation* animation) {
    ++animation_progressed_;
  }
  void AnimationEnded(const ui::Animation* animation) {
    ++animation_ended_;
  }
  int animation_progressed_;
  int animation_ended_;
};


TEST(BubbleViewBasicTest, CreateArrowBubble) {
  MessageLoopForUI message_loop;
  Widget* bubble_widget = new Widget();
  Widget::InitParams params(Widget::InitParams::TYPE_BUBBLE);
  TestBubbleDelegate* delegate = new TestBubbleDelegate(bubble_widget);
  params.delegate = delegate;
  bubble_widget->Init(params);

  BubbleBorder* border =
      static_cast<BubbleBorder*>(bubble_widget->non_client_view()
                                        ->frame_view()->border());
  EXPECT_EQ(delegate->GetFrameArrowLocation(), border->arrow_location());
  MessageLoop::current()->RunAllPending();
}

}  // namespace

TEST(BubbleViewTest, FadeAnimation) {
  MessageLoopForUI message_loop;

  Widget* bubble_widget = new Widget();
  Widget::InitParams params(Widget::InitParams::TYPE_BUBBLE);
  params.delegate = new TestBubbleDelegate(bubble_widget);
  bubble_widget->Init(params);
  bubble_widget->Show();
  BubbleView* bubble_view = bubble_widget->client_view()->AsBubbleView();
  TestAnimationDelegate test_animation_delegate;
  bubble_view->set_animation_delegate(&test_animation_delegate);
  bubble_view->StartFade();

  bubble_view->AnimationProgressed(bubble_view->fade_animation_.get());
  bubble_view->AnimationEnded(bubble_view->fade_animation_.get());

  EXPECT_LT(0, test_animation_delegate.animation_progressed_);
  EXPECT_EQ(1, test_animation_delegate.animation_ended_);
  MessageLoop::current()->RunAllPending();
}

}  // namespace views

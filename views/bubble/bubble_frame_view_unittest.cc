// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "third_party/skia/include/core/SkColor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/bubble/bubble_border.h"
#include "views/bubble/bubble_frame_view.h"
#include "views/bubble/bubble_delegate.h"
#include "views/widget/widget.h"
#if !defined(OS_WIN)
#include "views/window/hit_test.h"
#endif
namespace views {

namespace {

gfx::Rect kBound = gfx::Rect(10, 10, 200, 200);
SkColor kBackgroundColor = SK_ColorRED;
BubbleBorder::ArrowLocation kArrow =  BubbleBorder::LEFT_BOTTOM;

TEST(BubbleFrameViewBasicTest, GetBoundsForClientView) {
  MessageLoopForUI message_loop;
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  widget->Init(params);
  BubbleFrameView frame(widget, kBound, kBackgroundColor, kArrow);
  EXPECT_EQ(kBound, frame.bounds());
  EXPECT_EQ(kArrow,
            static_cast<BubbleBorder*>(frame.border())->arrow_location());
  EXPECT_EQ(kBackgroundColor,
            static_cast<BubbleBorder*>(frame.border())->background_color());

  BubbleBorder* expected_border = new BubbleBorder(BubbleBorder::LEFT_BOTTOM);
  gfx::Insets expected_insets;
  expected_border->GetInsets(&expected_insets);
  EXPECT_EQ(frame.GetBoundsForClientView().x(), expected_insets.left());
  EXPECT_EQ(frame.GetBoundsForClientView().y(), expected_insets.top());
  MessageLoop::current()->RunAllPending();
}

class TestBubbleDelegate : public BubbleDelegateView {
 public:
  explicit TestBubbleDelegate(Widget *frame): BubbleDelegateView(frame) {}
  SkColor GetFrameBackgroundColor() { return kBackgroundColor; }
  gfx::Rect GetBounds() { return gfx::Rect(10, 10, 200, 200); }
  BubbleBorder::ArrowLocation GetFrameArrowLocation() { return kArrow; }
};

TEST(BubbleFrameViewBasicTest, NonClientHitTest) {
  MessageLoopForUI message_loop;
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  TestBubbleDelegate* delegate = new TestBubbleDelegate(widget);
  params.delegate = delegate;
  widget->Init(params);
  gfx::Point kPtInBound(100, 100);
  gfx::Point kPtOutsideBound(1000, 1000);
  EXPECT_EQ(HTCLIENT, widget->non_client_view()->NonClientHitTest(kPtInBound));
  EXPECT_EQ(HTNOWHERE,
            widget->non_client_view()->NonClientHitTest(kPtOutsideBound));
  MessageLoop::current()->RunAllPending();
}

}  // namespace
}  // namespace views

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
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
  scoped_ptr<Widget> widget(new views::Widget());
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  BubbleFrameView frame(widget.get(), kBound, kBackgroundColor, kArrow);
  EXPECT_EQ(kBound, frame.bounds());
  EXPECT_EQ(kArrow,
            static_cast<BubbleBorder*>(frame.border())->arrow_location());
  EXPECT_EQ(kBackgroundColor,
            static_cast<BubbleBorder*>(frame.border())->background_color());

  gfx::Insets expected_insets;
  frame.border()->GetInsets(&expected_insets);
  EXPECT_EQ(expected_insets.left(), frame.GetBoundsForClientView().x());
  EXPECT_EQ(expected_insets.top(), frame.GetBoundsForClientView().y());
  widget->CloseNow();
  widget.reset(NULL);
  MessageLoop::current()->RunAllPending();
}

class TestBubbleDelegate : public BubbleDelegateView {
 public:
  explicit TestBubbleDelegate(Widget *frame): BubbleDelegateView(frame) {}
  SkColor GetFrameBackgroundColor() { return kBackgroundColor; }
  gfx::Rect GetBounds() { return gfx::Rect(10, 10, 200, 200); }
  BubbleBorder::ArrowLocation GetFrameArrowLocation() { return kArrow; }
  View* GetContentsView() { return &view_; }

  View view_;
};

TEST(BubbleFrameViewBasicTest, NonClientHitTest) {
  MessageLoopForUI message_loop;
  scoped_ptr<Widget> widget(new Widget());
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  TestBubbleDelegate delegate(widget.get());
  params.delegate = &delegate;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  gfx::Point kPtInBound(100, 100);
  gfx::Point kPtOutsideBound(1000, 1000);
  EXPECT_EQ(HTCLIENT, widget->non_client_view()->NonClientHitTest(kPtInBound));
  EXPECT_EQ(HTNOWHERE,
            widget->non_client_view()->NonClientHitTest(kPtOutsideBound));
  widget->CloseNow();
  widget.reset(NULL);
  MessageLoop::current()->RunAllPending();
}

}  // namespace
}  // namespace views

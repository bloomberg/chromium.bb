// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_frame_view.h"
#include "views/bubble/bubble_delegate.h"
#include "views/test/views_test_base.h"
#include "views/widget/widget.h"
#if !defined(OS_WIN)
#include "views/window/hit_test.h"
#endif

namespace views {

namespace {

typedef ViewsTestBase BubbleFrameViewBasicTest;

const BubbleBorder::ArrowLocation kArrow =  BubbleBorder::LEFT_BOTTOM;
const gfx::Rect kRect(10, 10, 200, 200);
const SkColor kBackgroundColor = SK_ColorRED;

TEST_F(BubbleFrameViewBasicTest, GetBoundsForClientView) {
  BubbleFrameView frame(kArrow, kRect.size(), kBackgroundColor);
  EXPECT_EQ(frame.GetWindowBoundsForClientBounds(kRect).size(), frame.size());
  BubbleBorder* bubble_border = static_cast<BubbleBorder*>(frame.border());
  EXPECT_EQ(kArrow, bubble_border->arrow_location());
  EXPECT_EQ(kBackgroundColor, bubble_border->background_color());

  gfx::Insets expected_insets(frame.GetInsets());
  EXPECT_EQ(expected_insets.left(), frame.GetBoundsForClientView().x());
  EXPECT_EQ(expected_insets.top(), frame.GetBoundsForClientView().y());
}

TEST_F(BubbleFrameViewBasicTest, NonClientHitTest) {
  BubbleDelegateView* delegate = new BubbleDelegateView();
  scoped_ptr<Widget> widget(
      views::BubbleDelegateView::CreateBubble(delegate, NULL));
  widget->SetBounds(kRect);
  widget->Show();
  gfx::Point kPtInBound(100, 100);
  gfx::Point kPtOutsideBound(1000, 1000);
  EXPECT_EQ(HTCLIENT, widget->non_client_view()->NonClientHitTest(kPtInBound));
  EXPECT_EQ(HTNOWHERE,
            widget->non_client_view()->NonClientHitTest(kPtOutsideBound));
  widget->CloseNow();
  widget.reset();
  RunPendingMessages();
}

}  // namespace

}  // namespace views

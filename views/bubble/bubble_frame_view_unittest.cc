// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_delegate.h"
#include "views/bubble/bubble_frame_view.h"
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

}  // namespace

class SizedBubbleDelegateView : public BubbleDelegateView {
 public:
   SizedBubbleDelegateView() {}
   virtual ~SizedBubbleDelegateView() {}

   // View overrides:
   virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SizedBubbleDelegateView);
};

gfx::Size SizedBubbleDelegateView::GetPreferredSize() { return kRect.size(); }

TEST_F(BubbleFrameViewBasicTest, NonClientHitTest) {
  SizedBubbleDelegateView* delegate = new SizedBubbleDelegateView();
  scoped_ptr<Widget> widget(BubbleDelegateView::CreateBubble(delegate, NULL));
  delegate->Show();
  gfx::Point kPtInBound(100, 100);
  gfx::Point kPtOutsideBound(1000, 1000);
  BubbleFrameView* bubble_frame_view = delegate->GetBubbleFrameView();
  EXPECT_EQ(HTCLIENT, bubble_frame_view->NonClientHitTest(kPtInBound));
  EXPECT_EQ(HTNOWHERE, bubble_frame_view->NonClientHitTest(kPtOutsideBound));
  widget->CloseNow();
  widget.reset();
  RunPendingMessages();
}

}  // namespace views

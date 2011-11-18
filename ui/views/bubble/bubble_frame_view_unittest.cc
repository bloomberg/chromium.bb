// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test.h"
#include "ui/views/bubble/border_contents_view.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "views/widget/widget.h"

namespace views {

typedef ViewsTestBase BubbleFrameViewBasicTest;

const BubbleBorder::ArrowLocation kArrow = BubbleBorder::TOP_LEFT;
const gfx::Rect kRect(10, 10, 200, 200);
const SkColor kBackgroundColor = SK_ColorRED;
const bool kAllowBubbleOffscreen = true;

TEST_F(BubbleFrameViewBasicTest, GetBoundsForClientView) {
  BubbleFrameView frame(kArrow, kRect.size(), kBackgroundColor,
                        kAllowBubbleOffscreen);
  EXPECT_EQ(frame.GetWindowBoundsForClientBounds(kRect).size(), frame.size());
  EXPECT_EQ(kArrow, frame.bubble_border()->arrow_location());
  EXPECT_EQ(kBackgroundColor, frame.bubble_border()->background_color());

  int margin_x = frame.border_contents_->content_margins().left();
  int margin_y = frame.border_contents_->content_margins().top();
  gfx::Insets insets;
  frame.bubble_border()->GetInsets(&insets);
  EXPECT_EQ(insets.left() + margin_x, frame.GetBoundsForClientView().x());
  EXPECT_EQ(insets.top() + margin_y, frame.GetBoundsForClientView().y());
}

namespace {

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

}  // namespace

TEST_F(BubbleFrameViewBasicTest, NonClientHitTest) {
  BubbleDelegateView* delegate = new SizedBubbleDelegateView();
  Widget* widget(BubbleDelegateView::CreateBubble(delegate));
  delegate->Show();
  gfx::Point kPtInBound(100, 100);
  gfx::Point kPtOutsideBound(1000, 1000);
  BubbleFrameView* bubble_frame_view = delegate->GetBubbleFrameView();
  EXPECT_EQ(HTCLIENT, bubble_frame_view->NonClientHitTest(kPtInBound));
  EXPECT_EQ(HTNOWHERE, bubble_frame_view->NonClientHitTest(kPtOutsideBound));
  widget->CloseNow();
  RunPendingMessages();
}

}  // namespace views

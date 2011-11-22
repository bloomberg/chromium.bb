// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase BubbleDelegateTest;

TEST_F(BubbleDelegateTest, CreateDelegate) {
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      NULL, BubbleBorder::NONE, SK_ColorGREEN);
  Widget* bubble_widget(
      BubbleDelegateView::CreateBubble(bubble_delegate));
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());

  BubbleBorder* border =
      bubble_delegate->GetBubbleFrameView()->bubble_border();
  EXPECT_EQ(bubble_delegate->GetArrowLocation(), border->arrow_location());
  EXPECT_EQ(bubble_delegate->GetColor(), border->background_color());

  bubble_widget->CloseNow();
  RunPendingMessages();
}

}  // namespace views

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase BubbleDelegateTest;

TEST_F(BubbleDelegateTest, CreateDelegate) {
  BubbleDelegateView* bubble_delegate =
      new BubbleDelegateView(NULL, BubbleBorder::NONE);
  bubble_delegate->set_color(SK_ColorGREEN);
  Widget* bubble_widget(
      BubbleDelegateView::CreateBubble(bubble_delegate));
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());

  BubbleBorder* border =
      bubble_delegate->GetBubbleFrameView()->bubble_border();
  EXPECT_EQ(bubble_delegate->arrow_location(), border->arrow_location());
  EXPECT_EQ(bubble_delegate->color(), border->background_color());

  bubble_widget->CloseNow();
  RunPendingMessages();
}

TEST_F(BubbleDelegateTest, ResetAnchorWidget) {
  // Create the anchor widget first.
  Widget* widget = new Widget;
  View* contents = new View;

  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  widget->Init(params);
  widget->SetContentsView(contents);
  widget->Show();

  Widget* parent = new Widget;
  parent->Init(params);
  parent->SetContentsView(new View);
  parent->Show();

  // Make sure the bubble widget is parented to a widget other than the anchor
  // widget so that closing the anchor widget does not close the bubble widget.
  BubbleDelegateView* bubble_delegate =
      new BubbleDelegateView(contents, BubbleBorder::NONE);
  bubble_delegate->set_parent_window(parent->GetNativeView());
  bubble_delegate->set_color(SK_ColorGREEN);
  Widget* bubble_widget(
      BubbleDelegateView::CreateBubble(bubble_delegate));
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(widget, bubble_delegate->anchor_widget());

  bubble_widget->Show();
  RunPendingMessages();
  EXPECT_EQ(widget, bubble_delegate->anchor_widget());

  bubble_widget->Hide();
  RunPendingMessages();
  EXPECT_EQ(widget, bubble_delegate->anchor_widget());

  // Closing the anchor widget should unset the reference to the anchor widget
  // for the bubble.
  widget->CloseNow();
  RunPendingMessages();
  EXPECT_NE(bubble_delegate->anchor_widget(), widget);

  bubble_widget->CloseNow();
  parent->CloseNow();
  RunPendingMessages();
}

}  // namespace views

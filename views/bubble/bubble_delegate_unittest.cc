// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_delegate.h"
#include "views/test/views_test_base.h"
#include "views/widget/widget.h"

namespace views {

namespace {

typedef ViewsTestBase BubbleDelegateTest;

TEST_F(BubbleDelegateTest, CreateDelegate) {
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView();
  scoped_ptr<Widget> bubble_widget(
      views::BubbleDelegateView::CreateBubble(bubble_delegate, NULL));
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  bubble_widget->CloseNow();
  bubble_widget.reset();
  RunPendingMessages();
}

}  // namespace

}  // namespace views

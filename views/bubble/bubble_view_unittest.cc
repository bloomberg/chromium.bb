// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_delegate.h"
#include "views/test/views_test_base.h"
#include "views/widget/widget.h"

namespace views {

namespace {

typedef ViewsTestBase BubbleViewBasicTest;

TEST_F(BubbleViewBasicTest, CreateArrowBubble) {
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView();
  scoped_ptr<Widget> bubble_widget(
      views::BubbleDelegateView::CreateBubble(bubble_delegate, NULL));

  BubbleBorder* border = static_cast<BubbleBorder*>(
      bubble_widget->non_client_view()->frame_view()->border());
  EXPECT_EQ(bubble_delegate->GetArrowLocation(), border->arrow_location());
  bubble_widget->CloseNow();
  bubble_widget.reset();
  RunPendingMessages();
}

}  // namespace

}  // namespace views

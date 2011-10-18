// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_delegate.h"

#include "views/bubble/bubble_frame_view.h"
#include "views/bubble/bubble_view.h"
#include "views/widget/widget.h"

namespace views {

BubbleDelegateView::~BubbleDelegateView() {}

// static
Widget* BubbleDelegateView::CreateBubble(BubbleDelegateView* bubble_delegate,
                                         Widget* parent_widget) {
  bubble_delegate->Init();
  views::Widget* bubble_widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  params.delegate = bubble_delegate;
  params.transparent = true;
  if (!parent_widget)
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent_widget = parent_widget;
  bubble_widget->Init(params);
  bubble_widget->SetBounds(bubble_delegate->GetBubbleBounds());
  return bubble_widget;
}

ClientView* BubbleDelegateView::CreateClientView(Widget* widget) {
  return new BubbleView(widget, GetContentsView());
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView() {
  return new BubbleFrameView(GetArrowLocation(),
                             GetPreferredSize(),
                             GetColor());
}

gfx::Point BubbleDelegateView::GetAnchorPoint() const {
  return gfx::Point();
}

const BubbleView* BubbleDelegateView::GetBubbleView() const {
  return GetWidget()->client_view()->AsBubbleView();
}

const BubbleFrameView* BubbleDelegateView::GetBubbleFrameView() const {
  return static_cast<BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
}

gfx::Rect BubbleDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  return GetBubbleFrameView()->GetWindowBoundsForClientBounds(
      gfx::Rect(GetAnchorPoint(), GetPreferredSize()));
}

}  // namespace views

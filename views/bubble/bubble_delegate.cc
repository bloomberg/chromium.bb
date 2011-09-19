// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_delegate.h"
#include "views/bubble/bubble_frame_view.h"
#include "views/bubble/bubble_view.h"

#include "base/logging.h"
#include "views/widget/widget.h"

namespace views {

BubbleDelegate* BubbleDelegate::AsBubbleDelegate() { return this; }

ClientView* BubbleDelegate::CreateClientView(Widget* widget) {
  BubbleView* bubble_view = new BubbleView(widget, GetContentsView());
  bubble_view->SetBounds(0, 0, GetBounds().width(), GetBounds().height());
  if (widget->GetFocusManager()) {
    widget->GetFocusManager()->RegisterAccelerator(
        views::Accelerator(ui::VKEY_ESCAPE, false, false, false),
        bubble_view);
  }
  return bubble_view;
}

NonClientFrameView* BubbleDelegate::CreateNonClientFrameView() {
  return new BubbleFrameView(GetWidget(),
                             GetBounds(),
                             GetFrameBackgroundColor(),
                             GetFrameArrowLocation());
}

const BubbleView* BubbleDelegate::GetBubbleView() const {
  return GetWidget()->client_view()->AsBubbleView();
}

BubbleView* BubbleDelegate::GetBubbleView() {
  return GetWidget()->client_view()->AsBubbleView();
}

const BubbleFrameView* BubbleDelegate::GetBubbleFrameView() const {
  return static_cast<BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
}

BubbleFrameView* BubbleDelegate::GetBubbleFrameView() {
  return static_cast<BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
}

BubbleDelegateView::BubbleDelegateView(Widget* frame):frame_(frame) {
}

BubbleDelegateView::~BubbleDelegateView() {
}

Widget* BubbleDelegateView::GetWidget() {
  return frame_;
}

const Widget* BubbleDelegateView::GetWidget() const {
  return frame_;
}

}  // namespace views

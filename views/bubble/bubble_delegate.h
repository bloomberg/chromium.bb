// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#define VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#pragma once

#include "views/bubble/bubble_border.h"
#include "views/widget/widget_delegate.h"

namespace views {

class BubbleFrameView;
class BubbleView;

// BubbleDelegateView creates frame and client views for bubble Widgets.
// BubbleDelegateView itself is the client's contents view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleDelegateView : public WidgetDelegateView {
 public:
  virtual ~BubbleDelegateView();

  // Create a bubble Widget from the argument BubbleDelegateView.
  static Widget* CreateBubble(BubbleDelegateView* bubble_delegate,
                              Widget* parent_widget);

  // WidgetDelegate overrides:
  virtual View* GetContentsView() OVERRIDE { return this; }
  virtual ClientView* CreateClientView(Widget* widget) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Get the arrow's anchor point in screen space.
  virtual gfx::Point GetAnchorPoint() const;

  // Get the arrow's location on the bubble.
  virtual BubbleBorder::ArrowLocation GetArrowLocation() const {
    return BubbleBorder::TOP_LEFT;
  }

  // Get the color used for the background and border.
  virtual SkColor GetColor() const { return SK_ColorWHITE; }

 protected:
  // Perform view initialization on the contents for bubble sizing.
  virtual void Init() {}

 private:
  const BubbleView* GetBubbleView() const;
  const BubbleFrameView* GetBubbleFrameView() const;

  // Get bubble bounds from the anchor point and client view's preferred size.
  gfx::Rect GetBubbleBounds();
};

}  // namespace views

#endif  // VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

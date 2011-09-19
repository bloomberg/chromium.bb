// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#define VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#pragma once

#include <string>

#include "views/bubble/bubble_border.h"
#include "views/view.h"
#include "views/widget/widget_delegate.h"

namespace views {
class BubbleBorder;
class BubbleFrameView;
class BubbleView;
class View;

// BubbleDelegate interface to create bubble frame view and bubble client view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleDelegate : public WidgetDelegate {
 public:
  virtual BubbleDelegate* AsBubbleDelegate() OVERRIDE;
  virtual ClientView* CreateClientView(Widget* widget) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  virtual SkColor GetFrameBackgroundColor() = 0;
  virtual gfx::Rect GetBounds() = 0;
  virtual BubbleBorder::ArrowLocation GetFrameArrowLocation() = 0;

  const BubbleView* GetBubbleView() const;
  BubbleView* GetBubbleView();

  const BubbleFrameView* GetBubbleFrameView() const;
  BubbleFrameView* GetBubbleFrameView();

 protected:
  virtual ~BubbleDelegate() {}
};

// BubbleDelegateView to create bubble frame view and bubble client view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleDelegateView : public BubbleDelegate, public View {
 public:
  explicit BubbleDelegateView(Widget* frame);
  virtual ~BubbleDelegateView();

  // Overridden from WidgetDelegate:
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;

 private:
  Widget* frame_;
  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateView);
};

}  // namespace views

#endif  // VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

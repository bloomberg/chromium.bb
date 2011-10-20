// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#define VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#pragma once

#include "ui/base/animation/animation_delegate.h"
#include "views/bubble/bubble_border.h"
#include "views/widget/widget_delegate.h"

namespace ui {
class SlideAnimation;
}  // namespace ui

namespace views {

class BubbleFrameView;
class BubbleView;

// BubbleDelegateView creates frame and client views for bubble Widgets.
// BubbleDelegateView itself is the client's contents view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleDelegateView : public WidgetDelegateView,
                                        public ui::AnimationDelegate {
 public:
  BubbleDelegateView();
  BubbleDelegateView(const gfx::Point& anchor_point,
                     BubbleBorder::ArrowLocation arrow_location,
                     const SkColor& color);
  virtual ~BubbleDelegateView();

  // Create a bubble Widget from the argument BubbleDelegateView.
  static Widget* CreateBubble(BubbleDelegateView* bubble_delegate,
                              Widget* parent_widget);

  // WidgetDelegate overrides:
  virtual View* GetInitiallyFocusedView() OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual ClientView* CreateClientView(Widget* widget) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  bool close_on_esc() const { return close_on_esc_; }
  void set_close_on_esc(bool close_on_esc) { close_on_esc_ = close_on_esc; }

  // Get the arrow's anchor point in screen space.
  virtual gfx::Point GetAnchorPoint() const;

  // Get the arrow's location on the bubble.
  virtual BubbleBorder::ArrowLocation GetArrowLocation() const;

  // Get the color used for the background and border.
  virtual SkColor GetColor() const;

  // Fade the bubble in or out via Widget transparency.
  // Fade in calls Widget::Show; fade out calls Widget::Close upon completion.
  void StartFade(bool fade_in);

 protected:
  // View overrides:
  virtual bool AcceleratorPressed(const Accelerator& accelerator) OVERRIDE;

  // Perform view initialization on the contents for bubble sizing.
  virtual void Init();

 private:
  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);

  const BubbleView* GetBubbleView() const;
  const BubbleFrameView* GetBubbleFrameView() const;

  // Get bubble bounds from the anchor point and client view's preferred size.
  gfx::Rect GetBubbleBounds();

  // Fade animation for bubble.
  scoped_ptr<ui::SlideAnimation> fade_animation_;

  // Should this bubble close on the escape key?
  bool close_on_esc_;

  // The screen point where this bubble's arrow is anchored.
  gfx::Point anchor_point_;

  // The arrow's location on the bubble.
  BubbleBorder::ArrowLocation arrow_location_;

  // The background color of the bubble.
  SkColor color_;
};

}  // namespace views

#endif  // VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

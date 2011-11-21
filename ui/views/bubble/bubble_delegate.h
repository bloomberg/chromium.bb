// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {
class SlideAnimation;
}  // namespace ui

namespace views {

class BubbleFrameView;

// BubbleDelegateView creates frame and client views for bubble Widgets.
// BubbleDelegateView itself is the client's contents view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleDelegateView : public WidgetDelegateView,
                                        public ui::AnimationDelegate,
                                        public Widget::Observer {
 public:
  BubbleDelegateView();
  BubbleDelegateView(View* anchor_view,
                     BubbleBorder::ArrowLocation arrow_location,
                     const SkColor& color);
  virtual ~BubbleDelegateView();

  // Create and initialize the bubble Widget(s) with proper bounds.
  static Widget* CreateBubble(BubbleDelegateView* bubble_delegate);

  // WidgetDelegate overrides:
  virtual View* GetInitiallyFocusedView() OVERRIDE;
  virtual BubbleDelegateView* AsBubbleDelegate() OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Widget::Observer overrides:
  virtual void OnWidgetActivationChanged(Widget* widget, bool active) OVERRIDE;

  bool close_on_esc() const { return close_on_esc_; }
  void set_close_on_esc(bool close_on_esc) { close_on_esc_ = close_on_esc; }

  bool close_on_deactivate() const { return close_on_deactivate_; }
  void set_close_on_deactivate(bool close_on_deactivate) {
      close_on_deactivate_ = close_on_deactivate;
  }

  bool allow_bubble_offscreen() const { return allow_bubble_offscreen_; }
  void set_allow_bubble_offscreen(bool allow_bubble_offscreen) {
    allow_bubble_offscreen_ = allow_bubble_offscreen;
  }

  View* anchor_view() const { return anchor_view_; }
  void set_anchor_view(View* anchor_view) { anchor_view_ = anchor_view; }

  bool use_focusless() const { return use_focusless_; }
  void set_use_focusless(bool use_focusless) {
    use_focusless_ = use_focusless;
  }

  // Get the arrow's anchor point in screen space.
  virtual gfx::Point GetAnchorPoint();

  // Get the arrow's location on the bubble.
  virtual BubbleBorder::ArrowLocation GetArrowLocation() const;

  // Get the color used for the background and border.
  virtual SkColor GetColor() const;

  // Show the bubble's widget (and |border_widget_| on Windows).
  void Show();

  // Fade the bubble in or out via Widget transparency.
  // Fade in calls Widget::Show; fade out calls Widget::Close upon completion.
  void StartFade(bool fade_in);

  // Reset fade and opacity of bubble. Restore the opacity of the
  // bubble to the setting before StartFade() was called.
  void ResetFade();

 protected:
  // View overrides:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Perform view initialization on the contents for bubble sizing.
  virtual void Init();

  // Resizes and potentially moves the Bubble to best accommodate the
  // contents preferred size.
  void SizeToContents();

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewBasicTest, NonClientHitTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CreateDelegate);

  BubbleFrameView* GetBubbleFrameView() const;

  // Get bubble bounds from the anchor point and client view's preferred size.
  gfx::Rect GetBubbleBounds();

#if defined(OS_WIN) && !defined(USE_AURA)
  // Get bounds for the Windows-only widget that hosts the bubble's contents.
  gfx::Rect GetBubbleClientBounds() const;
#endif

  // Fade animation for bubble.
  scoped_ptr<ui::SlideAnimation> fade_animation_;

  // Flags controlling bubble closure on the escape key and deactivation.
  bool close_on_esc_;
  bool close_on_deactivate_;

  // Whether the bubble is allowed to be displayed offscreen, or if auto
  // re-positioning should be performed.
  bool allow_bubble_offscreen_;

  // The view hosting this bubble; the arrow is anchored to this view.
  View* anchor_view_;

  // The arrow's location on the bubble.
  BubbleBorder::ArrowLocation arrow_location_;

  // The background color of the bubble.
  SkColor color_;

  // Original opacity of the bubble.
  int original_opacity_;

  // The widget hosting the border for this bubble (non-Aura Windows only).
  Widget* border_widget_;

  // Create a popup window for focusless bubbles on Linux/ChromeOS.
  // These bubbles are not interactive and should not gain focus.
  bool use_focusless_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

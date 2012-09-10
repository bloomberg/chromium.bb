// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_
#define UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

#include "base/gtest_prod_util.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class SlideAnimation;
}

namespace views {

class BubbleFrameView;

// BubbleDelegateView creates frame and client views for bubble Widgets.
// BubbleDelegateView itself is the client's contents view.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT BubbleDelegateView : public WidgetDelegateView,
                                        public ui::AnimationDelegate,
                                        public WidgetObserver {
 public:
  // The default bubble background color.
  static const SkColor kBackgroundColor;

  BubbleDelegateView();
  BubbleDelegateView(View* anchor_view,
                     BubbleBorder::ArrowLocation arrow_location);
  virtual ~BubbleDelegateView();

  // Create and initialize the bubble Widget(s) with proper bounds.
  static Widget* CreateBubble(BubbleDelegateView* bubble_delegate);

  // WidgetDelegate overrides:
  virtual View* GetInitiallyFocusedView() OVERRIDE;
  virtual BubbleDelegateView* AsBubbleDelegate() OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView(Widget* widget) OVERRIDE;

  // WidgetObserver overrides:
  virtual void OnWidgetClosing(Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible) OVERRIDE;
  virtual void OnWidgetActivationChanged(Widget* widget, bool active) OVERRIDE;
  virtual void OnWidgetMoved(Widget* widget) OVERRIDE;

  bool close_on_esc() const { return close_on_esc_; }
  void set_close_on_esc(bool close_on_esc) { close_on_esc_ = close_on_esc; }

  bool close_on_deactivate() const { return close_on_deactivate_; }
  void set_close_on_deactivate(bool close_on_deactivate) {
    close_on_deactivate_ = close_on_deactivate;
  }

  View* anchor_view() const { return anchor_view_; }
  Widget* anchor_widget() const { return anchor_widget_; }

  BubbleBorder::ArrowLocation arrow_location() const { return arrow_location_; }
  void set_arrow_location(BubbleBorder::ArrowLocation arrow_location) {
    arrow_location_ = arrow_location;
  }

  SkColor color() const { return color_; }
  void set_color(SkColor color) { color_ = color; }

  const gfx::Insets& margins() const { return margins_; }
  void set_margins(const gfx::Insets& margins) { margins_ = margins; }

  void set_anchor_insets(const gfx::Insets& insets) { anchor_insets_ = insets; }
  const gfx::Insets& anchor_insets() const { return anchor_insets_; }

  gfx::NativeView parent_window() const { return parent_window_; }
  void set_parent_window(gfx::NativeView window) { parent_window_ = window; }

  bool use_focusless() const { return use_focusless_; }
  void set_use_focusless(bool use_focusless) {
    use_focusless_ = use_focusless;
  }

  bool accept_events() const { return accept_events_; }
  void set_accept_events(bool accept_events) { accept_events_ = accept_events; }

  bool try_mirroring_arrow() const { return try_mirroring_arrow_; }
  void set_try_mirroring_arrow(bool try_mirroring_arrow) {
    try_mirroring_arrow_ = try_mirroring_arrow;
  }

  // Get the arrow's anchor rect in screen space.
  virtual gfx::Rect GetAnchorRect();

  // Show the bubble's widget (and |border_widget_| on Windows).
  void Show();

  // Fade the bubble in or out via Widget transparency.
  // Fade in calls Widget::Show; fade out calls Widget::Close upon completion.
  void StartFade(bool fade_in);

  // Reset fade and opacity of bubble. Restore the opacity of the
  // bubble to the setting before StartFade() was called.
  void ResetFade();

  // Sets the bubble alignment relative to the anchor.
  void SetAlignment(BubbleBorder::BubbleAlignment alignment);

 protected:
  // Get bubble bounds from the anchor point and client view's preferred size.
  virtual gfx::Rect GetBubbleBounds();

  // View overrides:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Perform view initialization on the contents for bubble sizing.
  virtual void Init();

  // Set the anchor view, this (or set_anchor_point) must be done before
  // calling CreateBubble or Show.
  void set_anchor_view(View* anchor_view) { anchor_view_ = anchor_view; }

  // Sets the anchor point used in the absence of an anchor view. This
  // (or set_anchor_view) must be set before calling CreateBubble or Show.
  void set_anchor_point(gfx::Point anchor_point) {
    anchor_point_ = anchor_point;
  }

  bool move_with_anchor() const { return move_with_anchor_; }
  void set_move_with_anchor(bool move_with_anchor) {
    move_with_anchor_ = move_with_anchor;
  }

  // Resizes and potentially moves the Bubble to best accommodate the
  // contents preferred size.
  void SizeToContents();

  BubbleFrameView* GetBubbleFrameView() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleFrameViewTest, NonClientHitTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleDelegateTest, CreateDelegate);

#if defined(OS_WIN) && !defined(USE_AURA)
  // Get bounds for the Windows-only widget that hosts the bubble's contents.
  gfx::Rect GetBubbleClientBounds() const;
#endif

  // Fade animation for bubble.
  scoped_ptr<ui::SlideAnimation> fade_animation_;

  // Flags controlling bubble closure on the escape key and deactivation.
  bool close_on_esc_;
  bool close_on_deactivate_;

  // The view and widget to which this bubble is anchored.
  View* anchor_view_;
  Widget* anchor_widget_;

  // The anchor point used in the absence of an anchor view.
  gfx::Point anchor_point_;

  // If true, the bubble will re-anchor (and may resize) with |anchor_widget_|.
  bool move_with_anchor_;

  // The arrow's location on the bubble.
  BubbleBorder::ArrowLocation arrow_location_;

  // The background color of the bubble.
  SkColor color_;

  // The margins between the content and the inside of the border.
  gfx::Insets margins_;

  // Insets applied to the |anchor_view_| bounds.
  gfx::Insets anchor_insets_;

  // Original opacity of the bubble.
  int original_opacity_;

  // The widget hosting the border for this bubble (non-Aura Windows only).
  Widget* border_widget_;

  // Create a popup window for focusless bubbles on Linux/ChromeOS.
  // These bubbles are not interactive and should not gain focus.
  bool use_focusless_;

  // Specifies whether the popup accepts events or lets them pass through.
  bool accept_events_;

  // If true (defaults to true), the arrow may be mirrored to fit the
  // bubble on screen better.
  bool try_mirroring_arrow_;

  // Parent native window of the bubble.
  gfx::NativeView parent_window_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

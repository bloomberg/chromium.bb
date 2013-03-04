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

namespace gfx {
class Rect;
}

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
  BubbleDelegateView();
  BubbleDelegateView(View* anchor_view,
                     BubbleBorder::ArrowLocation arrow_location);
  virtual ~BubbleDelegateView();

  // Create and initialize the bubble Widget(s) with proper bounds.
  static Widget* CreateBubble(BubbleDelegateView* bubble_delegate);

  // WidgetDelegate overrides:
  virtual BubbleDelegateView* AsBubbleDelegate() OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView(Widget* widget) OVERRIDE;

  // WidgetObserver overrides:
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible) OVERRIDE;
  virtual void OnWidgetActivationChanged(Widget* widget, bool active) OVERRIDE;
  virtual void OnWidgetBoundsChanged(Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  bool close_on_esc() const { return close_on_esc_; }
  void set_close_on_esc(bool close_on_esc) { close_on_esc_ = close_on_esc; }

  bool close_on_deactivate() const { return close_on_deactivate_; }
  void set_close_on_deactivate(bool close_on_deactivate) {
    close_on_deactivate_ = close_on_deactivate;
  }

  View* anchor_view() const { return anchor_view_; }
  Widget* anchor_widget() const { return anchor_widget_; }

  // The anchor point is used in the absence of an anchor view.
  const gfx::Point& anchor_point() const { return anchor_point_; }

  BubbleBorder::ArrowLocation arrow_location() const { return arrow_location_; }
  void set_arrow_location(BubbleBorder::ArrowLocation arrow_location) {
    arrow_location_ = arrow_location;
  }

  BubbleBorder::Shadow shadow() const { return shadow_; }
  void set_shadow(BubbleBorder::Shadow shadow) { shadow_ = shadow; }

  SkColor color() const { return color_; }
  void set_color(SkColor color) {
    color_ = color;
    color_explicitly_set_ = true;
  }

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

  bool border_accepts_events() const { return border_accepts_events_; }
  void set_border_accepts_events(bool accept) {
    border_accepts_events_ = accept;
  }

  bool adjust_if_offscreen() const { return adjust_if_offscreen_; }
  void set_adjust_if_offscreen(bool adjust) { adjust_if_offscreen_ = adjust; }

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

  // Sets the bubble alignment relative to the anchor. This may only be called
  // after calling CreateBubble.
  void SetAlignment(BubbleBorder::BubbleAlignment alignment);

  // Create the bubble frame for the bubble.
  virtual BubbleFrameView* CreateBubbleFrameView();

 protected:
  // Get bubble bounds from the anchor point and client view's preferred size.
  virtual gfx::Rect GetBubbleBounds();

  // View overrides:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Perform view initialization on the contents for bubble sizing.
  virtual void Init();

  // Set the anchor view, this (or set_anchor_point) must be done before
  // calling CreateBubble or Show.
  void set_anchor_view(View* anchor_view) { anchor_view_ = anchor_view; }

  // The anchor point or anchor view must be set before calling CreateBubble or
  // Show.
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

  // Update the bubble color from |theme|, unless it was explicitly set.
  void UpdateColorsFromTheme(const ui::NativeTheme* theme);

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

  // Bubble border shadow to use.
  BubbleBorder::Shadow shadow_;

  // The background color of the bubble; and flag for when it's explicitly set.
  SkColor color_;
  bool color_explicitly_set_;

  // The margins between the content and the inside of the border.
  gfx::Insets margins_;

  // Insets applied to the |anchor_view_| bounds.
  gfx::Insets anchor_insets_;

  // Original opacity of the bubble.
  int original_opacity_;

  // The widget hosting the border for this bubble (non-Aura Windows only).
  Widget* border_widget_;

  // If true (defaults to false), the bubble does not take user focus upon
  // display.
  bool use_focusless_;

  // Specifies whether the popup accepts events or lets them pass through.
  bool accept_events_;

  // Specifies whether the bubble border accepts events or lets them pass
  // through.
  bool border_accepts_events_;

  // If true (defaults to true), the arrow may be mirrored and moved to fit the
  // bubble on screen better. It would be a no-op if the bubble has no arrow.
  bool adjust_if_offscreen_;

  // Parent native window of the bubble.
  gfx::NativeView parent_window_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_DELEGATE_H_

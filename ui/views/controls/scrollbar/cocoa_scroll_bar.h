// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_COCOA_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_COCOA_SCROLL_BAR_H_

#include "base/macros.h"
#import "base/mac/scoped_nsobject.h"
#import "ui/views/cocoa/views_scrollbar_bridge.h"
#include "ui/views/controls/scrollbar/base_scroll_bar.h"
#include "ui/views/views_export.h"

namespace views {

// The transparent scrollbar for Mac which overlays its contents.
class VIEWS_EXPORT CocoaScrollBar : public BaseScrollBar,
                                    public ViewsScrollbarBridgeDelegate {
 public:
  explicit CocoaScrollBar(bool horizontal);
  ~CocoaScrollBar() override;

  // Called by CocoaScrollBarThumb when the mouse enters or exits the view.
  void OnMouseEnteredScrollbarThumb(const ui::MouseEvent& event);

  // ScrollDelegate:
  bool OnScroll(float dx, float dy) override;

  // ViewsScrollbarBridgeDelegate:
  void OnScrollerStyleChanged() override;

  // Returns the scroller style.
  NSScrollerStyle GetScrollerStyle() const { return scroller_style_; }

 protected:
  // BaseScrollBar:
  gfx::Rect GetTrackBounds() const override;

  // ScrollBar:
  int GetLayoutSize() const override;
  int GetContentOverlapSize() const override;

  // View:
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Methods to change the visibility of the scrollbar.
  void ShowScrollbar();
  void HideScrollbar();

  // Scroller style the scrollbar is using.
  NSScrollerStyle scroller_style_;

  // Timer that will start the scrollbar's hiding animation when it reaches 0.
  base::Timer hide_scrollbar_timer_;

  // True when the scrolltrack should be drawn.
  bool has_scrolltrack_;

  // The bridge for NSScroller.
  base::scoped_nsobject<ViewsScrollbarBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(CocoaScrollBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_COCOA_SCROLL_BAR_H_

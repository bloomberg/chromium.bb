// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_

#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/scrollbar/base_scroll_bar.h"

namespace views {

// The transparent scrollbar which overlays its contents.
class VIEWS_EXPORT OverlayScrollBar : public BaseScrollBar {
 public:
  explicit OverlayScrollBar(bool horizontal);
  virtual ~OverlayScrollBar();

 protected:
  // BaseScrollBar overrides:
  virtual gfx::Rect GetTrackBounds() const override;
  virtual void OnGestureEvent(ui::GestureEvent* event) override;

  // ScrollBar overrides:
  virtual int GetLayoutSize() const override;
  virtual int GetContentOverlapSize() const override;
  virtual void OnMouseEnteredScrollView(const ui::MouseEvent& event) override;
  virtual void OnMouseExitedScrollView(const ui::MouseEvent& event) override;

  // View overrides:
  virtual gfx::Size GetPreferredSize() const override;
  virtual void Layout() override;
  virtual void OnPaint(gfx::Canvas* canvas) override;

 private:
  gfx::SlideAnimation animation_;
  DISALLOW_COPY_AND_ASSIGN(OverlayScrollBar);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_OVERLAY_SCROLL_BAR_H_

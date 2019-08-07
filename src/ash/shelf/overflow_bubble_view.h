// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_bubble.h"
#include "base/macros.h"

namespace ash {
class Shelf;
class ShelfView;

// OverflowBubbleView hosts a ShelfView to display overflown items.
// Exports to access this class from OverflowBubbleViewTestAPI.
class ASH_EXPORT OverflowBubbleView : public ShelfBubble {
 public:
  // |anchor| is the overflow button on the main shelf. |shelf_view| is the
  // ShelfView containing the overflow items.
  OverflowBubbleView(ShelfView* shelf_view,
                     views::View* anchor,
                     SkColor background_color);
  ~OverflowBubbleView() override;

  // Handles events for scrolling the bubble. Returns whether the event
  // has been consumed.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  // These return the actual offset (sometimes reduced by the clamping).
  int ScrollByXOffset(int x_offset);
  int ScrollByYOffset(int y_offset);

  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  bool CanActivate() const override;

  ShelfView* shelf_view() { return shelf_view_; }

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

 private:
  friend class OverflowBubbleViewTestAPI;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  const char* GetClassName() const override;

  // ui::EventHandler:
  void OnScrollEvent(ui::ScrollEvent* event) override;

  Shelf* shelf_;
  ShelfView* shelf_view_;  // Owned by views hierarchy.
  gfx::Vector2d scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_

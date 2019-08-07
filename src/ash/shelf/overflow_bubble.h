// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
class OverflowBubbleView;
class OverflowButton;
class Shelf;
class ShelfView;

// OverflowBubble shows shelf items that won't fit on the main shelf in a
// separate bubble.
class ASH_EXPORT OverflowBubble : public ui::EventHandler,
                                  public views::WidgetObserver {
 public:
  // |shelf| is the shelf that spawns the bubble.
  explicit OverflowBubble(Shelf* shelf);
  ~OverflowBubble() override;

  // Shows an bubble pointing to |overflow_button| with |shelf_view| as its
  // content.  This |shelf_view| is different than the main shelf's view and
  // only contains the overflow items.
  void Show(OverflowButton* overflow_button, ShelfView* shelf_view);

  void Hide();

  bool IsShowing() const { return !!bubble_; }
  OverflowBubbleView* bubble_view() { return bubble_; }

 private:
  void ProcessPressedEvent(ui::LocatedEvent* event);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  Shelf* shelf_;
  OverflowBubbleView* bubble_;       // Owned by views hierarchy.
  OverflowButton* overflow_button_;  // Owned by ShelfView.

  DISALLOW_COPY_AND_ASSIGN(OverflowBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_H_

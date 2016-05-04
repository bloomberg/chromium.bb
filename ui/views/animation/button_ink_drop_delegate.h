// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_BUTTON_INK_DROP_DELEGATE_H_
#define UI_VIEWS_ANIMATION_BUTTON_INK_DROP_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop_delegate.h"
#include "ui/views/views_export.h"

namespace ui {
class ScopedTargetHandler;
}

namespace views {

class InkDrop;
class InkDropHost;
class View;

// An InkDropDelegate that handles animations for things that act like buttons.
class VIEWS_EXPORT ButtonInkDropDelegate : public InkDropDelegate,
                                           public ui::EventHandler {
 public:
  ButtonInkDropDelegate(InkDropHost* ink_drop_host, View* view);
  ~ButtonInkDropDelegate() override;

  const gfx::Point& last_ink_drop_location() const {
    return last_ink_drop_location_;
  }
  void set_last_ink_drop_location(const gfx::Point& point) {
    last_ink_drop_location_ = point;
  }

  // InkDropDelegate:
  void OnAction(InkDropState state) override;
  void SnapToActivated() override;
  void SetHovered(bool is_hovered) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // An instance of ScopedTargetHandler allowing |this| to handle events.
  std::unique_ptr<ui::ScopedTargetHandler> target_handler_;

  // Parent InkDropHost (typically a View) that hosts the ink ripple animations.
  InkDropHost* ink_drop_host_;

  // Location of the last ink drop triggering event in coordinate system of the
  // ctor argument |view|.
  gfx::Point last_ink_drop_location_;

  // Animation controller for the ink drop ripple effect.
  std::unique_ptr<InkDrop> ink_drop_;

  DISALLOW_COPY_AND_ASSIGN(ButtonInkDropDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_BUTTON_INK_DROP_DELEGATE_H_

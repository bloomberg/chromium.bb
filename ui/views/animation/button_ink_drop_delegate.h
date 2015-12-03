// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_BUTTON_INK_DROP_DELEGATE_H_
#define UI_VIEWS_ANIMATION_BUTTON_INK_DROP_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/views/animation/ink_drop_delegate.h"
#include "ui/views/views_export.h"

namespace ui {
class ScopedTargetHandler;
}

namespace views {

class InkDropAnimationController;
class InkDropHost;
class View;

// An InkDropDelegate that handles animations for toolbar buttons.
class VIEWS_EXPORT ButtonInkDropDelegate : public InkDropDelegate,
                                           public ui::EventHandler {
 public:
  ButtonInkDropDelegate(InkDropHost* ink_drop_host, View* view);
  ~ButtonInkDropDelegate() override;

  // InkDropDelegate:
  void SetInkDropSize(int large_size,
                      int large_corner_radius,
                      int small_size,
                      int small_corner_radius) override;
  void OnLayout() override;
  void OnAction(InkDropState state) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // An instance of ScopedTargetHandler allowing |this| to handle events.
  scoped_ptr<ui::ScopedTargetHandler> target_handler_;

  // Parent InkDropHost (typically a View) that hosts the ink ripple animations.
  InkDropHost* ink_drop_host_;

  // Animation controller for the ink drop ripple effect.
  scoped_ptr<InkDropAnimationController> ink_drop_animation_controller_;

  DISALLOW_COPY_AND_ASSIGN(ButtonInkDropDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_BUTTON_INK_DROP_DELEGATE_H_

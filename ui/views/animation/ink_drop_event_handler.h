// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_EVENT_HANDLER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_EVENT_HANDLER_H_

#include <memory>

#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace ui {
class LocatedEvent;
class ScopedTargetHandler;
}  // namespace ui

namespace views {
class View;
class InkDrop;
enum class InkDropState;

// This class handles ink-drop changes due to events on its host.
class VIEWS_EXPORT InkDropEventHandler : public ui::EventHandler {
 public:
  // Delegate class that allows InkDropEventHandler to be used with InkDrops
  // that are hosted in multiple ways.
  class Delegate {
   public:
    // Gets the InkDrop (or stub) that should react to incoming events.
    virtual InkDrop* GetInkDrop() = 0;
    // Start animating the InkDrop to another target state.
    // TODO(pbos): Consider moving the implementation of
    // InkDropHostView::AnimateInkDrop into InkDropEventHandler. In this case
    // InkDropHostView would forward AnimateInkDrop into
    // InkDropEventHandler::AnimateInkDrop.
    virtual void AnimateInkDrop(InkDropState state,
                                const ui::LocatedEvent* event) = 0;
    // Returns true if gesture events should affect the InkDrop.
    virtual bool SupportsGestureEvents() const = 0;
  };

  InkDropEventHandler(View* host_view, Delegate* delegate);
  ~InkDropEventHandler() override;

 private:
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Allows |this| to handle all GestureEvents on |host_view_|.
  std::unique_ptr<ui::ScopedTargetHandler> target_handler_;

  // The host view.
  View* const host_view_;

  // Delegate used to get the InkDrop, etc.
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(InkDropEventHandler);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_EVENT_HANDLER_H_

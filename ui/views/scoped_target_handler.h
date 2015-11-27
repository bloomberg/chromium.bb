// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_SCOPED_TARGET_HANDLER_H_
#define UI_VIEWS_SCOPED_TARGET_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// An EventHandler that replaces an View's target handler with itself to pass
// events first to the original handlers and second to an additional new
// EventHandler. The new handler gets called after the original handlers even
// if they call SetHandled() or StopPropagation() on the event.
class VIEWS_EXPORT ScopedTargetHandler : public ui::EventHandler {
 public:
  ScopedTargetHandler(View* view, ui::EventHandler* new_handler);
  ~ScopedTargetHandler() override;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnCancelMode(ui::CancelModeEvent* event) override;

 private:
  // If non-null the destructor sets this to true. This is set while handling
  // an event and used to detect if |this| has been deleted.
  bool* destroyed_flag_;

  // An View that has its target handler replaced with |this| for a life time of
  // |this|.
  View* view_;

  // An EventHandler that gets restored on |view_| when |this| is destroyed.
  ui::EventHandler* original_handler_;

  // A new handler that gets events in addition to the |original_handler_| or
  // |view_|.
  ui::EventHandler* new_handler_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTargetHandler);
};

}  // namespace views

#endif  // UI_VIEWS_SCOPED_TARGET_HANDLER_H_

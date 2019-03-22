// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_GESTURE_DRAG_HANDLER_H_
#define ASH_WM_IMMERSIVE_GESTURE_DRAG_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace ash {

class TabletModeAppWindowDragController;

// ImmersiveGestureHandler is responsible for calling
// TabletAppModeWindowDragController::DragWindowFromTop() to drag the window
// from the top if CanDrag is true when a gesture is received.
class ASH_EXPORT ImmersiveGestureDragHandler : public ui::EventHandler {
 public:
  explicit ImmersiveGestureDragHandler(aura::Window* window);
  ~ImmersiveGestureDragHandler() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Returns true if the target of |event| can be dragged.
  bool CanDrag(ui::GestureEvent* event);

  std::unique_ptr<TabletModeAppWindowDragController>
      tablet_mode_app_window_drag_controller_;

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveGestureDragHandler);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_GESTURE_DRAG_HANDLER_H_

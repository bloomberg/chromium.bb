// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_CLIENT_SIDE_WINDOW_MOVE_HANDLER_H_
#define UI_AURA_MUS_CLIENT_SIDE_WINDOW_MOVE_HANDLER_H_

#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}

namespace aura {

class Env;
class Window;
class WindowTreeClient;

// ClientSideWindowMoveHandler handles mouse/gesture events and performs the
// window move session when the event is located on draggable area.
class ClientSideWindowMoveHandler : public ui::EventHandler {
 public:
  ClientSideWindowMoveHandler(Env* env, WindowTreeClient* client);
  ~ClientSideWindowMoveHandler() override;

 private:
  // Setup |last_target_| and |last_location_| for |event|, or clear them when
  // the event will not involve window move.
  void MaybeSetupLastTarget(ui::LocatedEvent* event);

  // Updates the resize shadow for |window| to |component| in the window server.
  void UpdateWindowResizeShadow(Window* window, int component);

  // Conduct the window move.
  void MaybePerformWindowMove(ui::LocatedEvent* event,
                              ws::mojom::MoveLoopSource source);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void OnWindowMoveDone(bool success);

  Env* env_;
  WindowTreeClient* client_;
  WindowTracker last_shadow_target_;

  // |last_target_| tracks the toplevel Window that will be the subject of a
  // potential window move.
  WindowTracker last_target_;

  // |dragging_window_| is the window that's currently being moved, if any.
  Window* dragging_window_ = nullptr;

  gfx::Point last_location_;
  int last_component_ = HTNOWHERE;

  DISALLOW_COPY_AND_ASSIGN(ClientSideWindowMoveHandler);
};

}  // namespace aura

#endif  // UI_AURA_MUS_CLIENT_SIDE_WINDOW_MOVE_HANDLER_H_

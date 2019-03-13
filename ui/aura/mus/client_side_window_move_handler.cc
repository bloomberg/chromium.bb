// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_side_window_move_handler.h"

#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace aura {

namespace {

// TODO(estade,mukai): De-dupe this constant and the following logic with
// WmToplevelWindowEventHandler.
constexpr int kDragStartTopEdgeInset = 8;

Window* GetToplevelTargetForEvent(ui::LocatedEvent* event, int* component) {
  DCHECK(!event->handled());
  auto* window = static_cast<Window*>(event->target());

  if (!window || !window->delegate())
    return nullptr;

  *component = window->delegate()->GetNonClientComponent(event->location());

  if (ui::CanPerformDragOrResize(*component)) {
    DCHECK_EQ(window, window->GetToplevelWindow());
    return window;
  }

  // Gestures can sometimes trigger drags from the client area. All other events
  // and hit locations will not trigger a drag.
  if (event->type() != ui::ET_GESTURE_SCROLL_BEGIN || *component != HTCLIENT)
    return nullptr;

  window = window->GetToplevelWindow();
  if (!window->GetRootWindow()->GetProperty(
          client::kGestureDragFromClientAreaTopMovesWindow)) {
    return nullptr;
  }

  if (event->AsGestureEvent()->details().scroll_y_hint() < 0)
    return nullptr;

  const gfx::Point location_in_screen =
      event->target()->GetScreenLocation(*event);
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();

  gfx::Rect hit_bounds_in_screen(work_area_bounds);
  hit_bounds_in_screen.set_height(kDragStartTopEdgeInset);

  // There may be a bezel sensor off screen logically above
  // |hit_bounds_in_screen|. Handles the ET_GESTURE_SCROLL_BEGIN event
  // triggered in the bezel area too.
  bool in_bezel = location_in_screen.y() < hit_bounds_in_screen.y() &&
                  location_in_screen.x() >= hit_bounds_in_screen.x() &&
                  location_in_screen.x() < hit_bounds_in_screen.right();

  if (hit_bounds_in_screen.Contains(location_in_screen) || in_bezel)
    return window;

  return nullptr;
}

int GetHitTestComponent(Window* window, const gfx::Point& location) {
  if (!window->delegate())
    return HTNOWHERE;
  int component = window->delegate()->GetNonClientComponent(location);
  if (!ui::IsResizingComponent(component))
    return HTNOWHERE;
  return component;
}

}  // namespace

ClientSideWindowMoveHandler::ClientSideWindowMoveHandler(
    Env* env,
    WindowTreeClient* client)
    : env_(env), client_(client) {
  env_->AddPreTargetHandler(this);
}

ClientSideWindowMoveHandler::~ClientSideWindowMoveHandler() {
  env_->RemovePreTargetHandler(this);
}

void ClientSideWindowMoveHandler::MaybeSetupLastTarget(
    ui::LocatedEvent* event) {
  // Do nothing in the middle of a dragging session, otherwise some properties
  // (e.g. |last_component_| or |dragging_window_|) might get wrong. See:
  // https://crbug.com/940545.
  if (dragging_window_)
    return;

  last_target_.RemoveAll();

  Window* window = GetToplevelTargetForEvent(event, &last_component_);
  if (!window) {
    last_component_ = HTNOWHERE;
    return;
  }

  last_target_.Add(window);
  last_location_ = event->location();
  UpdateWindowResizeShadow(window, ui::IsResizingComponent(last_component_)
                                       ? last_component_
                                       : HTNOWHERE);
}

void ClientSideWindowMoveHandler::UpdateWindowResizeShadow(Window* window,
                                                           int component) {
  // Do nothing in the middle of a dragging session. See:
  // https://crbug.com/940545.
  if (dragging_window_)
    return;

  client_->SetWindowResizeShadow(window->GetRootWindow(), component);
  last_shadow_target_.RemoveAll();
  last_shadow_target_.Add(window);
}

void ClientSideWindowMoveHandler::MaybePerformWindowMove(
    ui::LocatedEvent* event,
    ws::mojom::MoveLoopSource source) {
  // Do nothing in the middle of a dragging session. See:
  // https://crbug.com/940545.
  if (dragging_window_)
    return;

  Window* target = static_cast<Window*>(event->target());
  if (!last_target_.Contains(target->GetToplevelWindow()))
    return;

  gfx::Point screen_location = last_location_;
  auto* screen_position_client =
      aura::client::GetScreenPositionClient(target->GetRootWindow());
  // screen_position_client may be null in tests.
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(target, &screen_location);

  WindowTreeHostMus::ForWindow(target)->PerformWindowMove(
      target, source, screen_location, last_component_,
      base::BindOnce(&ClientSideWindowMoveHandler::OnWindowMoveDone,
                     base::Unretained(this)));

  // Clear |last_target_| so that event->target() won't match with
  // |last_target_| anymore.
  last_target_.RemoveAll();

  dragging_window_ = target;
  event->SetHandled();
}

void ClientSideWindowMoveHandler::OnMouseEvent(ui::MouseEvent* event) {
  // The logic here should be aligned with ash::WmToplevelWindowEventHandler.
  // TODO(mukai): create a common class in ash/public/cpp to share the logic.
  if (event->handled())
    return;
  if ((event->flags() &
       (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0) {
    return;
  }
  switch (event->type()) {
    case ui::ET_MOUSE_EXITED:
      if (!last_shadow_target_.Contains(
              static_cast<Window*>(event->target())) ||
          last_component_ == HTNOWHERE) {
        return;
      }

      client_->SetWindowResizeShadow(last_shadow_target_.Pop()->GetRootWindow(),
                                     HTNOWHERE);
      last_component_ = HTNOWHERE;
      break;

    case ui::ET_MOUSE_MOVED: {
      if (!event->target()) {
        last_component_ = HTNOWHERE;
        last_shadow_target_.RemoveAll();
        return;
      }
      Window* window = static_cast<Window*>(event->target());
      int component = GetHitTestComponent(window, event->location());
      if (component == last_component_)
        return;
      last_component_ = component;
      UpdateWindowResizeShadow(window, last_component_);
      break;
    }

    case ui::ET_MOUSE_PRESSED:
      MaybeSetupLastTarget(event);
      break;

    case ui::ET_MOUSE_DRAGGED:
      MaybePerformWindowMove(event, ws::mojom::MoveLoopSource::MOUSE);
      break;

    default:
      // Do nothing.
      break;
  }
}

void ClientSideWindowMoveHandler::OnGestureEvent(ui::GestureEvent* event) {
  // The logic here should be aligned with ash::WmToplevelWindowEventHandler.
  // TODO(mukai): create a common class in ash/public/cpp to share the logic.
  if (event->handled())
    return;

  // A drag may have already been started and gesture events transferred, but
  // in-flight gesture events may still come through, targetting the original
  // window. These need to be set to handled so that the client area doesn't
  // complain about unexpected scroll updates or ends.
  if (dragging_window_) {
    event->SetHandled();
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      MaybeSetupLastTarget(event);
      if (!last_target_.windows().empty())
        event->SetHandled();
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      MaybePerformWindowMove(event, ws::mojom::MoveLoopSource::TOUCH);
      break;

    default:
      // Do nothing.
      break;
  }
}

void ClientSideWindowMoveHandler::OnWindowMoveDone(bool success) {
  dragging_window_ = nullptr;
}

}  // namespace aura

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_side_window_move_handler.h"

#include "base/bind.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace aura {

namespace {

void WindowMoveEnded(Window* window, bool success) {
  window->env()->gesture_recognizer()->CancelActiveTouches(window);
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
  last_target_.RemoveAll();
  Window* window = static_cast<Window*>(event->target());
  if (!window || !window->delegate())
    return;
  int component = window->delegate()->GetNonClientComponent(event->location());
  if (!ui::CanPerformDragOrResize(component))
    return;

  last_target_.Add(window);
  last_location_ = event->location();
  last_component_ = component;
}

void ClientSideWindowMoveHandler::MaybePerformWindowMove(
    ui::LocatedEvent* event,
    ws::mojom::MoveLoopSource source) {
  Window* target = static_cast<Window*>(event->target());
  if (!target || !last_target_.Contains(target) || !target->delegate())
    return;

  gfx::Point screen_location = last_location_;
  auto* screen_position_client =
      aura::client::GetScreenPositionClient(target->GetRootWindow());
  // screen_position_client may be null.in test.
  if (screen_position_client)
    screen_position_client->ConvertPointToScreen(target, &screen_location);
  WindowTreeHostMus::ForWindow(target)->PerformWindowMove(
      target, source, screen_location, last_component_,
      base::BindOnce(&WindowMoveEnded, target));

  // Clear |last_target_| so that event->target() won't match with
  // |last_target_| anymore.
  last_target_.RemoveAll();
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
      if (last_component_ != HTNOWHERE &&
          !last_shadow_target_.windows().empty()) {
        client_->SetWindowResizeShadow(last_shadow_target_.Pop(), HTNOWHERE);
      }
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
      client_->SetWindowResizeShadow(window->GetRootWindow(), last_component_);
      last_shadow_target_.RemoveAll();
      last_shadow_target_.Add(window->GetRootWindow());
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
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      MaybeSetupLastTarget(event);
      return;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      MaybePerformWindowMove(event, ws::mojom::MoveLoopSource::TOUCH);
      break;

    default:
      // Do nothing.
      break;
  }
}

}  // namespace aura

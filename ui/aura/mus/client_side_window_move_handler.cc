// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_side_window_move_handler.h"

#include "base/bind.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
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

}  // namespace

ClientSideWindowMoveHandler::ClientSideWindowMoveHandler(Env* env) : env_(env) {
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

  // TODO(mukai): add the support of window resizing components like HTTOP.
  if (component != HTCAPTION)
    return;

  last_target_.Add(window);
  last_location_ = event->location();
}

void ClientSideWindowMoveHandler::MaybePerformWindowMove(
    ui::LocatedEvent* event,
    ws::mojom::MoveLoopSource source) {
  Window* target = static_cast<Window*>(event->target());
  if (!target || !last_target_.Contains(target) || !target->delegate())
    return;

  gfx::Point screen_location = last_location_;
  aura::client::GetScreenPositionClient(target->GetRootWindow())
      ->ConvertPointToScreen(target, &screen_location);
  WindowTreeHostMus::ForWindow(target)->PerformWindowMove(
      target, source, screen_location,
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

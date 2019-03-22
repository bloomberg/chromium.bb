// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/gesture_recognizer_impl_mus.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace aura {

GestureRecognizerImplMus::GestureRecognizerImplMus(
    aura::WindowTreeClient* client)
    : client_(client) {
  client->AddObserver(this);
}

GestureRecognizerImplMus::~GestureRecognizerImplMus() {
  OnWindowMoveEnded(false);
  if (client_)
    client_->RemoveObserver(this);
}

void GestureRecognizerImplMus::OnWillDestroyClient(
    aura::WindowTreeClient* client) {
  DCHECK_EQ(client_, client);
  OnWindowMoveEnded(false);
  client_->RemoveObserver(this);
  client_ = nullptr;
}

void GestureRecognizerImplMus::OnWindowMoveStarted(
    aura::Window* window,
    const gfx::Point& cursor_location,
    ws::mojom::MoveLoopSource source) {
  DCHECK(!moving_window_);
  if (source != ws::mojom::MoveLoopSource::TOUCH)
    return;
  moving_window_ = window;
  cursor_offset_ = cursor_location - window->GetBoundsInScreen().origin();
}

void GestureRecognizerImplMus::OnWindowMoveEnded(bool success) {
  moving_window_ = nullptr;
}

bool GestureRecognizerImplMus::GetLastTouchPointForTarget(
    ui::GestureConsumer* consumer,
    gfx::PointF* point) {
  // When a window is moving, the touch events are handled completely within the
  // shell and do not come to the client and so the default
  // GetLastTouchPointForTarget won't work. Instead, this reports the last
  // location through PointerWatcher. See also
  // https://docs.google.com/document/d/1AKeK8IuF-j2TJ-2sPsewORXdjnr6oAzy5nnR1zwrsfc/edit#
  aura::Window* target_window = static_cast<aura::Window*>(consumer);
  if (moving_window_ && moving_window_->Contains(target_window)) {
    aura::client::ScreenPositionClient* client =
        aura::client::GetScreenPositionClient(target_window->GetRootWindow());
    if (client) {
      // Use the original offset when the window move started. ui::EventObserver
      // isn't used since its OnEvent may be called slightly later than window
      // move (bounds change) is conducted. See crbug.com/901540.
      point->set_x(cursor_offset_.x());
      point->set_y(cursor_offset_.y());
      return true;
    }
  }
  return GestureRecognizerImpl::GetLastTouchPointForTarget(consumer, point);
}

}  // namespace aura

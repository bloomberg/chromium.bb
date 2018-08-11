// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service_delegate.h"

namespace ui {
namespace ws2 {

bool WindowServiceDelegate::StoreAndSetCursor(aura::Window* window,
                                              ui::Cursor cursor) {
  return false;
}

void WindowServiceDelegate::RunWindowMoveLoop(aura::Window* window,
                                              mojom::MoveLoopSource source,
                                              const gfx::Point& cursor,
                                              DoneCallback callback) {
  std::move(callback).Run(false);
}

void WindowServiceDelegate::RunDragLoop(
    aura::Window* window,
    const ui::OSExchangeData& data,
    const gfx::Point& screen_location,
    uint32_t drag_operation,
    ui::DragDropTypes::DragEventSource source,
    DragDropCompletedCallback callback) {
  std::move(callback).Run(ui::DragDropTypes::DRAG_NONE);
}

SystemInputInjector* WindowServiceDelegate::GetSystemInputInjector() {
  return nullptr;
}

aura::WindowTreeHost* WindowServiceDelegate::GetWindowTreeHostForDisplayId(
    int64_t display_id) {
  return nullptr;
}

aura::Window* WindowServiceDelegate::GetTopmostWindowAtPoint(
    const gfx::Point& location_in_screen,
    const std::set<aura::Window*>& ignore,
    aura::Window** real_topmost) {
  return nullptr;
}

}  // namespace ws2
}  // namespace ui

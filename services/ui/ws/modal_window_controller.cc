// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/modal_window_controller.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "services/ui/ws/event_dispatcher.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_drawn_tracker.h"

namespace ui {
namespace ws {
namespace {

const ServerWindow* GetModalChildForWindowAncestor(const ServerWindow* window) {
  for (const ServerWindow* ancestor = window; ancestor;
       ancestor = ancestor->parent()) {
    for (auto* transient_child : ancestor->transient_children()) {
      if (transient_child->modal_type() != MODAL_TYPE_NONE &&
          transient_child->IsDrawn())
        return transient_child;
    }
  }
  return nullptr;
}

const ServerWindow* GetWindowModalTargetForWindow(const ServerWindow* window) {
  const ServerWindow* modal_window = GetModalChildForWindowAncestor(window);
  if (!modal_window)
    return window;
  return GetWindowModalTargetForWindow(modal_window);
}

}  // namespace

ModalWindowController::ModalWindowController(EventDispatcher* event_dispatcher)
    : event_dispatcher_(event_dispatcher) {}

ModalWindowController::~ModalWindowController() {
  for (auto it = system_modal_windows_.begin();
       it != system_modal_windows_.end(); it++) {
    (*it)->RemoveObserver(this);
  }
}

void ModalWindowController::AddSystemModalWindow(ServerWindow* window) {
  DCHECK(window);
  DCHECK(!base::ContainsValue(system_modal_windows_, window));

  window->SetModalType(ui::MODAL_TYPE_SYSTEM);
  system_modal_windows_.push_back(window);
  window_drawn_trackers_.insert(make_pair(
      window, base::MakeUnique<ServerWindowDrawnTracker>(window, this)));
  window->AddObserver(this);

  event_dispatcher_->ReleaseCaptureBlockedByModalWindow(window);
}

bool ModalWindowController::IsWindowBlockedBy(
    const ServerWindow* window,
    const ServerWindow* modal_window) const {
  DCHECK(window);
  DCHECK(modal_window);
  if (modal_window->modal_type() == MODAL_TYPE_NONE ||
      !modal_window->IsDrawn()) {
    return false;
  }

  if (modal_window->transient_parent() &&
      !modal_window->transient_parent()->Contains(window)) {
    return false;
  }

  return true;
}

bool ModalWindowController::IsWindowBlocked(const ServerWindow* window) const {
  DCHECK(window);
  return GetTargetForWindow(window) != window;
}

const ServerWindow* ModalWindowController::GetTargetForWindow(
    const ServerWindow* window) const {
  // TODO(moshayedi): crbug.com/697127. Handle MODAL_TYPE_CHILD.
  ServerWindow* system_modal_window = GetActiveSystemModalWindow();
  if (system_modal_window)
    return system_modal_window->Contains(window) ? window : system_modal_window;

  return window ? GetWindowModalTargetForWindow(window) : nullptr;
}

ServerWindow* ModalWindowController::GetActiveSystemModalWindow() const {
  for (auto it = system_modal_windows_.rbegin();
       it != system_modal_windows_.rend(); it++) {
    ServerWindow* modal = *it;
    if (modal->IsDrawn())
      return modal;
  }
  return nullptr;
}

void ModalWindowController::RemoveWindow(ServerWindow* window) {
  window->RemoveObserver(this);
  auto it = std::find(system_modal_windows_.begin(),
                      system_modal_windows_.end(), window);
  DCHECK(it != system_modal_windows_.end());
  system_modal_windows_.erase(it);
  window_drawn_trackers_.erase(window);
}

void ModalWindowController::OnWindowDestroyed(ServerWindow* window) {
  RemoveWindow(window);
}

void ModalWindowController::OnWindowModalTypeChanged(ServerWindow* window,
                                                     ModalType old_modal_type) {
  DCHECK_EQ(MODAL_TYPE_SYSTEM, old_modal_type);
  DCHECK_NE(MODAL_TYPE_SYSTEM, window->modal_type());
  RemoveWindow(window);
}

void ModalWindowController::OnDrawnStateChanged(ServerWindow* ancestor,
                                                ServerWindow* window,
                                                bool is_drawn) {
  if (!is_drawn)
    return;

  // Move the most recently shown window to the end of the list.
  auto it = std::find(system_modal_windows_.begin(),
                      system_modal_windows_.end(), window);
  DCHECK(it != system_modal_windows_.end());
  system_modal_windows_.splice(system_modal_windows_.end(),
                               system_modal_windows_, it);
}

}  // namespace ws
}  // namespace ui

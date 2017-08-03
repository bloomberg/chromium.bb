// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/modal_window_controller.h"

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
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

// Returns true if |transient| is a valid modal window. |target_window| is
// the window being considered.
bool IsModalTransientChild(const ServerWindow* transient,
                           const ServerWindow* target_window) {
  return transient->IsDrawn() &&
         (transient->modal_type() == MODAL_TYPE_WINDOW ||
          transient->modal_type() == MODAL_TYPE_SYSTEM ||
          (transient->modal_type() == MODAL_TYPE_CHILD &&
           (transient->GetChildModalParent() &&
            transient->GetChildModalParent()->Contains(target_window))));
}

// Returns the deepest modal window starting at |activatable|. |target_window|
// is the window being considered.
const ServerWindow* GetModalTransientChild(const ServerWindow* activatable,
                                           const ServerWindow* target_window) {
  for (const ServerWindow* transient : activatable->transient_children()) {
    if (IsModalTransientChild(transient, target_window)) {
      if (transient->transient_children().empty())
        return transient;

      const ServerWindow* modal_child =
          GetModalTransientChild(transient, target_window);
      return modal_child ? modal_child : transient;
    }
  }
  return nullptr;
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

  event_dispatcher_->ReleaseCaptureBlockedByAnyModalWindow();
}

bool ModalWindowController::IsWindowBlocked(const ServerWindow* window) const {
  if (!window)
    return false;

  if (GetModalTransient(window))
    return true;

  // TODO(sky): the checks here are not enough, need to match that of
  // RootWindowController::CanWindowReceiveEvents().
  ServerWindow* system_modal_window = GetActiveSystemModalWindow();
  return system_modal_window ? !system_modal_window->Contains(window) : false;
}

const ServerWindow* ModalWindowController::GetModalTransient(
    const ServerWindow* window) const {
  if (!window)
    return nullptr;

  // We always want to check the for the transient child of the toplevel window.
  const ServerWindow* toplevel = GetToplevelWindow(window);
  if (!toplevel)
    return nullptr;

  return GetModalTransientChild(toplevel, window);
}

const ServerWindow* ModalWindowController::GetToplevelWindow(
    const ServerWindow* window) const {
  const ServerWindow* last = nullptr;
  for (const ServerWindow *w = window; w; last = w, w = w->parent()) {
    if (w->is_activation_parent())
      return last;
  }
  return nullptr;
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

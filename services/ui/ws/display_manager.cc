// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display_manager.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "services/ui/display/viewport_metrics.h"
#include "services/ui/ws/cursor_location_manager.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/event_dispatcher.h"
#include "services/ui/ws/frame_generator.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/user_display_manager_delegate.h"
#include "services/ui/ws/user_id_tracker.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_manager_window_tree_factory.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "ui/events/event_rewriter.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#endif

namespace ui {
namespace ws {

DisplayManager::DisplayManager(WindowServer* window_server,
                               UserIdTracker* user_id_tracker)
    // |next_root_id_| is used as the lower bits, so that starting at 0 is
    // fine. |next_display_id_| is used by itself, so we start at 1 to reserve
    // 0 as invalid.
    : window_server_(window_server),
      user_id_tracker_(user_id_tracker),
      next_root_id_(0) {
#if defined(OS_CHROMEOS)
  // TODO: http://crbug.com/701468 fix function key preferences and sticky keys.
  ui::EventRewriterChromeOS::Delegate* delegate = nullptr;
  ui::EventRewriter* sticky_keys_controller = nullptr;
  event_rewriter_ = base::MakeUnique<ui::EventRewriterChromeOS>(
      delegate, sticky_keys_controller);
#endif
  user_id_tracker_->AddObserver(this);
}

DisplayManager::~DisplayManager() {
  user_id_tracker_->RemoveObserver(this);
  DestroyAllDisplays();
}

UserDisplayManager* DisplayManager::GetUserDisplayManager(
    const UserId& user_id) {
  if (!user_display_managers_.count(user_id)) {
    user_display_managers_[user_id] =
        base::MakeUnique<UserDisplayManager>(window_server_, user_id);
  }
  return user_display_managers_[user_id].get();
}

CursorLocationManager* DisplayManager::GetCursorLocationManager(
    const UserId& user_id) {
  if (!cursor_location_managers_.count(user_id)) {
    cursor_location_managers_[user_id] =
        base::MakeUnique<CursorLocationManager>();
  }
  return cursor_location_managers_[user_id].get();
}

void DisplayManager::AddDisplay(Display* display) {
  DCHECK_EQ(0u, pending_displays_.count(display));
  pending_displays_.insert(display);
}

void DisplayManager::DestroyDisplay(Display* display) {
  if (pending_displays_.count(display)) {
    pending_displays_.erase(display);
  } else {
    for (const auto& pair : user_display_managers_)
      pair.second->OnWillDestroyDisplay(display->GetId());

    DCHECK(displays_.count(display));
    displays_.erase(display);
    window_server_->OnDisplayDestroyed(display);
  }
  delete display;

  // If we have no more roots left, let the app know so it can terminate.
  // TODO(sky): move to delegate/observer.
  if (displays_.empty() && pending_displays_.empty())
    window_server_->OnNoMoreDisplays();
}

void DisplayManager::DestroyAllDisplays() {
  while (!pending_displays_.empty())
    DestroyDisplay(*pending_displays_.begin());
  DCHECK(pending_displays_.empty());

  while (!displays_.empty())
    DestroyDisplay(*displays_.begin());
  DCHECK(displays_.empty());
}

std::set<const Display*> DisplayManager::displays() const {
  std::set<const Display*> ret_value(displays_.begin(), displays_.end());
  return ret_value;
}

void DisplayManager::OnDisplayUpdate(const display::Display& display) {
  for (const auto& pair : user_display_managers_)
    pair.second->OnDisplayUpdate(display);
}

Display* DisplayManager::GetDisplayContaining(const ServerWindow* window) {
  return const_cast<Display*>(
      static_cast<const DisplayManager*>(this)->GetDisplayContaining(window));
}

const Display* DisplayManager::GetDisplayContaining(
    const ServerWindow* window) const {
  while (window && window->parent())
    window = window->parent();
  for (Display* display : displays_) {
    if (window == display->root_window())
      return display;
  }
  return nullptr;
}

Display* DisplayManager::GetDisplayById(int64_t display_id) {
  for (Display* display : displays_) {
    if (display->GetId() == display_id)
      return display;
  }
  return nullptr;
}

const WindowManagerDisplayRoot* DisplayManager::GetWindowManagerDisplayRoot(
    const ServerWindow* window) const {
  const ServerWindow* last = window;
  while (window && window->parent()) {
    last = window;
    window = window->parent();
  }
  for (Display* display : displays_) {
    if (window == display->root_window())
      return display->GetWindowManagerDisplayRootWithRoot(last);
  }
  return nullptr;
}

WindowManagerDisplayRoot* DisplayManager::GetWindowManagerDisplayRoot(
    const ServerWindow* window) {
  return const_cast<WindowManagerDisplayRoot*>(
      const_cast<const DisplayManager*>(this)->GetWindowManagerDisplayRoot(
          window));
}

WindowId DisplayManager::GetAndAdvanceNextRootId() {
  // TODO(sky): handle wrapping!
  const uint16_t id = next_root_id_++;
  DCHECK_LT(id, next_root_id_);
  return RootWindowId(id);
}

void DisplayManager::OnDisplayAcceleratedWidgetAvailable(Display* display) {
  DCHECK_NE(0u, pending_displays_.count(display));
  DCHECK_EQ(0u, displays_.count(display));
  const bool is_first_display = displays_.empty();
  displays_.insert(display);
  pending_displays_.erase(display);
  if (event_rewriter_)
    display->platform_display()->AddEventRewriter(event_rewriter_.get());
  window_server_->OnDisplayReady(display, is_first_display);
}

void DisplayManager::SetHighContrastMode(bool enabled) {
  for (Display* display : displays_) {
    display->platform_display()->GetFrameGenerator()->SetHighContrastMode(
        enabled);
  }
}

void DisplayManager::OnActiveUserIdChanged(const UserId& previously_active_id,
                                           const UserId& active_id) {
  WindowManagerState* previous_window_manager_state =
      window_server_->GetWindowManagerStateForUser(previously_active_id);
  gfx::Point mouse_location_on_screen;
  if (previous_window_manager_state) {
    mouse_location_on_screen = previous_window_manager_state->event_dispatcher()
                                   ->mouse_pointer_last_location();
    previous_window_manager_state->Deactivate();
  }

  WindowManagerState* current_window_manager_state =
      window_server_->GetWindowManagerStateForUser(active_id);
  if (current_window_manager_state)
    current_window_manager_state->Activate(mouse_location_on_screen);
}

void DisplayManager::OnDisplayAdded(const display::Display& display,
                                    const display::ViewportMetrics& metrics) {
  DVLOG(3) << "OnDisplayAdded: " << display.ToString();

  ws::Display* ws_display = new ws::Display(window_server_);
  ws_display->SetDisplay(display);
  ws_display->Init(metrics, nullptr);
}

void DisplayManager::OnDisplayRemoved(int64_t display_id) {
  DVLOG(3) << "OnDisplayRemoved: " << display_id;
  Display* display = GetDisplayById(display_id);
  if (display)
    DestroyDisplay(display);
}

void DisplayManager::OnDisplayModified(
    const display::Display& display,
    const display::ViewportMetrics& metrics) {
  DVLOG(3) << "OnDisplayModified: " << display.ToString();

  Display* ws_display = GetDisplayById(display.id());
  DCHECK(ws_display);

  // Update the cached display information.
  ws_display->SetDisplay(display);

  // Send IPC to WMs with new display information.
  std::vector<WindowManagerWindowTreeFactory*> factories =
      window_server_->window_manager_window_tree_factory_set()->GetFactories();
  for (WindowManagerWindowTreeFactory* factory : factories) {
    if (factory->window_tree())
      factory->window_tree()->OnWmDisplayModified(display);
  }

  // Update the PlatformWindow and ServerWindow size. This must happen after
  // OnWmDisplayModified() so the WM has updated the display size.
  ws_display->OnViewportMetricsChanged(metrics);

  OnDisplayUpdate(display);
}

void DisplayManager::OnPrimaryDisplayChanged(int64_t primary_display_id) {
  DVLOG(3) << "OnPrimaryDisplayChanged: " << primary_display_id;
  // TODO(kylechar): Send IPCs to WM clients first.

  // Send IPCs to any DisplayManagerObservers.
  for (const auto& pair : user_display_managers_)
    pair.second->OnPrimaryDisplayChanged(primary_display_id);
}

}  // namespace ws
}  // namespace ui

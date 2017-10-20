// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display_manager.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "services/ui/display/screen_manager.h"
#include "services/ui/display/viewport_metrics.h"
#include "services/ui/ws/cursor_location_manager.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_creation_config.h"
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
#include "ui/display/display_list.h"
#include "ui/display/screen_base.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/point_conversions.h"

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
      next_root_id_(0),
      internal_display_id_(display::kInvalidDisplayId) {
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

void DisplayManager::OnDisplayCreationConfigSet() {
  if (window_server_->display_creation_config() ==
      DisplayCreationConfig::MANUAL) {
    for (const auto& pair : user_display_managers_)
      pair.second->DisableAutomaticNotification();
  } else {
    // In AUTOMATIC mode SetDisplayConfiguration() is never called.
    got_initial_config_from_window_manager_ = true;
  }
}

bool DisplayManager::SetDisplayConfiguration(
    const std::vector<display::Display>& displays,
    const std::vector<display::ViewportMetrics>& viewport_metrics,
    int64_t primary_display_id,
    int64_t internal_display_id,
    const std::vector<display::Display>& mirrors) {
  if (window_server_->display_creation_config() !=
      DisplayCreationConfig::MANUAL) {
    LOG(ERROR) << "SetDisplayConfiguration is only valid when roots manually "
                  "created";
    return false;
  }
  if (displays.size() != viewport_metrics.size()) {
    LOG(ERROR) << "SetDisplayConfiguration called with mismatch in sizes";
    return false;
  }
  size_t primary_display_index = displays.size();
  std::set<int64_t> display_ids;
  size_t internal_display_index = displays.size();
  for (size_t i = 0; i < displays.size(); ++i) {
    const display::Display& display = displays[i];
    if (display.id() == display::kInvalidDisplayId) {
      LOG(ERROR) << "SetDisplayConfiguration passed invalid display id";
      return false;
    }
    if (!display_ids.insert(display.id()).second) {
      LOG(ERROR) << "SetDisplayConfiguration passed duplicate display id";
      return false;
    }
    if (display.id() == primary_display_id)
      primary_display_index = i;
    if (display.id() == internal_display_id)
      internal_display_index = i;
    Display* ws_display = GetDisplayById(display.id());
    if (!ws_display) {
      LOG(ERROR) << "SetDisplayConfiguration passed unknown display id "
                 << display.id();
      return false;
    }
  }
  if (primary_display_index == displays.size()) {
    LOG(ERROR) << "SetDisplayConfiguration primary id not in displays";
    return false;
  }
  if (internal_display_index == displays.size() &&
      internal_display_id != display::kInvalidDisplayId) {
    LOG(ERROR) << "SetDisplayConfiguration internal display id not in displays";
    return false;
  }
  if (!mirrors.empty()) {
    NOTIMPLEMENTED() << "TODO(crbug.com/764472): Mus unified/mirroring modes.";
    return false;
  }

  // See comment in header as to why this doesn't use Display::SetInternalId().
  internal_display_id_ = internal_display_id;

  display::DisplayList& display_list =
      display::ScreenManager::GetInstance()->GetScreen()->display_list();
  display_list.AddOrUpdateDisplay(displays[primary_display_index],
                                  display::DisplayList::Type::PRIMARY);
  for (size_t i = 0; i < displays.size(); ++i) {
    Display* ws_display = GetDisplayById(displays[i].id());
    DCHECK(ws_display);
    ws_display->SetDisplay(displays[i]);
    ws_display->OnViewportMetricsChanged(viewport_metrics[i]);
    if (i != primary_display_index) {
      display_list.AddOrUpdateDisplay(displays[i],
                                      display::DisplayList::Type::NOT_PRIMARY);
    }
  }

  std::set<int64_t> existing_display_ids;
  for (const display::Display& display : display_list.displays())
    existing_display_ids.insert(display.id());
  std::set<int64_t> removed_display_ids =
      base::STLSetDifference<std::set<int64_t>>(existing_display_ids,
                                                display_ids);
  for (int64_t display_id : removed_display_ids)
    display_list.RemoveDisplay(display_id);

  for (auto& pair : user_display_managers_)
    pair.second->CallOnDisplaysChanged();

  if (!got_initial_config_from_window_manager_) {
    got_initial_config_from_window_manager_ = true;
    window_server_->delegate()->OnFirstDisplayReady();
  }

  return true;
}

UserDisplayManager* DisplayManager::GetUserDisplayManager(
    const UserId& user_id) {
  if (!user_display_managers_.count(user_id)) {
    user_display_managers_[user_id] =
        base::MakeUnique<UserDisplayManager>(window_server_, user_id);
    if (window_server_->display_creation_config() ==
        DisplayCreationConfig::MANUAL) {
      user_display_managers_[user_id]->DisableAutomaticNotification();
    }
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

Display* DisplayManager::AddDisplayForWindowManager(
    bool is_primary_display,
    const display::Display& display,
    const display::ViewportMetrics& metrics) {
  DCHECK_EQ(DisplayCreationConfig::MANUAL,
            window_server_->display_creation_config());
  const display::DisplayList::Type display_type =
      is_primary_display ? display::DisplayList::Type::PRIMARY
                         : display::DisplayList::Type::NOT_PRIMARY;
  display::ScreenManager::GetInstance()->GetScreen()->display_list().AddDisplay(
      display, display_type);
  OnDisplayAdded(display, metrics);
  return GetDisplayById(display.id());
}

void DisplayManager::DestroyDisplay(Display* display) {
  const bool is_pending = pending_displays_.count(display) > 0;
  if (is_pending) {
    pending_displays_.erase(display);
  } else {
    DCHECK(displays_.count(display));
    displays_.erase(display);
    window_server_->OnDisplayDestroyed(display);
  }
  const int64_t display_id = display->GetId();
  delete display;

  // If we have no more roots left, let the app know so it can terminate.
  // TODO(sky): move to delegate/observer.
  if (displays_.empty() && pending_displays_.empty()) {
    window_server_->OnNoMoreDisplays();
  } else {
    for (const auto& pair : user_display_managers_)
      pair.second->OnDisplayDestroyed(display_id);
  }
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

void DisplayManager::OnDisplayUpdated(const display::Display& display) {
  for (const auto& pair : user_display_managers_)
    pair.second->OnDisplayUpdated(display);
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
  const bool is_first_display =
      displays_.empty() && got_initial_config_from_window_manager_;
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

bool DisplayManager::IsInternalDisplay(const display::Display& display) const {
  return display.id() == GetInternalDisplayId();
}

int64_t DisplayManager::GetInternalDisplayId() const {
  if (window_server_->display_creation_config() ==
      DisplayCreationConfig::MANUAL) {
    return internal_display_id_;
  }
  return display::Display::HasInternalDisplay()
             ? display::Display::InternalDisplayId()
             : display::kInvalidDisplayId;
}

void DisplayManager::OnActiveUserIdChanged(const UserId& previously_active_id,
                                           const UserId& active_id) {
  WindowManagerState* previous_window_manager_state =
      window_server_->GetWindowManagerStateForUser(previously_active_id);
  gfx::Point mouse_location_on_display;
  int64_t mouse_display_id = 0;
  if (previous_window_manager_state) {
    mouse_location_on_display =
        gfx::ToFlooredPoint(previous_window_manager_state->event_dispatcher()
                                ->mouse_pointer_last_location());
    mouse_display_id = previous_window_manager_state->event_dispatcher()
                           ->mouse_pointer_display_id();
    previous_window_manager_state->Deactivate();
  }

  WindowManagerState* current_window_manager_state =
      window_server_->GetWindowManagerStateForUser(active_id);
  if (current_window_manager_state)
    current_window_manager_state->Activate(mouse_location_on_display,
                                           mouse_display_id);
}

void DisplayManager::CreateDisplay(const display::Display& display,
                                   const display::ViewportMetrics& metrics) {
  ws::Display* ws_display = new ws::Display(window_server_);
  ws_display->SetDisplay(display);
  ws_display->Init(metrics, nullptr);
}

void DisplayManager::OnDisplayAdded(const display::Display& display,
                                    const display::ViewportMetrics& metrics) {
  DVLOG(3) << "OnDisplayAdded: " << display.ToString();
  CreateDisplay(display, metrics);
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

  OnDisplayUpdated(display);
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

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/drag_drop_controller_mus.h"

#include <map>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/drag_drop_controller_host.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/drop_target_event.h"

// Interaction with DragDropDelegate assumes constants are the same.
static_assert(ui::DragDropTypes::DRAG_NONE == ws::mojom::kDropEffectNone,
              "Drag constants must be the same");
static_assert(ui::DragDropTypes::DRAG_MOVE == ws::mojom::kDropEffectMove,
              "Drag constants must be the same");
static_assert(ui::DragDropTypes::DRAG_COPY == ws::mojom::kDropEffectCopy,
              "Drag constants must be the same");
static_assert(ui::DragDropTypes::DRAG_LINK == ws::mojom::kDropEffectLink,
              "Drag constants must be the same");

namespace aura {

// State related to a drag initiated by this client.
struct DragDropControllerMus::CurrentDragState {
  ws::Id window_id;

  // The change id of the drag. Used to identify the drag on the server.
  uint32_t change_id;

  // The result of the drag. This is set on completion and returned to the
  // caller.
  uint32_t completed_action;

  // OSExchangeData supplied to StartDragAndDrop().
  const ui::OSExchangeData& drag_data;

  // StartDragDrop() runs a nested run loop. This closure is used to quit
  // the run loop when the drag completes.
  base::Closure runloop_quit_closure;

  // The starting location of the drag gesture in screen coordinates.
  gfx::Point start_screen_location;

  // A tracker for the window that received the initial drag and drop events,
  // used to send ET_GESTURE_LONG_TAP when the user presses, pauses, and
  // releases a touch without any movement; it is reset if movement is detected.
  WindowTracker source_window_tracker;
};

DragDropControllerMus::DragDropControllerMus(
    DragDropControllerHost* drag_drop_controller_host,
    ws::mojom::WindowTree* window_tree)
    : drag_drop_controller_host_(drag_drop_controller_host),
      window_tree_(window_tree) {}

DragDropControllerMus::~DragDropControllerMus() {}

bool DragDropControllerMus::DoesChangeIdMatchDragChangeId(uint32_t id) const {
  return current_drag_state_ && current_drag_state_->change_id == id;
}

void DragDropControllerMus::OnDragDropStart(
    std::map<std::string, std::vector<uint8_t>> data) {
  os_exchange_data_ = std::make_unique<ui::OSExchangeData>();
}

uint32_t DragDropControllerMus::OnDragEnter(WindowMus* window,
                                            uint32_t event_flags,
                                            const gfx::PointF& location_in_root,
                                            const gfx::PointF& location,
                                            uint32_t effect_bitmask) {
  return HandleDragEnterOrOver(window, event_flags, location_in_root, location,
                               effect_bitmask, true);
}

uint32_t DragDropControllerMus::OnDragOver(WindowMus* window,
                                           uint32_t event_flags,
                                           const gfx::PointF& location_in_root,
                                           const gfx::PointF& location,
                                           uint32_t effect_bitmask) {
  return HandleDragEnterOrOver(window, event_flags, location_in_root, location,
                               effect_bitmask, false);
}

void DragDropControllerMus::OnDragLeave(WindowMus* window) {
  if (drop_target_window_tracker_.windows().empty())
    return;
  DCHECK(window);
  Window* current_target = drop_target_window_tracker_.Pop();
  DCHECK_EQ(window->GetWindow(), current_target);
  client::GetDragDropDelegate(current_target)->OnDragExited();
}

uint32_t DragDropControllerMus::OnCompleteDrop(
    WindowMus* window,
    uint32_t event_flags,
    const gfx::PointF& location_in_root,
    const gfx::PointF& location,
    uint32_t effect_bitmask) {
  if (drop_target_window_tracker_.windows().empty())
    return ws::mojom::kDropEffectNone;

  DCHECK(window);
  Window* current_target = drop_target_window_tracker_.Pop();
  DCHECK_EQ(window->GetWindow(), current_target);
  std::unique_ptr<ui::DropTargetEvent> event =
      CreateDropTargetEvent(window->GetWindow(), event_flags, location_in_root,
                            location, effect_bitmask);
  return client::GetDragDropDelegate(current_target)->OnPerformDrop(*event);
}

void DragDropControllerMus::OnPerformDragDropCompleted(uint32_t action_taken) {
  DCHECK(current_drag_state_);
  for (client::DragDropClientObserver& observer : observers_)
    observer.OnDragEnded();

  // When the user presses, pauses, and releases a touch without any movement,
  // that gesture should be interpreted as a long tap and show a menu, etc.
  // See Classic Ash's long tap event handling in ash::DragDropController.
  if (action_taken == ws::mojom::kDropEffectNone &&
      !current_drag_state_->source_window_tracker.windows().empty()) {
    auto* window = current_drag_state_->source_window_tracker.windows()[0];
    gfx::Point location = current_drag_state_->start_screen_location;
    if (auto* client = client::GetScreenPositionClient(window->GetRootWindow()))
      client->ConvertPointFromScreen(window, &location);
    ui::GestureEventDetails details(ui::ET_GESTURE_LONG_TAP);
    details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent long_tap(location.x(), location.y(), ui::EF_NONE,
                              base::TimeTicks::Now(), details);
    window->delegate()->OnEvent(&long_tap);
  }

  current_drag_state_->completed_action = action_taken;
  current_drag_state_->runloop_quit_closure.Run();
  current_drag_state_ = nullptr;
}

void DragDropControllerMus::OnDragDropDone() {
  os_exchange_data_.reset();
}

int DragDropControllerMus::StartDragAndDrop(
    const ui::OSExchangeData& data,
    Window* root_window,
    Window* source_window,
    const gfx::Point& screen_location,
    int drag_operations,
    ui::DragDropTypes::DragEventSource source) {
  DCHECK(!current_drag_state_);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  WindowMus* source_window_mus = WindowMus::Get(source_window);
  const uint32_t change_id =
      drag_drop_controller_host_->CreateChangeIdForDrag(source_window_mus);
  CurrentDragState current_drag_state = {
      source_window_mus->server_id(), change_id,
      ws::mojom::kDropEffectNone,     data,
      run_loop.QuitClosure(),         screen_location};

  // current_drag_state_ will be reset in |OnPerformDragDropCompleted| before
  // run_loop.Run() quits.
  current_drag_state_ = &current_drag_state;

  ui::mojom::PointerKind mojo_source = ui::mojom::PointerKind::MOUSE;
  if (source != ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE) {
    // TODO(erg): This collapses both touch and pen events to touch.
    mojo_source = ui::mojom::PointerKind::TOUCH;
    // Track the source window, to possibly send ET_GESTURE_LONG_TAP later.
    current_drag_state.source_window_tracker.Add(source_window);
  }

  std::map<std::string, std::vector<uint8_t>> drag_data;

  for (client::DragDropClientObserver& observer : observers_)
    observer.OnDragStarted();

  window_tree_->PerformDragDrop(change_id, source_window_mus->server_id(),
                                screen_location, mojo::MapToFlatMap(drag_data),
                                gfx::ImageSkia(), gfx::Vector2d(),
                                drag_operations, mojo_source);

  run_loop.Run();
  return current_drag_state.completed_action;
}

void DragDropControllerMus::DragCancel() {
  DCHECK(current_drag_state_);
  // Server will clean up drag and fail the in-flight change.
  window_tree_->CancelDragDrop(current_drag_state_->window_id);
}

bool DragDropControllerMus::IsDragDropInProgress() {
  return current_drag_state_ != nullptr;
}

void DragDropControllerMus::AddObserver(
    client::DragDropClientObserver* observer) {
  observers_.AddObserver(observer);
}

void DragDropControllerMus::RemoveObserver(
    client::DragDropClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

uint32_t DragDropControllerMus::HandleDragEnterOrOver(
    WindowMus* window,
    uint32_t event_flags,
    const gfx::PointF& location_in_root,
    const gfx::PointF& location,
    uint32_t effect_bitmask,
    bool is_enter) {
  if (current_drag_state_) {
    // Reset the tracker on drag movement, ET_GESTURE_LONG_TAP will not be
    // needed.
    current_drag_state_->source_window_tracker.RemoveAll();
  }

  client::DragDropDelegate* drag_drop_delegate =
      window ? client::GetDragDropDelegate(window->GetWindow()) : nullptr;
  WindowTreeHost* window_tree_host =
      window ? window->GetWindow()->GetHost() : nullptr;
  if ((!is_enter && drop_target_window_tracker_.windows().empty()) ||
      !drag_drop_delegate || !window_tree_host) {
    drop_target_window_tracker_.RemoveAll();
    return ws::mojom::kDropEffectNone;
  }
  drop_target_window_tracker_.Add(window->GetWindow());

  std::unique_ptr<ui::DropTargetEvent> event =
      CreateDropTargetEvent(window->GetWindow(), event_flags, location_in_root,
                            location, effect_bitmask);
  if (is_enter)
    drag_drop_delegate->OnDragEntered(*event);
  return drag_drop_delegate->OnDragUpdated(*event);
}

std::unique_ptr<ui::DropTargetEvent>
DragDropControllerMus::CreateDropTargetEvent(
    Window* window,
    uint32_t event_flags,
    const gfx::PointF& location_in_root,
    const gfx::PointF& location,
    uint32_t effect_bitmask) {
  std::unique_ptr<ui::DropTargetEvent> event =
      std::make_unique<ui::DropTargetEvent>(
          current_drag_state_ ? current_drag_state_->drag_data
                              : *(os_exchange_data_.get()),
          location, location_in_root, effect_bitmask);
  event->set_flags(event_flags);
  ui::Event::DispatcherApi(event.get()).set_target(window);
  return event;
}

}  // namespace aura

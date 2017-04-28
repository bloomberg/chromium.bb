// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/event_dispatcher.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "services/ui/ws/accelerator.h"
#include "services/ui/ws/drag_controller.h"
#include "services/ui/ws/drag_source.h"
#include "services/ui/ws/event_dispatcher_delegate.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/window_coordinate_conversions.h"
#include "services/ui/ws/window_finder.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace ui {
namespace ws {

using Entry = std::pair<uint32_t, std::unique_ptr<Accelerator>>;

namespace {

bool IsOnlyOneMouseButtonDown(int flags) {
  const uint32_t button_only_flags =
      flags & (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
               ui::EF_RIGHT_MOUSE_BUTTON);
  return button_only_flags == ui::EF_LEFT_MOUSE_BUTTON ||
         button_only_flags == ui::EF_MIDDLE_MOUSE_BUTTON ||
         button_only_flags == ui::EF_RIGHT_MOUSE_BUTTON;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

EventDispatcher::EventDispatcher(EventDispatcherDelegate* delegate)
    : delegate_(delegate),
      capture_window_(nullptr),
      capture_window_client_id_(kInvalidClientId),
      modal_window_controller_(this),
      mouse_button_down_(false),
      mouse_cursor_source_window_(nullptr),
      mouse_cursor_in_non_client_area_(false) {}

EventDispatcher::~EventDispatcher() {
  SetMouseCursorSourceWindow(nullptr);
  if (capture_window_) {
    UnobserveWindow(capture_window_);
    capture_window_ = nullptr;
    capture_window_client_id_ = kInvalidClientId;
  }
  for (const auto& pair : pointer_targets_) {
    if (pair.second.window)
      UnobserveWindow(pair.second.window);
  }
  pointer_targets_.clear();
}

void EventDispatcher::Reset() {
  if (capture_window_) {
    CancelPointerEventsToTarget(capture_window_);
    DCHECK(capture_window_ == nullptr);
  }

  while (!pointer_targets_.empty())
    StopTrackingPointer(pointer_targets_.begin()->first);

  mouse_button_down_ = false;
}

void EventDispatcher::SetMousePointerScreenLocation(
    const gfx::Point& screen_location) {
  DCHECK(pointer_targets_.empty());
  mouse_pointer_last_location_ = screen_location;
  UpdateCursorProviderByLastKnownLocation();
  // Write our initial location back to our shared screen coordinate. This
  // shouldn't cause problems because we already read the cursor before we
  // process any events in views during window construction.
  delegate_->OnMouseCursorLocationChanged(screen_location);
}

ui::CursorData EventDispatcher::GetCurrentMouseCursor() const {
  if (drag_controller_)
    return drag_controller_->current_cursor();

  if (!mouse_cursor_source_window_)
    return ui::CursorData(ui::CursorType::kPointer);

  if (mouse_cursor_in_non_client_area_)
    return mouse_cursor_source_window_->non_client_cursor();

  const ServerWindow* window = GetWindowForMouseCursor();
  return window ? window->cursor() : ui::CursorData(ui::CursorType::kPointer);
}

bool EventDispatcher::SetCaptureWindow(ServerWindow* window,
                                       ClientSpecificId client_id) {
  if (!window)
    client_id = kInvalidClientId;

  if (window == capture_window_ && client_id == capture_window_client_id_)
    return true;

  // A window that is blocked by a modal window cannot gain capture.
  if (window && modal_window_controller_.IsWindowBlocked(window))
    return false;

  // If we're currently performing a drag and drop, reject setting the capture
  // window.
  if (drag_controller_)
    return false;

  if (capture_window_) {
    // Stop observing old capture window. |pointer_targets_| are cleared on
    // initial setting of a capture window.
    UnobserveWindow(capture_window_);
  } else {
    CancelImplicitCaptureExcept(window, client_id);
  }

  // Set the capture before changing native capture; otherwise, the callback
  // from native platform might try to set the capture again.
  const bool had_capture_window = capture_window_ != nullptr;
  ServerWindow* old_capture_window = capture_window_;
  capture_window_ = window;
  capture_window_client_id_ = client_id;

  delegate_->OnCaptureChanged(capture_window_, old_capture_window);

  // Begin tracking the capture window if it is not yet being observed.
  if (window) {
    ObserveWindow(window);
    // TODO(sky): this conditional is problematic for the case of capture moving
    // to a different display.
    if (!had_capture_window)
      delegate_->SetNativeCapture(window);
  } else {
    delegate_->ReleaseNativeCapture();
    if (!mouse_button_down_)
      UpdateCursorProviderByLastKnownLocation();
  }
  return true;
}

void EventDispatcher::SetDragDropSourceWindow(
    DragSource* drag_source,
    ServerWindow* window,
    DragTargetConnection* source_connection,
    int32_t drag_pointer,
    const std::unordered_map<std::string, std::vector<uint8_t>>& mime_data,
    uint32_t drag_operations) {
  CancelImplicitCaptureExcept(nullptr, kInvalidClientId);
  drag_controller_ = base::MakeUnique<DragController>(
      this, drag_source, window, source_connection, drag_pointer, mime_data,
      drag_operations);
}

void EventDispatcher::CancelDragDrop() {
  if (drag_controller_)
    drag_controller_->Cancel();
}

void EventDispatcher::EndDragDrop() {
  drag_controller_.reset();
}

void EventDispatcher::OnWillDestroyDragTargetConnection(
    DragTargetConnection* connection) {
  if (drag_controller_)
    drag_controller_->OnWillDestroyDragTargetConnection(connection);
}

void EventDispatcher::AddSystemModalWindow(ServerWindow* window) {
  modal_window_controller_.AddSystemModalWindow(window);
}

void EventDispatcher::ReleaseCaptureBlockedByModalWindow(
    const ServerWindow* modal_window) {
  if (!capture_window_)
    return;

  if (modal_window_controller_.IsWindowBlockedBy(capture_window_,
                                                 modal_window)) {
    SetCaptureWindow(nullptr, kInvalidClientId);
  }
}

void EventDispatcher::ReleaseCaptureBlockedByAnyModalWindow() {
  if (!capture_window_)
    return;

  if (modal_window_controller_.IsWindowBlocked(capture_window_))
    SetCaptureWindow(nullptr, kInvalidClientId);
}

const ServerWindow* EventDispatcher::GetWindowForMouseCursor() const {
  if (mouse_cursor_in_non_client_area_ || !mouse_cursor_source_window_)
    return mouse_cursor_source_window_;

  // Return the ancestor (starting at |mouse_cursor_source_window_|) whose
  // client id differs. In other words, return the first window ancestor that is
  // an embed root. This is done to match the behavior of aura, which sets the
  // cursor on the root.
  const ClientSpecificId target_client_id = delegate_->GetEventTargetClientId(
      mouse_cursor_source_window_, mouse_cursor_in_non_client_area_);
  const ServerWindow* window = mouse_cursor_source_window_;
  while (window && window->id().client_id == target_client_id)
    window = window->parent();
  return window;
}

void EventDispatcher::UpdateNonClientAreaForCurrentWindow() {
  if (mouse_cursor_source_window_) {
    DeepestWindow deepest_window =
        FindDeepestVisibleWindowForEvents(mouse_pointer_last_location_);
    if (deepest_window.window == mouse_cursor_source_window_) {
      mouse_cursor_in_non_client_area_ = mouse_cursor_source_window_
                                             ? deepest_window.in_non_client_area
                                             : false;
    }
  }
}

void EventDispatcher::UpdateCursorProviderByLastKnownLocation() {
  if (!mouse_button_down_) {
    DeepestWindow deepest_window =
        FindDeepestVisibleWindowForEvents(mouse_pointer_last_location_);
    SetMouseCursorSourceWindow(deepest_window.window);
    if (mouse_cursor_source_window_) {
      mouse_cursor_in_non_client_area_ = deepest_window.in_non_client_area;
    } else {
      gfx::Point location = mouse_pointer_last_location_;
      SetMouseCursorSourceWindow(delegate_->GetRootWindowContaining(&location));
      mouse_cursor_in_non_client_area_ = true;
    }
  }
}

bool EventDispatcher::AddAccelerator(uint32_t id,
                                     mojom::EventMatcherPtr event_matcher) {
  std::unique_ptr<Accelerator> accelerator(new Accelerator(id, *event_matcher));
  // If an accelerator with the same id or matcher already exists, then abort.
  for (const auto& pair : accelerators_) {
    if (pair.first == id) {
      DVLOG(1) << "duplicate accelerator. Accelerator id=" << accelerator->id()
               << " type=" << event_matcher->type_matcher->type
               << " flags=" << event_matcher->flags_matcher->flags;
      return false;
    } else if (accelerator->EqualEventMatcher(pair.second.get())) {
      DVLOG(1) << "duplicate matcher. Accelerator id=" << accelerator->id()
               << " type=" << event_matcher->type_matcher->type
               << " flags=" << event_matcher->flags_matcher->flags;
      return false;
    }
  }
  accelerators_.insert(Entry(id, std::move(accelerator)));
  return true;
}

void EventDispatcher::RemoveAccelerator(uint32_t id) {
  auto it = accelerators_.find(id);
  // Clients may pass bogus ids.
  if (it != accelerators_.end())
    accelerators_.erase(it);
}

void EventDispatcher::ProcessEvent(const ui::Event& event,
                                   AcceleratorMatchPhase match_phase) {
#if !defined(NDEBUG)
  if (match_phase == AcceleratorMatchPhase::POST_ONLY) {
    // POST_ONLY should always be preceeded by ANY with the same event.
    DCHECK(previous_event_);
    // Event doesn't define ==, so this compares the key fields.
    DCHECK(event.type() == previous_event_->type() &&
           event.time_stamp() == previous_event_->time_stamp() &&
           event.flags() == previous_event_->flags());
    DCHECK_EQ(previous_accelerator_match_phase_, AcceleratorMatchPhase::ANY);
  }
  previous_event_ = Event::Clone(event);
  previous_accelerator_match_phase_ = match_phase;
#endif
  if (event.IsKeyEvent()) {
    const ui::KeyEvent* key_event = event.AsKeyEvent();
    if (!key_event->is_char() && match_phase == AcceleratorMatchPhase::ANY) {
      Accelerator* pre_target =
          FindAccelerator(*key_event, ui::mojom::AcceleratorPhase::PRE_TARGET);
      if (pre_target) {
        delegate_->OnAccelerator(
            pre_target->id(), event,
            EventDispatcherDelegate::AcceleratorPhase::PRE);
        return;
      }
    }
    ProcessKeyEvent(*key_event, match_phase);
    return;
  }

  DCHECK(event.IsPointerEvent());
  ProcessPointerEvent(*event.AsPointerEvent());
  return;
}

void EventDispatcher::SetMouseCursorSourceWindow(ServerWindow* window) {
  if (mouse_cursor_source_window_ == window)
    return;

  if (mouse_cursor_source_window_)
    UnobserveWindow(mouse_cursor_source_window_);
  mouse_cursor_source_window_ = window;
  if (mouse_cursor_source_window_)
    ObserveWindow(mouse_cursor_source_window_);
}

void EventDispatcher::ProcessKeyEvent(const ui::KeyEvent& event,
                                      AcceleratorMatchPhase match_phase) {
  Accelerator* post_target =
      FindAccelerator(event, ui::mojom::AcceleratorPhase::POST_TARGET);
  if (drag_controller_ && event.type() == ui::ET_KEY_PRESSED &&
      event.key_code() == ui::VKEY_ESCAPE) {
    drag_controller_->Cancel();
    return;
  }
  ServerWindow* focused_window =
      delegate_->GetFocusedWindowForEventDispatcher();
  if (focused_window) {
    // Assume key events are for the client area.
    const bool in_nonclient_area = false;
    const ClientSpecificId client_id =
        delegate_->GetEventTargetClientId(focused_window, in_nonclient_area);
    delegate_->DispatchInputEventToWindow(focused_window, client_id, event,
                                          post_target);
    return;
  }
  delegate_->OnEventTargetNotFound(event);
  if (post_target)
    delegate_->OnAccelerator(post_target->id(), event,
                             EventDispatcherDelegate::AcceleratorPhase::POST);
}

void EventDispatcher::ProcessPointerEvent(const ui::PointerEvent& event) {
  DCHECK(event.IsPointerEvent());
  const bool is_mouse_event = event.IsMousePointerEvent();

  if (is_mouse_event) {
    mouse_pointer_last_location_ = event.root_location();
    delegate_->OnMouseCursorLocationChanged(event.root_location());
  }

  // Release capture on pointer up. For mouse we only release if there are
  // no buttons down.
  const bool is_pointer_going_up =
      (event.type() == ui::ET_POINTER_UP ||
       event.type() == ui::ET_POINTER_CANCELLED) &&
      (!is_mouse_event || IsOnlyOneMouseButtonDown(event.flags()));

  // Update mouse down state upon events which change it.
  if (is_mouse_event) {
    if (event.type() == ui::ET_POINTER_DOWN)
      mouse_button_down_ = true;
    else if (is_pointer_going_up)
      mouse_button_down_ = false;
  }

  if (drag_controller_) {
    const PointerTarget target = PointerTargetForEvent(event);
    if (drag_controller_->DispatchPointerEvent(event, target.window))
      return;
  }

  if (capture_window_) {
    SetMouseCursorSourceWindow(capture_window_);
    DispatchToClient(capture_window_, capture_window_client_id_, event);
    return;
  }

  const int32_t pointer_id = event.pointer_details().id;
  if (!IsTrackingPointer(pointer_id) ||
      !pointer_targets_[pointer_id].is_pointer_down) {
    const bool any_pointers_down = AreAnyPointersDown();
    UpdateTargetForPointer(pointer_id, event);
    if (is_mouse_event)
      SetMouseCursorSourceWindow(pointer_targets_[pointer_id].window);

    PointerTarget& pointer_target = pointer_targets_[pointer_id];
    if (pointer_target.is_pointer_down) {
      if (is_mouse_event)
        SetMouseCursorSourceWindow(pointer_target.window);
      if (!any_pointers_down) {
        if (pointer_target.window)
          delegate_->SetFocusedWindowFromEventDispatcher(pointer_target.window);
        ServerWindow* capture_window = pointer_target.window;
        if (!capture_window) {
          gfx::Point event_location = event.root_location();
          capture_window = delegate_->GetRootWindowContaining(&event_location);
        }
        delegate_->SetNativeCapture(capture_window);
      }
    }
  }

  // When we release the mouse button, we want the cursor to be sourced from
  // the window under the mouse pointer, even though we're sending the button
  // up event to the window that had implicit capture. We have to set this
  // before we perform dispatch because the Delegate is going to read this
  // information from us.
  if (is_pointer_going_up && is_mouse_event)
    UpdateCursorProviderByLastKnownLocation();

  DispatchToPointerTarget(pointer_targets_[pointer_id], event);

  if (is_pointer_going_up) {
    if (is_mouse_event)
      pointer_targets_[pointer_id].is_pointer_down = false;
    else
      StopTrackingPointer(pointer_id);
    if (!AreAnyPointersDown())
      delegate_->ReleaseNativeCapture();
  }
}

void EventDispatcher::StartTrackingPointer(
    int32_t pointer_id,
    const PointerTarget& pointer_target) {
  DCHECK(!IsTrackingPointer(pointer_id));
  if (pointer_target.window)
    ObserveWindow(pointer_target.window);
  pointer_targets_[pointer_id] = pointer_target;
}

void EventDispatcher::StopTrackingPointer(int32_t pointer_id) {
  DCHECK(IsTrackingPointer(pointer_id));
  ServerWindow* window = pointer_targets_[pointer_id].window;
  pointer_targets_.erase(pointer_id);
  if (window)
    UnobserveWindow(window);
}

void EventDispatcher::UpdateTargetForPointer(int32_t pointer_id,
                                             const ui::LocatedEvent& event) {
  if (!IsTrackingPointer(pointer_id)) {
    StartTrackingPointer(pointer_id, PointerTargetForEvent(event));
    return;
  }

  const PointerTarget pointer_target = PointerTargetForEvent(event);
  if (pointer_target.window == pointer_targets_[pointer_id].window &&
      pointer_target.in_nonclient_area ==
          pointer_targets_[pointer_id].in_nonclient_area) {
    // The targets are the same, only set the down state to true if necessary.
    // Down going to up is handled by ProcessLocatedEvent().
    if (pointer_target.is_pointer_down)
      pointer_targets_[pointer_id].is_pointer_down = true;
    return;
  }

  // The targets are changing. Send an exit if appropriate.
  if (event.IsMousePointerEvent()) {
    ui::PointerEvent exit_event(
        ui::ET_POINTER_EXITED, event.location(), event.root_location(),
        event.flags(), 0 /* changed_button_flags */,
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE,
                           ui::MouseEvent::kMousePointerId),
        event.time_stamp());
    DispatchToPointerTarget(pointer_targets_[pointer_id], exit_event);
  }

  // Technically we're updating in place, but calling start then stop makes for
  // simpler code.
  StopTrackingPointer(pointer_id);
  StartTrackingPointer(pointer_id, pointer_target);
}

EventDispatcher::PointerTarget EventDispatcher::PointerTargetForEvent(
    const ui::LocatedEvent& event) {
  PointerTarget pointer_target;
  DeepestWindow deepest_window =
      FindDeepestVisibleWindowForEvents(event.root_location());
  pointer_target.window =
      modal_window_controller_.GetTargetForWindow(deepest_window.window);
  pointer_target.is_mouse_event = event.IsMousePointerEvent();
  pointer_target.in_nonclient_area =
      deepest_window.window != pointer_target.window ||
      !pointer_target.window || deepest_window.in_non_client_area;
  pointer_target.is_pointer_down = event.type() == ui::ET_POINTER_DOWN;
  return pointer_target;
}

bool EventDispatcher::AreAnyPointersDown() const {
  for (const auto& pair : pointer_targets_) {
    if (pair.second.is_pointer_down)
      return true;
  }
  return false;
}

void EventDispatcher::DispatchToPointerTarget(const PointerTarget& target,
                                              const ui::LocatedEvent& event) {
  if (!target.window) {
    delegate_->OnEventTargetNotFound(event);
    return;
  }

  if (target.is_mouse_event)
    mouse_cursor_in_non_client_area_ = target.in_nonclient_area;

  DispatchToClient(target.window, delegate_->GetEventTargetClientId(
                                      target.window, target.in_nonclient_area),
                   event);
}

void EventDispatcher::DispatchToClient(ServerWindow* window,
                                       ClientSpecificId client_id,
                                       const ui::LocatedEvent& event) {
  gfx::Point location(event.location());
  gfx::Transform transform(GetTransformToWindow(window));
  transform.TransformPoint(&location);
  std::unique_ptr<ui::Event> clone = ui::Event::Clone(event);
  clone->AsLocatedEvent()->set_location(location);
  // TODO(jonross): add post-target accelerator support once accelerators
  // support pointer events.
  delegate_->DispatchInputEventToWindow(window, client_id, *clone, nullptr);
}

void EventDispatcher::CancelPointerEventsToTarget(ServerWindow* window) {
  if (capture_window_ == window) {
    UnobserveWindow(window);
    capture_window_ = nullptr;
    capture_window_client_id_ = kInvalidClientId;
    mouse_button_down_ = false;
    // A window only cares to be informed that it lost capture if it explicitly
    // requested capture. A window can lose capture if another window gains
    // explicit capture.
    delegate_->OnCaptureChanged(nullptr, window);
    delegate_->ReleaseNativeCapture();
    UpdateCursorProviderByLastKnownLocation();
    return;
  }

  for (auto& pair : pointer_targets_) {
    if (pair.second.window == window) {
      UnobserveWindow(window);
      pair.second.window = nullptr;
    }
  }
}

void EventDispatcher::ObserveWindow(ServerWindow* window) {
  auto res = observed_windows_.insert(std::make_pair(window, 0u));
  res.first->second++;
  if (res.second)
    window->AddObserver(this);
}

void EventDispatcher::UnobserveWindow(ServerWindow* window) {
  auto it = observed_windows_.find(window);
  DCHECK(it != observed_windows_.end());
  DCHECK_LT(0u, it->second);
  it->second--;
  if (!it->second) {
    window->RemoveObserver(this);
    observed_windows_.erase(it);
  }
}

Accelerator* EventDispatcher::FindAccelerator(
    const ui::KeyEvent& event,
    const ui::mojom::AcceleratorPhase phase) {
  for (const auto& pair : accelerators_) {
    if (pair.second->MatchesEvent(event, phase))
      return pair.second.get();
  }
  return nullptr;
}

DeepestWindow EventDispatcher::FindDeepestVisibleWindowForEvents(
    const gfx::Point& location) {
  gfx::Point relative_location(location);
  // For the case of no root.
  ServerWindow* root = delegate_->GetRootWindowContaining(&relative_location);
  return root ? ui::ws::FindDeepestVisibleWindowForEvents(root,
                                                          relative_location)
              : DeepestWindow();
}

void EventDispatcher::CancelImplicitCaptureExcept(ServerWindow* window,
                                                  ClientSpecificId client_id) {
  for (const auto& pair : pointer_targets_) {
    ServerWindow* target = pair.second.window;
    if (!target)
      continue;
    UnobserveWindow(target);
    if (target == window)
      continue;

    // Don't send cancel events to the same client requesting capture,
    // otherwise the client can easily get confused.
    if (window &&
        client_id ==
            delegate_->GetEventTargetClientId(target,
                                              pair.second.in_nonclient_area)) {
      continue;
    }

    ui::EventType event_type = pair.second.is_mouse_event
                                   ? ui::ET_POINTER_EXITED
                                   : ui::ET_POINTER_CANCELLED;
    ui::EventPointerType pointer_type =
        pair.second.is_mouse_event ? ui::EventPointerType::POINTER_TYPE_MOUSE
                                   : ui::EventPointerType::POINTER_TYPE_TOUCH;
    // TODO(jonross): Track previous location in PointerTarget for sending
    // cancels.
    ui::PointerEvent event(event_type, gfx::Point(), gfx::Point(), ui::EF_NONE,
                           0 /* changed_button_flags */,
                           ui::PointerDetails(pointer_type, pair.first),
                           ui::EventTimeForNow());
    DispatchToPointerTarget(pair.second, event);
  }
  pointer_targets_.clear();
}

void EventDispatcher::OnWillChangeWindowHierarchy(ServerWindow* window,
                                                  ServerWindow* new_parent,
                                                  ServerWindow* old_parent) {
  // TODO(sky): moving to a different root likely needs to transfer capture.
  // TODO(sky): this isn't quite right, I think the logic should be (assuming
  // moving in same root and still drawn):
  // . if there is capture and window is still in the same root, continue
  //   sending to it.
  // . if there isn't capture, then reevaluate each of the pointer targets
  //   sending exit as necessary.
  // http://crbug.com/613646 .
  if (!new_parent || !new_parent->IsDrawn() ||
      new_parent->GetRoot() != old_parent->GetRoot()) {
    CancelPointerEventsToTarget(window);
  }
}

void EventDispatcher::OnWindowVisibilityChanged(ServerWindow* window) {
  CancelPointerEventsToTarget(window);
}

void EventDispatcher::OnWindowDestroyed(ServerWindow* window) {
  CancelPointerEventsToTarget(window);

  if (mouse_cursor_source_window_ == window)
    SetMouseCursorSourceWindow(nullptr);
}

void EventDispatcher::OnDragCursorUpdated() {
  delegate_->UpdateNativeCursorFromDispatcher();
}

}  // namespace ws
}  // namespace ui

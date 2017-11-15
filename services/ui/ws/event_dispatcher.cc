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
#include "services/ui/ws/event_location.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_drawn_tracker.h"
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

// This is meant to mirror when implicit capture stops. Specifically non-mouse
// pointer up, or mouse and no more buttons down.
bool IsPointerGoingUp(const PointerEvent& event) {
  return (event.type() == ui::ET_POINTER_UP ||
          event.type() == ui::ET_POINTER_CANCELLED) &&
         (!event.IsMousePointerEvent() ||
          IsOnlyOneMouseButtonDown(event.flags()));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

EventDispatcher::ObservedWindow::ObservedWindow() = default;
EventDispatcher::ObservedWindow::~ObservedWindow() = default;

EventDispatcher::EventDispatcher(EventDispatcherDelegate* delegate)
    : delegate_(delegate),
      capture_window_(nullptr),
      capture_window_client_id_(kInvalidClientId),
      event_targeter_(std::make_unique<EventTargeter>(this)),
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
  drag_controller_ = std::make_unique<DragController>(
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
  ReleaseCaptureBlockedByAnyModalWindow();
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
  if (!mouse_button_down_ && mouse_cursor_source_window_) {
    event_targeter_->FindTargetForLocation(
        EventSource::MOUSE,
        {mouse_pointer_last_location_, mouse_pointer_last_location_,
         mouse_pointer_display_id_},
        base::BindOnce(
            &EventDispatcher::UpdateNonClientAreaForCurrentWindowOnFoundWindow,
            base::Unretained(this)));
  }
}

void EventDispatcher::UpdateCursorProviderByLastKnownLocation() {
  if (!mouse_button_down_) {
    event_targeter_->FindTargetForLocation(
        EventSource::MOUSE,
        {mouse_pointer_last_location_, mouse_pointer_last_location_,
         mouse_pointer_display_id_},
        base::BindOnce(&EventDispatcher::
                           UpdateCursorProviderByLastKnownLocationOnFoundWindow,
                       base::Unretained(this)));
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

void EventDispatcher::SetKeyEventsThatDontHideCursor(
    std::vector<::ui::mojom::EventMatcherPtr> dont_hide_cursor_list) {
  dont_hide_cursor_matchers_.clear();
  for (auto& matcher_ptr : dont_hide_cursor_list) {
    EventMatcher matcher(*matcher_ptr);
    // Ensure we don't have pointer matchers in our key only list.
    DCHECK(!matcher.HasFields(EventMatcher::POINTER_KIND |
                              EventMatcher::POINTER_LOCATION));
    dont_hide_cursor_matchers_.push_back(std::move(matcher));
  }
}

bool EventDispatcher::IsProcessingEvent() const {
  return waiting_on_event_targeter_;
}

void EventDispatcher::ProcessEvent(const ui::Event& event,
                                   const EventLocation& event_location,
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
            pre_target->id(), event_location.display_id, event,
            EventDispatcherDelegate::AcceleratorPhase::PRE);
        return;
      }
    }
    ProcessKeyEvent(*key_event, event_location.display_id, match_phase);
    return;
  }

  DCHECK(event.IsPointerEvent());
  DCHECK(!waiting_on_event_targeter_);
  const EventSource event_source =
      event.IsMousePointerEvent() ? EventSource::MOUSE : EventSource::TOUCH;
  DCHECK(event_location.location == event.AsPointerEvent()->root_location_f());
  DCHECK(event_location.location == event.AsPointerEvent()->location_f());
  if (ShouldUseEventTargeter(*event.AsPointerEvent())) {
    waiting_on_event_targeter_ = true;
    event_targeter_->FindTargetForLocation(
        event_source, event_location,
        base::BindOnce(&EventDispatcher::ProcessPointerEventOnFoundTarget,
                       base::Unretained(this), *event.AsPointerEvent()));
  } else {
    ProcessPointerEventOnFoundTargetImpl(*event.AsPointerEvent(),
                                         event_location, nullptr);
  }
}

ServerWindow* EventDispatcher::GetRootWindowForDisplay(int64_t display_id) {
  return delegate_->GetRootWindowForDisplay(display_id);
}

void EventDispatcher::ProcessNextAvailableEvent() {
  delegate_->ProcessNextAvailableEvent();
}

viz::HitTestQuery* EventDispatcher::GetHitTestQueryForDisplay(
    int64_t display_id) {
  return delegate_->GetHitTestQueryForDisplay(display_id);
}

ServerWindow* EventDispatcher::GetWindowFromFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  return delegate_->GetWindowFromFrameSinkId(frame_sink_id);
}

DeepestWindow EventDispatcher::AdjustTargetForModal(
    const DeepestWindow& target) const {
  const ServerWindow* modal_transient =
      modal_window_controller_.GetModalTransient(target.window);
  if (!modal_transient &&
      !modal_window_controller_.IsWindowBlocked(target.window)) {
    return target;
  }

  DeepestWindow updated_target = target;
  updated_target.in_non_client_area = true;
  updated_target.window =
      target.window ? delegate_->GetFallbackTargetForEventBlockedByModal(
                          target.window->GetRootForDrawn())
                    : nullptr;
  return updated_target;
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

void EventDispatcher::SetMousePointerLocation(
    const gfx::PointF& new_mouse_location,
    int64_t new_mouse_display_id) {
  // TODO: this needs to account for grab and a different display. By that I
  // mean during a grab |mouse_pointer_last_location_| may actually be in a
  // different display.
  mouse_pointer_last_location_ = new_mouse_location;
  mouse_pointer_display_id_ = new_mouse_display_id;
}

void EventDispatcher::ProcessKeyEvent(const ui::KeyEvent& event,
                                      int64_t display_id,
                                      AcceleratorMatchPhase match_phase) {
  Accelerator* post_target =
      FindAccelerator(event, ui::mojom::AcceleratorPhase::POST_TARGET);
  if (drag_controller_ && event.type() == ui::ET_KEY_PRESSED &&
      event.key_code() == ui::VKEY_ESCAPE) {
    drag_controller_->Cancel();
    return;
  }
  ServerWindow* focused_window =
      delegate_->GetFocusedWindowForEventDispatcher(display_id);
  if (focused_window) {
    // We only hide the cursor when there's a window to receive the key
    // event. We want to hide the cursor when the user is entering text
    // somewhere so if the user is at the desktop with no window to react to
    // the key press, there's no reason to hide the cursor.
    HideCursorOnMatchedKeyEvent(event);

    // Assume key events are for the client area.
    const bool in_nonclient_area = false;
    const ClientSpecificId client_id =
        delegate_->GetEventTargetClientId(focused_window, in_nonclient_area);
    delegate_->DispatchInputEventToWindow(focused_window, client_id,
                                          EventLocation(display_id), event,
                                          post_target);
    return;
  }
  delegate_->OnEventTargetNotFound(event, display_id);
  if (post_target)
    delegate_->OnAccelerator(post_target->id(), display_id, event,
                             EventDispatcherDelegate::AcceleratorPhase::POST);
}

void EventDispatcher::HideCursorOnMatchedKeyEvent(const ui::KeyEvent& event) {
  if (event.IsSynthesized()) {
    // Don't bother performing the matching; it will be rejected anyway.
    return;
  }

  bool hide_cursor = !dont_hide_cursor_matchers_.empty();
  for (auto& matcher : dont_hide_cursor_matchers_) {
    if (matcher.MatchesEvent(event)) {
      hide_cursor = false;
      break;
    }
  }

  if (hide_cursor)
    delegate_->OnEventChangesCursorVisibility(event, false);
}

bool EventDispatcher::ShouldUseEventTargeter(const PointerEvent& event) const {
  const int32_t pointer_id = event.pointer_details().id;
  if (drag_controller_)
    return true;

  if (capture_window_)
    return false;

  auto iter = pointer_targets_.find(pointer_id);
  if (iter == pointer_targets_.end() || !iter->second.is_pointer_down)
    return true;

  return (event.IsMousePointerEvent() && IsPointerGoingUp(event)) ||
         event.type() == ET_POINTER_DOWN;
}

void EventDispatcher::ProcessPointerEventOnFoundTarget(
    const ui::PointerEvent& event,
    const EventLocation& event_location,
    const DeepestWindow& target) {
  DCHECK(waiting_on_event_targeter_);
  waiting_on_event_targeter_ = false;
  ProcessPointerEventOnFoundTargetImpl(event, event_location, &target);
}

void EventDispatcher::ProcessPointerEventOnFoundTargetImpl(
    const ui::PointerEvent& event,
    const EventLocation& event_location,
    const DeepestWindow* found_target) {
  DCHECK(!waiting_on_event_targeter_);
  // WARNING: |found_target| may be null!
  std::unique_ptr<ui::Event> cloned_event = ui::Event::Clone(event);

  UpdateCursorRelatedProperties(event, event_location);

  const bool is_mouse_event = event.IsMousePointerEvent();
  const bool is_pointer_going_up = IsPointerGoingUp(event);

  // Update mouse down state upon events which change it.
  if (is_mouse_event) {
    if (event.type() == ui::ET_POINTER_DOWN)
      mouse_button_down_ = true;
    else if (is_pointer_going_up)
      mouse_button_down_ = false;
  }

  if (drag_controller_) {
    DCHECK(found_target);
    if (drag_controller_->DispatchPointerEvent(
            *cloned_event->AsPointerEvent(),
            AdjustTargetForModal(*found_target).window)) {
      return;
    }
  }

  if (capture_window_) {
    SetMouseCursorSourceWindow(capture_window_);
    DispatchToClient(capture_window_, capture_window_client_id_,
                     *cloned_event->AsPointerEvent(), event_location);
    return;
  }

  std::unique_ptr<DeepestWindowAndTarget> result;
  if (found_target) {
    result = std::make_unique<DeepestWindowAndTarget>();
    result->deepest_window = AdjustTargetForModal(*found_target);
    result->pointer_target.is_mouse_event = is_mouse_event;
    result->pointer_target.window = result->deepest_window.window;
    result->pointer_target.in_nonclient_area =
        result->deepest_window.in_non_client_area;
    result->pointer_target.is_pointer_down =
        event.type() == ui::ET_POINTER_DOWN;
    result->pointer_target.display_id = event_location.display_id;
  }

  const int32_t pointer_id = event.pointer_details().id;

  if (!IsTrackingPointer(pointer_id) ||
      !pointer_targets_[pointer_id].is_pointer_down) {
    DCHECK(result);
    const bool any_pointers_down = AreAnyPointersDown();
    UpdateTargetForPointer(pointer_id, *cloned_event->AsPointerEvent(),
                           result->pointer_target, event_location);
    if (is_mouse_event)
      SetMouseCursorSourceWindow(pointer_targets_[pointer_id].window);

    PointerTarget& pointer_target = pointer_targets_[pointer_id];
    if (pointer_target.is_pointer_down) {
      if (!any_pointers_down) {
        // Don't attempt to change focus on pointer down. We assume client code
        // will do that.
        ServerWindow* capture_window = pointer_target.window;
        if (!capture_window) {
          capture_window =
              delegate_->GetRootWindowForDisplay(event_location.display_id);
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
  if (is_pointer_going_up && is_mouse_event) {
    DCHECK(result);
    UpdateCursorProvider(result->deepest_window);
  }

  DispatchToPointerTarget(pointer_targets_[pointer_id],
                          *cloned_event->AsPointerEvent(), event_location);

  if (is_pointer_going_up) {
    if (is_mouse_event)
      pointer_targets_[pointer_id].is_pointer_down = false;
    else
      StopTrackingPointer(pointer_id);
    if (!AreAnyPointersDown())
      delegate_->ReleaseNativeCapture();
  }

  if (event.type() == ET_POINTER_DOWN) {
    // Use |found_target| as |result| has already been adjusted for the
    // modal window.
    DCHECK(found_target);
    if (found_target->window)
      HandleClickOnBlockedWindow(*found_target);
  }
}

void EventDispatcher::UpdateCursorRelatedProperties(
    const ui::PointerEvent& event,
    const EventLocation& event_location) {
  if (event.IsMousePointerEvent()) {
    // This corresponds to the code in CompoundEventFilter which updates
    // visibility on each mouse event. Here, we're sure that we're a non-exit
    // mouse event and FROM_TOUCH doesn't exist in mus so we shouldn't need
    // further filtering.
    delegate_->OnEventChangesCursorTouchVisibility(event, true);
    delegate_->OnEventChangesCursorVisibility(event, true);

    SetMousePointerLocation(event_location.raw_location,
                            event_location.display_id);
    delegate_->OnMouseCursorLocationChanged(event_location.raw_location,
                                            event_location.display_id);
  } else {
    // When we have a non-touch event that wasn't synthesized, hide the mouse
    // cursor until the next non-synthesized mouse event.
    delegate_->OnEventChangesCursorTouchVisibility(event, false);
  }
}

void EventDispatcher::UpdateNonClientAreaForCurrentWindowOnFoundWindow(
    const EventLocation& event_location,
    const DeepestWindow& target) {
  if (!mouse_cursor_source_window_)
    return;

  const DeepestWindow updated_target = AdjustTargetForModal(target);
  if (updated_target.window == mouse_cursor_source_window_) {
    mouse_cursor_in_non_client_area_ =
        mouse_cursor_source_window_ ? updated_target.in_non_client_area : false;
  }
  delegate_->UpdateNativeCursorFromDispatcher();
}

void EventDispatcher::UpdateCursorProviderByLastKnownLocationOnFoundWindow(
    const EventLocation& event_location,
    const DeepestWindow& target) {
  UpdateCursorProvider(AdjustTargetForModal(target));
}

void EventDispatcher::UpdateCursorProvider(const DeepestWindow& target) {
  if (mouse_button_down_)
    return;

  SetMouseCursorSourceWindow(target.window);
  if (mouse_cursor_source_window_) {
    mouse_cursor_in_non_client_area_ = target.in_non_client_area;
  } else {
    SetMouseCursorSourceWindow(
        delegate_->GetRootWindowForDisplay(mouse_pointer_display_id_));
    mouse_cursor_in_non_client_area_ = true;
  }
  delegate_->UpdateNativeCursorFromDispatcher();
}

void EventDispatcher::HandleClickOnBlockedWindow(const DeepestWindow& target) {
  ServerWindow* modal_transient =
      modal_window_controller_.GetModalTransient(target.window);
  if (modal_transient) {
    ServerWindow* toplevel =
        modal_window_controller_.GetToplevelWindow(target.window);
    DCHECK(toplevel);
    delegate_->SetFocusedWindowFromEventDispatcher(toplevel);
    delegate_->OnEventOccurredOutsideOfModalWindow(modal_transient);
  } else if (target.window->IsDrawn() &&
             modal_window_controller_.IsWindowBlocked(target.window) &&
             modal_window_controller_.GetActiveSystemModalWindow()) {
    delegate_->OnEventOccurredOutsideOfModalWindow(
        modal_window_controller_.GetActiveSystemModalWindow());
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

void EventDispatcher::UpdateTargetForPointer(
    int32_t pointer_id,
    const ui::PointerEvent& event,
    const PointerTarget& pointer_target,
    const EventLocation& event_location) {
  if (!IsTrackingPointer(pointer_id)) {
    StartTrackingPointer(pointer_id, pointer_target);
    return;
  }

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
    DispatchToPointerTarget(pointer_targets_[pointer_id], exit_event,
                            event_location);
  }

  // Technically we're updating in place, but calling start then stop makes for
  // simpler code.
  StopTrackingPointer(pointer_id);
  StartTrackingPointer(pointer_id, pointer_target);
}

bool EventDispatcher::AreAnyPointersDown() const {
  for (const auto& pair : pointer_targets_) {
    if (pair.second.is_pointer_down)
      return true;
  }
  return false;
}

void EventDispatcher::DispatchToPointerTarget(
    const PointerTarget& target,
    const ui::LocatedEvent& event,
    const EventLocation& event_location) {
  if (!target.window) {
    delegate_->OnEventTargetNotFound(event, target.display_id);
    return;
  }

  if (target.is_mouse_event)
    mouse_cursor_in_non_client_area_ = target.in_nonclient_area;

  DispatchToClient(target.window,
                   delegate_->GetEventTargetClientId(target.window,
                                                     target.in_nonclient_area),
                   event, event_location);
}

void EventDispatcher::DispatchToClient(ServerWindow* window,
                                       ClientSpecificId client_id,
                                       const ui::LocatedEvent& event,
                                       const EventLocation& event_location) {
  gfx::Point location = ConvertPointFromRootForEventDispatch(
      delegate_->GetRootWindowForEventDispatch(window), window,
      event.location());
  std::unique_ptr<ui::Event> clone = ui::Event::Clone(event);
  clone->AsLocatedEvent()->set_location(location);
  // TODO(jonross): add post-target accelerator support once accelerators
  // support pointer events.
  delegate_->DispatchInputEventToWindow(window, client_id, event_location,
                                        *clone, nullptr);
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
  auto iter = observed_windows_.find(window);
  if (iter != observed_windows_.end()) {
    iter->second->num_observers++;
    return;
  }
  std::unique_ptr<ObservedWindow> observed_window =
      std::make_unique<ObservedWindow>();
  observed_window->num_observers = 1;
  observed_window->drawn_tracker =
      std::make_unique<ServerWindowDrawnTracker>(window, this);
  observed_windows_[window] = std::move(observed_window);
}

void EventDispatcher::UnobserveWindow(ServerWindow* window) {
  auto it = observed_windows_.find(window);
  DCHECK(it != observed_windows_.end());
  DCHECK_LT(0u, it->second->num_observers);
  if (--it->second->num_observers == 0u)
    observed_windows_.erase(it);
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
    DispatchToPointerTarget(pair.second, event,
                            EventLocation(pair.second.display_id));
  }
  pointer_targets_.clear();
}

void EventDispatcher::WindowNoLongerValidTarget(ServerWindow* window) {
  CancelPointerEventsToTarget(window);
  if (mouse_cursor_source_window_ == window)
    SetMouseCursorSourceWindow(nullptr);
}

void EventDispatcher::OnDrawnStateChanged(ServerWindow* ancestor,
                                          ServerWindow* window,
                                          bool is_drawn) {
  if (!is_drawn)
    WindowNoLongerValidTarget(window);
}

void EventDispatcher::OnRootDidChange(ServerWindow* ancestor,
                                      ServerWindow* window) {
  if (delegate_->IsWindowInDisplayRoot(window))
    return;

  WindowNoLongerValidTarget(window);
}

void EventDispatcher::OnDragCursorUpdated() {
  delegate_->UpdateNativeCursorFromDispatcher();
}

}  // namespace ws
}  // namespace ui

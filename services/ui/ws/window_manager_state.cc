// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_manager_state.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/ui/common/accelerator_util.h"
#include "services/ui/ws/accelerator.h"
#include "services/ui/ws/cursor_location_manager.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_creation_config.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/event_targeter.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/user_id_tracker.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {
namespace ws {
namespace {

// Flags that matter when checking if a key event matches an accelerator.
const int kAcceleratorEventFlags =
    EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN | EF_COMMAND_DOWN;

base::TimeDelta GetDefaultAckTimerDelay() {
#if defined(NDEBUG)
  return base::TimeDelta::FromMilliseconds(100);
#else
  return base::TimeDelta::FromMilliseconds(1000);
#endif
}

bool EventsCanBeCoalesced(const ui::Event& one, const ui::Event& two) {
  if (one.type() != two.type() || one.flags() != two.flags())
    return false;

  // TODO(sad): wheel events can also be merged.
  if (one.type() != ui::ET_POINTER_MOVED)
    return false;

  return one.AsPointerEvent()->pointer_details().id ==
         two.AsPointerEvent()->pointer_details().id;
}

std::unique_ptr<ui::Event> CoalesceEvents(std::unique_ptr<ui::Event> first,
                                          std::unique_ptr<ui::Event> second) {
  DCHECK(first->type() == ui::ET_POINTER_MOVED)
      << " Non-move events cannot be merged yet.";
  // For mouse moves, the new event just replaces the old event, but we need to
  // use the latency from the old event.
  second->set_latency(*first->latency());
  second->latency()->set_coalesced();
  return second;
}

const ServerWindow* GetEmbedRoot(const ServerWindow* window) {
  DCHECK(window);
  const ServerWindow* embed_root = window->parent();
  while (embed_root && embed_root->id().client_id == window->id().client_id)
    embed_root = embed_root->parent();
  return embed_root;
}

}  // namespace

WindowManagerState::InFlightEventDispatchDetails::InFlightEventDispatchDetails(
    WindowManagerState* window_manager_state,
    WindowTree* tree,
    int64_t display_id,
    const Event& event,
    EventDispatchPhase phase)
    : tree(tree),
      display_id(display_id),
      event(Event::Clone(event)),
      phase(phase),
      weak_factory(window_manager_state) {}

WindowManagerState::InFlightEventDispatchDetails::
    ~InFlightEventDispatchDetails() {}

class WindowManagerState::ProcessedEventTarget {
 public:
  ProcessedEventTarget(ServerWindow* window,
                       ClientSpecificId client_id,
                       Accelerator* accelerator)
      : client_id_(client_id) {
    tracker_.Add(window);
    if (accelerator)
      accelerator_ = accelerator->GetWeakPtr();
  }

  ~ProcessedEventTarget() {}

  // Return true if the event is still valid. The event becomes invalid if
  // the window is destroyed while waiting to dispatch.
  bool IsValid() const { return !tracker_.windows().empty(); }

  ServerWindow* window() {
    DCHECK(IsValid());
    return tracker_.windows().front();
  }

  ClientSpecificId client_id() const { return client_id_; }

  base::WeakPtr<Accelerator> accelerator() { return accelerator_; }

 private:
  ServerWindowTracker tracker_;
  const ClientSpecificId client_id_;
  base::WeakPtr<Accelerator> accelerator_;

  DISALLOW_COPY_AND_ASSIGN(ProcessedEventTarget);
};

bool WindowManagerState::DebugAccelerator::Matches(
    const ui::KeyEvent& event) const {
  return key_code == event.key_code() &&
         event_flags == (kAcceleratorEventFlags & event.flags()) &&
         !event.is_char();
}

WindowManagerState::QueuedEvent::QueuedEvent() {}
WindowManagerState::QueuedEvent::~QueuedEvent() {}

WindowManagerState::WindowManagerState(WindowTree* window_tree)
    : window_tree_(window_tree),
      event_dispatcher_(this),
      cursor_state_(window_tree_->display_manager(), this) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  AddDebugAccelerators();
}

WindowManagerState::~WindowManagerState() {
  for (auto& display_root : window_manager_display_roots_)
    display_root->display()->OnWillDestroyTree(window_tree_);

  if (window_tree_->automatically_create_display_roots()) {
    for (auto& display_root : orphaned_window_manager_display_roots_)
      display_root->root()->RemoveObserver(this);
  }
}

void WindowManagerState::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  got_frame_decoration_values_ = true;
  frame_decoration_values_ = values.Clone();
  UserDisplayManager* user_display_manager =
      display_manager()->GetUserDisplayManager(user_id());
  user_display_manager->OnFrameDecorationValuesChanged();
  if (window_server()->display_creation_config() ==
          DisplayCreationConfig::MANUAL &&
      display_manager()->got_initial_config_from_window_manager()) {
    user_display_manager->CallOnDisplaysChanged();
  }
}

bool WindowManagerState::SetCapture(ServerWindow* window,
                                    ClientSpecificId client_id) {
  DCHECK(IsActive());
  if (capture_window() == window &&
      client_id == event_dispatcher_.capture_window_client_id()) {
    return true;
  }
#if DCHECK_IS_ON()
  if (window) {
    WindowManagerDisplayRoot* display_root =
        display_manager()->GetWindowManagerDisplayRoot(window);
    DCHECK(display_root && display_root->window_manager_state() == this);
  }
#endif
  return event_dispatcher_.SetCaptureWindow(window, client_id);
}

void WindowManagerState::ReleaseCaptureBlockedByModalWindow(
    const ServerWindow* modal_window) {
  event_dispatcher_.ReleaseCaptureBlockedByModalWindow(modal_window);
}

void WindowManagerState::ReleaseCaptureBlockedByAnyModalWindow() {
  event_dispatcher_.ReleaseCaptureBlockedByAnyModalWindow();
}

void WindowManagerState::SetCursorLocation(const gfx::Point& display_pixels,
                                           int64_t display_id) {
  Display* display = display_manager()->GetDisplayById(display_id);
  if (!display) {
    NOTIMPLEMENTED() << "Window manager sent invalid display_id!";
    return;
  }

  event_dispatcher()->SetMousePointerDisplayLocation(display_pixels,
                                                     display_id);
  UpdateNativeCursorFromDispatcher();
  display->platform_display()->MoveCursorTo(display_pixels);
}

void WindowManagerState::SetKeyEventsThatDontHideCursor(
    std::vector<::ui::mojom::EventMatcherPtr> dont_hide_cursor_list) {
  event_dispatcher()->SetKeyEventsThatDontHideCursor(
      std::move(dont_hide_cursor_list));
}

void WindowManagerState::SetCursorTouchVisible(bool enabled) {
  cursor_state_.SetCursorTouchVisible(enabled);
}

void WindowManagerState::SetDragDropSourceWindow(
    DragSource* drag_source,
    ServerWindow* window,
    DragTargetConnection* source_connection,
    const std::unordered_map<std::string, std::vector<uint8_t>>& drag_data,
    uint32_t drag_operation) {
  int32_t drag_pointer = MouseEvent::kMousePointerId;
  if (in_flight_event_dispatch_details_ &&
      in_flight_event_dispatch_details_->event->IsPointerEvent()) {
    drag_pointer = in_flight_event_dispatch_details_->event->AsPointerEvent()
                       ->pointer_details()
                       .id;
  } else {
    NOTIMPLEMENTED() << "Set drag drop set up during something other than a "
                     << "pointer event; rejecting drag.";
    drag_source->OnDragCompleted(false, ui::mojom::kDropEffectNone);
    return;
  }

  event_dispatcher_.SetDragDropSourceWindow(drag_source, window,
                                            source_connection, drag_pointer,
                                            drag_data, drag_operation);
}

void WindowManagerState::CancelDragDrop() {
  event_dispatcher_.CancelDragDrop();
}

void WindowManagerState::EndDragDrop() {
  event_dispatcher_.EndDragDrop();
  UpdateNativeCursorFromDispatcher();
}

void WindowManagerState::AddSystemModalWindow(ServerWindow* window) {
  event_dispatcher_.AddSystemModalWindow(window);
}

void WindowManagerState::DeleteWindowManagerDisplayRoot(
    ServerWindow* display_root) {
  for (auto iter = orphaned_window_manager_display_roots_.begin();
       iter != orphaned_window_manager_display_roots_.end(); ++iter) {
    if ((*iter)->root() == display_root) {
      orphaned_window_manager_display_roots_.erase(iter);
      return;
    }
  }

  for (auto iter = window_manager_display_roots_.begin();
       iter != window_manager_display_roots_.end(); ++iter) {
    if ((*iter)->root() == display_root) {
      (*iter)->display()->RemoveWindowManagerDisplayRoot((*iter).get());
      window_manager_display_roots_.erase(iter);
      return;
    }
  }

  NOTREACHED();
}

const UserId& WindowManagerState::user_id() const {
  return window_tree_->user_id();
}

void WindowManagerState::OnWillDestroyTree(WindowTree* tree) {
  event_dispatcher_.OnWillDestroyDragTargetConnection(tree);

  if (!in_flight_event_dispatch_details_ ||
      in_flight_event_dispatch_details_->tree != tree)
    return;

  // The WindowTree is dying. So it's not going to ack the event.
  // If the dying tree matches the root |tree_| mark as handled so we don't
  // notify it of accelerators.
  OnEventAck(in_flight_event_dispatch_details_->tree,
             tree == window_tree_ ? mojom::EventResult::HANDLED
                                  : mojom::EventResult::UNHANDLED);
}

ServerWindow* WindowManagerState::GetOrphanedRootWithId(const WindowId& id) {
  for (auto& display_root_ptr : orphaned_window_manager_display_roots_) {
    if (display_root_ptr->root()->id() == id)
      return display_root_ptr->root();
  }
  return nullptr;
}

bool WindowManagerState::IsActive() const {
  return window_server()->user_id_tracker()->active_id() == user_id();
}

void WindowManagerState::Activate(const gfx::Point& mouse_location_on_display,
                                  int64_t display_id) {
  SetAllRootWindowsVisible(true);
  event_dispatcher_.Reset();
  event_dispatcher_.SetMousePointerDisplayLocation(mouse_location_on_display,
                                                   display_id);
}

void WindowManagerState::Deactivate() {
  SetAllRootWindowsVisible(false);
  event_dispatcher_.Reset();
  // The tree is no longer active, so no point in dispatching any further
  // events.
  std::queue<std::unique_ptr<QueuedEvent>> event_queue;
  event_queue.swap(event_queue_);
}

void WindowManagerState::ProcessEvent(const ui::Event& event,
                                      int64_t display_id) {
  // If this is still waiting for an ack from a previously sent event, then
  // queue up the event to be dispatched once the ack is received.
  if (event_dispatcher_.IsProcessingEvent() ||
      in_flight_event_dispatch_details_) {
    if (!event_queue_.empty() && !event_queue_.back()->processed_target &&
        EventsCanBeCoalesced(*event_queue_.back()->event, event)) {
      event_queue_.back()->event = CoalesceEvents(
          std::move(event_queue_.back()->event), ui::Event::Clone(event));
      event_queue_.back()->display_id = display_id;
      return;
    }
    QueueEvent(event, nullptr, display_id);
    return;
  }

  ProcessEventImpl(event, display_id);
}

void WindowManagerState::OnAcceleratorAck(
    mojom::EventResult result,
    const std::unordered_map<std::string, std::vector<uint8_t>>& properties) {
  DCHECK(in_flight_event_dispatch_details_);
  DCHECK_EQ(EventDispatchPhase::PRE_TARGET_ACCELERATOR,
            in_flight_event_dispatch_details_->phase);

  std::unique_ptr<InFlightEventDispatchDetails> details =
      std::move(in_flight_event_dispatch_details_);

  if (result == mojom::EventResult::UNHANDLED) {
    DCHECK(details->event->IsKeyEvent());
    if (!properties.empty())
      details->event->AsKeyEvent()->SetProperties(properties);
    event_dispatcher_.ProcessEvent(
        *details->event, details->display_id,
        EventDispatcher::AcceleratorMatchPhase::POST_ONLY);
  } else {
    // We're not going to process the event any further, notify event observers.
    // We don't do this first to ensure we don't send an event twice to clients.
    window_server()->SendToPointerWatchers(*details->event, user_id(), nullptr,
                                           nullptr, details->display_id);
    ProcessNextAvailableEvent();
  }
}

const WindowServer* WindowManagerState::window_server() const {
  return window_tree_->window_server();
}

WindowServer* WindowManagerState::window_server() {
  return window_tree_->window_server();
}

DisplayManager* WindowManagerState::display_manager() {
  return window_tree_->display_manager();
}

const DisplayManager* WindowManagerState::display_manager() const {
  return window_tree_->display_manager();
}

void WindowManagerState::AddWindowManagerDisplayRoot(
    std::unique_ptr<WindowManagerDisplayRoot> display_root) {
  window_manager_display_roots_.push_back(std::move(display_root));
}

void WindowManagerState::OnDisplayDestroying(Display* display) {
  if (display->platform_display() == platform_display_with_capture_)
    platform_display_with_capture_ = nullptr;

  for (auto iter = window_manager_display_roots_.begin();
       iter != window_manager_display_roots_.end(); ++iter) {
    if ((*iter)->display() == display) {
      if (window_tree_->automatically_create_display_roots())
        (*iter)->root()->AddObserver(this);
      orphaned_window_manager_display_roots_.push_back(std::move(*iter));
      window_manager_display_roots_.erase(iter);
      window_tree_->OnDisplayDestroying(display->GetId());
      orphaned_window_manager_display_roots_.back()->display_ = nullptr;
      return;
    }
  }
}

void WindowManagerState::SetAllRootWindowsVisible(bool value) {
  for (auto& display_root_ptr : window_manager_display_roots_)
    display_root_ptr->root()->SetVisible(value);
}

ServerWindow* WindowManagerState::GetWindowManagerRootForDisplayRoot(
    ServerWindow* window) {
  for (auto& display_root_ptr : window_manager_display_roots_) {
    if (display_root_ptr->root()->parent() == window)
      return display_root_ptr->GetClientVisibleRoot();
  }
  NOTREACHED();
  return nullptr;
}

void WindowManagerState::OnEventAck(mojom::WindowTree* tree,
                                    mojom::EventResult result) {
  DCHECK(in_flight_event_dispatch_details_);
  std::unique_ptr<InFlightEventDispatchDetails> details =
      std::move(in_flight_event_dispatch_details_);

  if (result == mojom::EventResult::UNHANDLED &&
      details->post_target_accelerator) {
    OnAccelerator(details->post_target_accelerator->id(), details->display_id,
                  *details->event, AcceleratorPhase::POST);
  }

  ProcessNextAvailableEvent();
}

void WindowManagerState::OnEventAckTimeout(ClientSpecificId client_id) {
  WindowTree* hung_tree = window_server()->GetTreeWithId(client_id);
  if (hung_tree && !hung_tree->janky())
    window_tree_->ClientJankinessChanged(hung_tree);
  if (in_flight_event_dispatch_details_->phase ==
      EventDispatchPhase::PRE_TARGET_ACCELERATOR) {
    OnAcceleratorAck(mojom::EventResult::UNHANDLED, KeyEvent::Properties());
  } else {
    OnEventAck(in_flight_event_dispatch_details_->tree,
               mojom::EventResult::UNHANDLED);
  }
}

void WindowManagerState::ProcessEventImpl(const ui::Event& event,
                                          int64_t display_id) {
  DCHECK(!in_flight_event_dispatch_details_ &&
         !event_dispatcher_.IsProcessingEvent());
  // Debug accelerators are always checked and don't interfere with processing.
  ProcessDebugAccelerator(event, display_id);
  event_dispatcher_.ProcessEvent(event, display_id,
                                 EventDispatcher::AcceleratorMatchPhase::ANY);
}

void WindowManagerState::QueueEvent(
    const ui::Event& event,
    std::unique_ptr<ProcessedEventTarget> processed_event_target,
    int64_t display_id) {
  std::unique_ptr<QueuedEvent> queued_event(new QueuedEvent);
  queued_event->event = ui::Event::Clone(event);
  queued_event->processed_target = std::move(processed_event_target);
  queued_event->display_id = display_id;
  event_queue_.push(std::move(queued_event));
}

// TODO(riajiang): We might want to do event targeting for the next event while
// waiting for the current event to be dispatched. crbug.com/724521
void WindowManagerState::DispatchInputEventToWindowImpl(
    ServerWindow* target,
    ClientSpecificId client_id,
    int64_t display_id,
    const ui::Event& event,
    base::WeakPtr<Accelerator> accelerator) {
  DCHECK(!in_flight_event_dispatch_details_);
  DCHECK(target);
  if (target->parent() == nullptr)
    target = GetWindowManagerRootForDisplayRoot(target);

  if (event.IsMousePointerEvent()) {
    DCHECK(event_dispatcher_.mouse_cursor_source_window());
    UpdateNativeCursorFromDispatcher();
  }

  WindowTree* tree = window_server()->GetTreeWithId(client_id);
  DCHECK(tree);
  ScheduleInputEventTimeout(tree, target, display_id, event,
                            EventDispatchPhase::TARGET);
  in_flight_event_dispatch_details_->post_target_accelerator = accelerator;

  // Ignore |tree| because it will receive the event via normal dispatch.
  window_server()->SendToPointerWatchers(
      event, user_id(), target, tree,
      in_flight_event_dispatch_details_->display_id);

  tree->DispatchInputEvent(
      target, event,
      base::BindOnce(
          &WindowManagerState::OnEventAck,
          in_flight_event_dispatch_details_->weak_factory.GetWeakPtr(), tree));
}

void WindowManagerState::AddDebugAccelerators() {
  const DebugAccelerator accelerator = {
      DebugAcceleratorType::PRINT_WINDOWS, ui::VKEY_S,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  debug_accelerators_.push_back(accelerator);
}

void WindowManagerState::ProcessDebugAccelerator(const ui::Event& event,
                                                 int64_t display_id) {
  if (event.type() != ui::ET_KEY_PRESSED)
    return;

  const ui::KeyEvent& key_event = *event.AsKeyEvent();
  for (const DebugAccelerator& accelerator : debug_accelerators_) {
    if (accelerator.Matches(key_event)) {
      HandleDebugAccelerator(accelerator.type, display_id);
      break;
    }
  }
}

void WindowManagerState::HandleDebugAccelerator(DebugAcceleratorType type,
                                                int64_t display_id) {
#if DCHECK_IS_ON()
  // Error so it will be collected in system logs.
  for (Display* display : display_manager()->displays()) {
    WindowManagerDisplayRoot* display_root =
        display->GetWindowManagerDisplayRootForUser(user_id());
    if (display_root) {
      LOG(ERROR) << "ServerWindow hierarchy:\n"
                 << display_root->root()->GetDebugWindowHierarchy();
    }
  }
  ServerWindow* focused_window = GetFocusedWindowForEventDispatcher(display_id);
  LOG(ERROR) << "Focused window: "
             << (focused_window ? focused_window->id().ToString() : "(null)");
#endif
}

void WindowManagerState::ScheduleInputEventTimeout(WindowTree* tree,
                                                   ServerWindow* target,
                                                   int64_t display_id,
                                                   const Event& event,
                                                   EventDispatchPhase phase) {
  std::unique_ptr<InFlightEventDispatchDetails> details =
      base::MakeUnique<InFlightEventDispatchDetails>(this, tree, display_id,
                                                     event, phase);

  // TOOD(sad): Adjust this delay, possibly make this dynamic.
  const base::TimeDelta max_delay = base::debug::BeingDebugged()
                                        ? base::TimeDelta::FromDays(1)
                                        : GetDefaultAckTimerDelay();
  details->timer.Start(
      FROM_HERE, max_delay,
      base::Bind(&WindowManagerState::OnEventAckTimeout,
                 details->weak_factory.GetWeakPtr(), tree->id()));
  in_flight_event_dispatch_details_ = std::move(details);
}

bool WindowManagerState::ConvertPointToScreen(int64_t display_id,
                                              gfx::Point* point) {
  Display* display = display_manager()->GetDisplayById(display_id);
  if (display) {
    const display::Display& originated_display = display->GetDisplay();
    *point = gfx::ConvertPointToDIP(originated_display.device_scale_factor(),
                                    *point);
    *point += originated_display.bounds().origin().OffsetFromOrigin();
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcherDelegate:

void WindowManagerState::OnAccelerator(uint32_t accelerator_id,
                                       int64_t display_id,
                                       const ui::Event& event,
                                       AcceleratorPhase phase) {
  DCHECK(IsActive());
  const bool needs_ack = phase == AcceleratorPhase::PRE;
  WindowTree::AcceleratorCallback ack_callback;
  if (needs_ack) {
    DCHECK(!in_flight_event_dispatch_details_);
    ScheduleInputEventTimeout(window_tree_, nullptr, display_id, event,
                              EventDispatchPhase::PRE_TARGET_ACCELERATOR);
    ack_callback = base::BindOnce(
        &WindowManagerState::OnAcceleratorAck,
        in_flight_event_dispatch_details_->weak_factory.GetWeakPtr());
  }
  window_tree_->OnAccelerator(accelerator_id, event, std::move(ack_callback));
}

void WindowManagerState::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  DCHECK(IsActive());
  window_server()->SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowManagerState::GetFocusedWindowForEventDispatcher(
    int64_t display_id) {
  ServerWindow* focused_window = window_server()->GetFocusedWindow();
  if (focused_window)
    return focused_window;

  // When none of the windows have focus return the window manager's root.
  for (auto& display_root_ptr : window_manager_display_roots_) {
    if (display_root_ptr->display()->GetId() == display_id)
      return display_root_ptr->GetClientVisibleRoot();
  }
  if (!window_manager_display_roots_.empty())
    return (*window_manager_display_roots_.begin())->GetClientVisibleRoot();
  return nullptr;
}

void WindowManagerState::SetNativeCapture(ServerWindow* window) {
  DCHECK(window);
  DCHECK(IsActive());
  WindowManagerDisplayRoot* display_root =
      display_manager()->GetWindowManagerDisplayRoot(window);
  DCHECK(display_root);
  platform_display_with_capture_ = display_root->display()->platform_display();
  platform_display_with_capture_->SetCapture();
}

void WindowManagerState::ReleaseNativeCapture() {
  // Tests trigger calling this without a corresponding SetNativeCapture().
  // TODO(sky): maybe abstract this away so that DCHECK can be added?
  if (!platform_display_with_capture_)
    return;

  platform_display_with_capture_->ReleaseCapture();
  platform_display_with_capture_ = nullptr;
}

void WindowManagerState::UpdateNativeCursorFromDispatcher() {
  const ui::CursorData cursor = event_dispatcher_.GetCurrentMouseCursor();
  cursor_state_.SetCurrentWindowCursor(cursor);
}

void WindowManagerState::OnCaptureChanged(ServerWindow* new_capture,
                                          ServerWindow* old_capture) {
  window_server()->ProcessCaptureChanged(new_capture, old_capture);
}

void WindowManagerState::OnMouseCursorLocationChanged(
    const gfx::Point& point_in_display,
    int64_t display_id) {
  gfx::Point point_in_screen(point_in_display);
  if (ConvertPointToScreen(display_id, &point_in_screen)) {
    window_server()
        ->display_manager()
        ->GetCursorLocationManager(user_id())
        ->OnMouseCursorLocationChanged(point_in_screen);
  }
  // If the display the |point_in_display| is on has been deleted, keep the old
  // cursor location.
}

void WindowManagerState::OnEventChangesCursorVisibility(const ui::Event& event,
                                                        bool visible) {
  if (event.IsSynthesized())
    return;
  cursor_state_.SetCursorVisible(visible);
}

void WindowManagerState::OnEventChangesCursorTouchVisibility(
    const ui::Event& event,
    bool visible) {
  if (event.IsSynthesized())
    return;

  // Setting cursor touch visibility needs to cause a callback which notifies a
  // caller so we can dispatch the state change to the window manager.
  cursor_state_.SetCursorTouchVisible(visible);
}

void WindowManagerState::DispatchInputEventToWindow(ServerWindow* target,
                                                    ClientSpecificId client_id,
                                                    int64_t display_id,
                                                    const ui::Event& event,
                                                    Accelerator* accelerator) {
  DCHECK(IsActive());
  // TODO(sky): this needs to see if another wms has capture and if so forward
  // to it.
  if (in_flight_event_dispatch_details_) {
    std::unique_ptr<ProcessedEventTarget> processed_event_target(
        new ProcessedEventTarget(target, client_id, accelerator));
    QueueEvent(event, std::move(processed_event_target), display_id);
    return;
  }

  base::WeakPtr<Accelerator> weak_accelerator;
  if (accelerator)
    weak_accelerator = accelerator->GetWeakPtr();
  DispatchInputEventToWindowImpl(target, client_id, display_id, event,
                                 weak_accelerator);
}

void WindowManagerState::ProcessNextAvailableEvent() {
  // Loop through |event_queue_| stopping after dispatching the first valid
  // event.
  while (!event_queue_.empty()) {
    if (in_flight_event_dispatch_details_)
      return;

    if (!event_queue_.front()->processed_target &&
        event_dispatcher_.IsProcessingEvent())
      return;

    std::unique_ptr<QueuedEvent> queued_event = std::move(event_queue_.front());
    event_queue_.pop();
    if (!queued_event->processed_target) {
      ProcessEventImpl(*queued_event->event, queued_event->display_id);
      return;
    }
    if (queued_event->processed_target->IsValid()) {
      DispatchInputEventToWindowImpl(
          queued_event->processed_target->window(),
          queued_event->processed_target->client_id(), queued_event->display_id,
          *queued_event->event, queued_event->processed_target->accelerator());
      return;
    }
  }
}

ClientSpecificId WindowManagerState::GetEventTargetClientId(
    const ServerWindow* window,
    bool in_nonclient_area) {
  if (in_nonclient_area) {
    // Events in the non-client area always go to the window manager.
    return window_tree_->id();
  }

  // If the window is an embed root, it goes to the tree embedded in the window.
  WindowTree* tree = window_server()->GetTreeWithRoot(window);
  if (!tree) {
    // Window is not an embed root, event goes to owner of the window.
    tree = window_server()->GetTreeWithId(window->id().client_id);
  }
  DCHECK(tree);

  // Ascend to the first tree marked as not embedder_intercepts_events().
  const ServerWindow* embed_root =
      tree->HasRoot(window) ? window : GetEmbedRoot(window);
  while (tree && tree->embedder_intercepts_events()) {
    DCHECK(tree->HasRoot(embed_root));
    tree = window_server()->GetTreeWithId(embed_root->id().client_id);
    embed_root = GetEmbedRoot(embed_root);
  }
  DCHECK(tree);
  return tree->id();
}

ServerWindow* WindowManagerState::GetRootWindowContaining(
    gfx::Point* location_in_display,
    int64_t* display_id) {
  if (window_manager_display_roots_.empty())
    return nullptr;

  gfx::Point location_in_screen(*location_in_display);
  if (!ConvertPointToScreen(*display_id, &location_in_screen))
    return nullptr;

  WindowManagerDisplayRoot* target_display_root = nullptr;
  for (auto& display_root_ptr : window_manager_display_roots_) {
    if (display_root_ptr->display()->GetDisplay().bounds().Contains(
            location_in_screen)) {
      target_display_root = display_root_ptr.get();
      break;
    }
  }

  // TODO(kylechar): Better handle locations outside the window. Overlapping X11
  // windows, dragging and touch sensors need to be handled properly.
  if (!target_display_root) {
    DVLOG(1) << "Invalid event location " << location_in_display->ToString()
             << " / display id " << *display_id;
    target_display_root = window_manager_display_roots_.begin()->get();
  }

  // Update |location_in_display| and |display_id| if the target display is
  // different from the originated display, e.g. drag-and-drop.
  if (*display_id != target_display_root->display()->GetId()) {
    gfx::Point origin =
        target_display_root->display()->GetDisplay().bounds().origin();
    *location_in_display = location_in_screen - origin.OffsetFromOrigin();
    *location_in_display = gfx::ConvertPointToPixel(
        target_display_root->display()->GetDisplay().device_scale_factor(),
        *location_in_display);
    *display_id = target_display_root->display()->GetId();
  }

  return target_display_root->GetClientVisibleRoot();
}

void WindowManagerState::OnEventTargetNotFound(const ui::Event& event,
                                               int64_t display_id) {
  window_server()->SendToPointerWatchers(event, user_id(), nullptr, /* window */
                                         nullptr /* ignore_tree */, display_id);
  if (event.IsMousePointerEvent())
    UpdateNativeCursorFromDispatcher();
}

void WindowManagerState::OnWindowEmbeddedAppDisconnected(ServerWindow* window) {
  for (auto iter = orphaned_window_manager_display_roots_.begin();
       iter != orphaned_window_manager_display_roots_.end(); ++iter) {
    if ((*iter)->root() == window) {
      window->RemoveObserver(this);
      orphaned_window_manager_display_roots_.erase(iter);
      return;
    }
  }
  NOTREACHED();
}

void WindowManagerState::OnCursorTouchVisibleChanged(bool enabled) {
  window_tree_->OnCursorTouchVisibleChanged(enabled);
}

}  // namespace ws
}  // namespace ui

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
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/user_id_tracker.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"
#include "ui/events/event.h"

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
  // For mouse moves, the new event just replaces the old event.
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

WindowManagerState::InFlightEventDetails::InFlightEventDetails(
    WindowTree* tree,
    int64_t display_id,
    const Event& event,
    EventDispatchPhase phase)
    : tree(tree),
      display_id(display_id),
      event(Event::Clone(event)),
      phase(phase) {}

WindowManagerState::InFlightEventDetails::~InFlightEventDetails() {}

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
    : window_tree_(window_tree), event_dispatcher_(this), weak_factory_(this) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  AddDebugAccelerators();
}

WindowManagerState::~WindowManagerState() {
  for (auto& display_root : window_manager_display_roots_)
    display_root->display()->OnWillDestroyTree(window_tree_);

  for (auto& display_root : orphaned_window_manager_display_roots_)
    display_root->root()->RemoveObserver(this);
}

void WindowManagerState::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  got_frame_decoration_values_ = true;
  frame_decoration_values_ = values.Clone();
  display_manager()
      ->GetUserDisplayManager(user_id())
      ->OnFrameDecorationValuesChanged();
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

void WindowManagerState::SetDragDropSourceWindow(
    DragSource* drag_source,
    ServerWindow* window,
    DragTargetConnection* source_connection,
    const std::unordered_map<std::string, std::vector<uint8_t>>& drag_data,
    uint32_t drag_operation) {
  int32_t drag_pointer = PointerEvent::kMousePointerId;
  if (in_flight_event_details_ &&
      in_flight_event_details_->event->IsPointerEvent()) {
    drag_pointer =
        in_flight_event_details_->event->AsPointerEvent()->pointer_details().id;
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
  DCHECK(!window->transient_parent());
  event_dispatcher_.AddSystemModalWindow(window);
}

const UserId& WindowManagerState::user_id() const {
  return window_tree_->user_id();
}

void WindowManagerState::OnWillDestroyTree(WindowTree* tree) {
  event_dispatcher_.OnWillDestroyDragTargetConnection(tree);

  if (!in_flight_event_details_ || in_flight_event_details_->tree != tree)
    return;

  // The WindowTree is dying. So it's not going to ack the event.
  // If the dying tree matches the root |tree_| marked as handled so we don't
  // notify it of accelerators.
  OnEventAck(in_flight_event_details_->tree,
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

void WindowManagerState::Activate(const gfx::Point& mouse_location_on_screen) {
  SetAllRootWindowsVisible(true);
  event_dispatcher_.Reset();
  event_dispatcher_.SetMousePointerScreenLocation(mouse_location_on_screen);
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
  if (in_flight_event_details_) {
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

void WindowManagerState::OnEventAck(mojom::WindowTree* tree,
                                    mojom::EventResult result) {
  if (!in_flight_event_details_ || in_flight_event_details_->tree != tree ||
      in_flight_event_details_->phase != EventDispatchPhase::TARGET) {
    // TODO(sad): The ack must have arrived after the timeout. We should do
    // something here, and in OnEventAckTimeout().
    return;
  }
  std::unique_ptr<InFlightEventDetails> details =
      std::move(in_flight_event_details_);

  base::WeakPtr<Accelerator> post_target_accelerator = post_target_accelerator_;
  post_target_accelerator_.reset();

  if (result == mojom::EventResult::UNHANDLED && post_target_accelerator) {
    OnAccelerator(post_target_accelerator->id(), *details->event,
                  AcceleratorPhase::POST);
  }

  ProcessNextEventFromQueue();
}

void WindowManagerState::OnAcceleratorAck(mojom::EventResult result) {
  if (!in_flight_event_details_ ||
      in_flight_event_details_->phase !=
          EventDispatchPhase::PRE_TARGET_ACCELERATOR) {
    // TODO(sad): The ack must have arrived after the timeout. We should do
    // something here, and in OnEventAckTimeout().
    return;
  }

  std::unique_ptr<InFlightEventDetails> details =
      std::move(in_flight_event_details_);

  if (result == mojom::EventResult::UNHANDLED) {
    event_processing_display_id_ = details->display_id;
    event_dispatcher_.ProcessEvent(
        *details->event, EventDispatcher::AcceleratorMatchPhase::POST_ONLY);
  } else {
    // We're not going to process the event any further, notify event observers.
    // We don't do this first to ensure we don't send an event twice to clients.
    window_server()->SendToPointerWatchers(*details->event, user_id(), nullptr,
                                           nullptr, details->display_id);
    ProcessNextEventFromQueue();
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
      (*iter)->root()->AddObserver(this);
      orphaned_window_manager_display_roots_.push_back(std::move(*iter));
      window_manager_display_roots_.erase(iter);
      window_tree_->OnDisplayDestroying(display->GetId());
      return;
    }
  }
  NOTREACHED();
}

void WindowManagerState::SetAllRootWindowsVisible(bool value) {
  for (auto& display_root_ptr : window_manager_display_roots_)
    display_root_ptr->root()->SetVisible(value);
}

ServerWindow* WindowManagerState::GetWindowManagerRoot(ServerWindow* window) {
  for (auto& display_root_ptr : window_manager_display_roots_) {
    if (display_root_ptr->root()->parent() == window)
      return display_root_ptr->root();
  }
  NOTREACHED();
  return nullptr;
}

void WindowManagerState::OnEventAckTimeout(ClientSpecificId client_id) {
  WindowTree* hung_tree = window_server()->GetTreeWithId(client_id);
  if (hung_tree && !hung_tree->janky())
    window_tree_->ClientJankinessChanged(hung_tree);
  if (in_flight_event_details_->phase ==
      EventDispatchPhase::PRE_TARGET_ACCELERATOR) {
    OnAcceleratorAck(mojom::EventResult::UNHANDLED);
  } else {
    OnEventAck(
        in_flight_event_details_ ? in_flight_event_details_->tree : nullptr,
        mojom::EventResult::UNHANDLED);
  }
}

void WindowManagerState::ProcessEventImpl(const ui::Event& event,
                                          int64_t display_id) {
  // Debug accelerators are always checked and don't interfere with processing.
  ProcessDebugAccelerator(event);
  event_processing_display_id_ = display_id;
  event_dispatcher_.ProcessEvent(event,
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

void WindowManagerState::ProcessNextEventFromQueue() {
  // Loop through |event_queue_| stopping after dispatching the first valid
  // event.
  while (!event_queue_.empty()) {
    std::unique_ptr<QueuedEvent> queued_event = std::move(event_queue_.front());
    event_queue_.pop();
    if (!queued_event->processed_target) {
      ProcessEventImpl(*queued_event->event, queued_event->display_id);
      return;
    }
    if (queued_event->processed_target->IsValid()) {
      DispatchInputEventToWindowImpl(
          queued_event->processed_target->window(),
          queued_event->processed_target->client_id(), *queued_event->event,
          queued_event->processed_target->accelerator());
      return;
    }
  }
}

void WindowManagerState::DispatchInputEventToWindowImpl(
    ServerWindow* target,
    ClientSpecificId client_id,
    const ui::Event& event,
    base::WeakPtr<Accelerator> accelerator) {
  DCHECK(target);
  if (target->parent() == nullptr)
    target = GetWindowManagerRoot(target);

  if (event.IsMousePointerEvent()) {
    DCHECK(event_dispatcher_.mouse_cursor_source_window());
    UpdateNativeCursorFromDispatcher();
  }

  WindowTree* tree = window_server()->GetTreeWithId(client_id);
  DCHECK(tree);
  ScheduleInputEventTimeout(tree, target, event, EventDispatchPhase::TARGET);

  if (accelerator)
    post_target_accelerator_ = accelerator;

  // Ignore |tree| because it will receive the event via normal dispatch.
  window_server()->SendToPointerWatchers(event, user_id(), target, tree,
                                         in_flight_event_details_->display_id);

  tree->DispatchInputEvent(target, event);
}

void WindowManagerState::AddDebugAccelerators() {
  const DebugAccelerator accelerator = {
      DebugAcceleratorType::PRINT_WINDOWS, ui::VKEY_S,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN};
  debug_accelerators_.push_back(accelerator);
}

void WindowManagerState::ProcessDebugAccelerator(const ui::Event& event) {
  if (event.type() != ui::ET_KEY_PRESSED)
    return;

  const ui::KeyEvent& key_event = *event.AsKeyEvent();
  for (const DebugAccelerator& accelerator : debug_accelerators_) {
    if (accelerator.Matches(key_event)) {
      HandleDebugAccelerator(accelerator.type);
      break;
    }
  }
}

void WindowManagerState::HandleDebugAccelerator(DebugAcceleratorType type) {
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
  ServerWindow* focused_window = GetFocusedWindowForEventDispatcher();
  LOG(ERROR) << "Focused window: "
             << (focused_window ? focused_window->id().ToString() : "(null)");
#endif
}

void WindowManagerState::ScheduleInputEventTimeout(WindowTree* tree,
                                                   ServerWindow* target,
                                                   const Event& event,
                                                   EventDispatchPhase phase) {
  std::unique_ptr<InFlightEventDetails> details =
      base::MakeUnique<InFlightEventDetails>(tree, event_processing_display_id_,
                                             event, phase);

  // TOOD(sad): Adjust this delay, possibly make this dynamic.
  const base::TimeDelta max_delay = base::debug::BeingDebugged()
                                        ? base::TimeDelta::FromDays(1)
                                        : GetDefaultAckTimerDelay();
  details->timer.Start(FROM_HERE, max_delay,
                       base::Bind(&WindowManagerState::OnEventAckTimeout,
                                  weak_factory_.GetWeakPtr(), tree->id()));
  in_flight_event_details_ = std::move(details);
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcherDelegate:

void WindowManagerState::OnAccelerator(uint32_t accelerator_id,
                                       const ui::Event& event,
                                       AcceleratorPhase phase) {
  DCHECK(IsActive());
  const bool needs_ack = phase == AcceleratorPhase::PRE;
  if (needs_ack) {
    DCHECK(!in_flight_event_details_);
    ScheduleInputEventTimeout(window_tree_, nullptr, event,
                              EventDispatchPhase::PRE_TARGET_ACCELERATOR);
  }
  window_tree_->OnAccelerator(accelerator_id, event, needs_ack);
}

void WindowManagerState::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  DCHECK(IsActive());
  window_server()->SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowManagerState::GetFocusedWindowForEventDispatcher() {
  return window_server()->GetFocusedWindow();
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
  const ui::mojom::Cursor cursor_id = event_dispatcher_.GetCurrentMouseCursor();
  for (Display* display : display_manager()->displays())
    display->UpdateNativeCursor(cursor_id);
}

void WindowManagerState::OnCaptureChanged(ServerWindow* new_capture,
                                          ServerWindow* old_capture) {
  window_server()->ProcessCaptureChanged(new_capture, old_capture);
}

void WindowManagerState::OnMouseCursorLocationChanged(const gfx::Point& point) {
  window_server()
      ->display_manager()
      ->GetCursorLocationManager(user_id())
      ->OnMouseCursorLocationChanged(point);
}

void WindowManagerState::DispatchInputEventToWindow(ServerWindow* target,
                                                    ClientSpecificId client_id,
                                                    const ui::Event& event,
                                                    Accelerator* accelerator) {
  DCHECK(IsActive());
  // TODO(sky): this needs to see if another wms has capture and if so forward
  // to it.
  if (in_flight_event_details_) {
    std::unique_ptr<ProcessedEventTarget> processed_event_target(
        new ProcessedEventTarget(target, client_id, accelerator));
    QueueEvent(event, std::move(processed_event_target),
               event_processing_display_id_);
    return;
  }

  base::WeakPtr<Accelerator> weak_accelerator;
  if (accelerator)
    weak_accelerator = accelerator->GetWeakPtr();
  DispatchInputEventToWindowImpl(target, client_id, event, weak_accelerator);
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
    gfx::Point* location) {
  if (window_manager_display_roots_.empty())
    return nullptr;

  WindowManagerDisplayRoot* target_display_root = nullptr;
  for (auto& display_root_ptr : window_manager_display_roots_) {
    if (display_root_ptr->display()->platform_display()->GetBounds().Contains(
            *location)) {
      target_display_root = display_root_ptr.get();
      break;
    }
  }

  // TODO(kylechar): Better handle locations outside the window. Overlapping X11
  // windows, dragging and touch sensors need to be handled properly.
  if (!target_display_root) {
    DVLOG(1) << "Invalid event location " << location->ToString();
    target_display_root = window_manager_display_roots_.begin()->get();
  }

  // Translate the location to be relative to the display instead of relative
  // to the screen space.
  gfx::Point origin =
      target_display_root->display()->platform_display()->GetBounds().origin();
  *location -= origin.OffsetFromOrigin();
  return target_display_root->root();
}

void WindowManagerState::OnEventTargetNotFound(const ui::Event& event) {
  window_server()->SendToPointerWatchers(event, user_id(), nullptr, /* window */
                                         nullptr /* ignore_tree */,
                                         event_processing_display_id_);
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

}  // namespace ws
}  // namespace ui

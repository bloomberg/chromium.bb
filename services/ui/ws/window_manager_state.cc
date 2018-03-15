// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_manager_state.h"

#include <utility>

#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/ui/common/accelerator_util.h"
#include "services/ui/ws/accelerator.h"
#include "services/ui/ws/cursor_location_manager.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_creation_config.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/event_location.h"
#include "services/ui/ws/event_targeter.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_tracker.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"

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

bool CanEventsBeCoalesced(const ui::Event& one, const ui::Event& two) {
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
  while (embed_root &&
         embed_root->owning_tree_id() == window->owning_tree_id()) {
    embed_root = embed_root->parent();
  }
  return embed_root;
}

const gfx::Rect& GetDisplayBoundsInPixels(Display* display) {
  return display->GetViewportMetrics().bounds_in_pixels;
}

gfx::Point PixelsToDips(Display* display, const gfx::Point& location) {
  return gfx::ConvertPointToDIP(
      display->GetDisplay().device_scale_factor() /
          display->GetViewportMetrics().ui_scale_factor,
      location);
}

gfx::Point DipsToPixels(Display* display, const gfx::Point& location) {
  return gfx::ConvertPointToPixel(
      display->GetDisplay().device_scale_factor() /
          display->GetViewportMetrics().ui_scale_factor,
      location);
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
  ProcessedEventTarget(WindowServer* window_server,
                       ServerWindow* window,
                       ClientSpecificId client_id,
                       Accelerator* accelerator)
      : window_server_(window_server), client_id_(client_id) {
    DCHECK(window_server_);
    tracker_.Add(window);
    if (accelerator)
      accelerator_ = accelerator->GetWeakPtr();
  }

  ~ProcessedEventTarget() {}

  // Return true if the event is still valid. The event becomes invalid if
  // the window is destroyed while waiting to dispatch.
  bool IsValid() {
    return window() && window_server_->GetTreeWithId(client_id_);
  }

  ServerWindow* window() {
    return tracker_.windows().empty() ? nullptr : tracker_.windows().front();
  }

  ClientSpecificId client_id() const { return client_id_; }

  base::WeakPtr<Accelerator> accelerator() { return accelerator_; }

 private:
  WindowServer* window_server_;
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

// Contains data used for event processing that needs to happen. See enum for
// details.
struct WindowManagerState::EventTask {
  enum class Type {
    // ProcessEvent() was called while waiting on a client or EventProcessor
    // to complete processing. |event| is non-null and |processed_target| is
    // null.
    kEvent,

    // In certain situations EventProcessor::ProcessEvent() generates more than
    // one event. When that happens, |kProcessedEvent| is used for all events
    // after the first. For example, a move may result in an exit for one
    // Window and and an enter for another Window. The event generated for the
    // enter results in an EventTask of type |kProcessedEvent|. In this case
    // both |event| and |processed_target| are valid.
    kProcessedEvent,

    // ScheduleCallbackWhenDoneProcessingEvents() is called while waiting on
    // a client or EventProcessor. |event| and |processed_target| are null.
    kClosure
  };

  EventTask() = default;
  ~EventTask() = default;

  Type type() const {
    if (done_closure)
      return Type::kClosure;
    if (processed_target) {
      DCHECK(event);
      return Type::kProcessedEvent;
    }
    DCHECK(event);
    return Type::kEvent;
  }

  std::unique_ptr<Event> event;
  std::unique_ptr<ProcessedEventTarget> processed_target;
  EventLocation event_location;
  base::OnceClosure done_closure;
};

WindowManagerState::WindowManagerState(WindowTree* window_tree)
    : window_tree_(window_tree),
      event_processor_(this, this),
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
      display_manager()->GetUserDisplayManager();
  user_display_manager->OnFrameDecorationValuesChanged();
  if (window_server()->display_creation_config() ==
          DisplayCreationConfig::MANUAL &&
      display_manager()->got_initial_config_from_window_manager()) {
    user_display_manager->CallOnDisplaysChanged();
  }
}

bool WindowManagerState::SetCapture(ServerWindow* window,
                                    ClientSpecificId client_id) {
  if (capture_window() == window &&
      client_id == event_processor_.capture_window_client_id()) {
    return true;
  }
#if DCHECK_IS_ON()
  if (window) {
    WindowManagerDisplayRoot* display_root =
        display_manager()->GetWindowManagerDisplayRoot(window);
    DCHECK(display_root && display_root->window_manager_state() == this);
  }
#endif
  return event_processor_.SetCaptureWindow(window, client_id);
}

void WindowManagerState::ReleaseCaptureBlockedByAnyModalWindow() {
  event_processor_.ReleaseCaptureBlockedByAnyModalWindow();
}

void WindowManagerState::SetCursorLocation(const gfx::Point& display_pixels,
                                           int64_t display_id) {
  Display* display = display_manager()->GetDisplayById(display_id);
  if (!display) {
    NOTIMPLEMENTED() << "Window manager sent invalid display_id!";
    return;
  }

  // MoveCursorTo() implicitly generates a mouse event.
  display->platform_display()->MoveCursorTo(display_pixels);
}

void WindowManagerState::SetKeyEventsThatDontHideCursor(
    std::vector<::ui::mojom::EventMatcherPtr> dont_hide_cursor_list) {
  event_processor()->SetKeyEventsThatDontHideCursor(
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

  event_processor_.SetDragDropSourceWindow(drag_source, window,
                                           source_connection, drag_pointer,
                                           drag_data, drag_operation);
}

void WindowManagerState::CancelDragDrop() {
  event_processor_.CancelDragDrop();
}

void WindowManagerState::EndDragDrop() {
  event_processor_.EndDragDrop();
  UpdateNativeCursorFromEventProcessor();
}

void WindowManagerState::AddSystemModalWindow(ServerWindow* window) {
  event_processor_.AddSystemModalWindow(window);
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
}

void WindowManagerState::OnWillDestroyTree(WindowTree* tree) {
  event_processor_.OnWillDestroyDragTargetConnection(tree);

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

void WindowManagerState::ProcessEvent(ui::Event* event, int64_t display_id) {
  EventLocation event_location(display_id);
  if (event->IsLocatedEvent()) {
    event_location.raw_location = event->AsLocatedEvent()->location_f();
    AdjustEventLocation(display_id, event->AsLocatedEvent());
    event_location.location = event->AsLocatedEvent()->root_location_f();
  }

  // If this is still waiting for an ack from a previously sent event, then
  // queue the event so it's dispatched once the ack is received.
  if (IsProcessingEvent()) {
    if (!event_tasks_.empty() &&
        event_tasks_.back()->type() == EventTask::Type::kEvent &&
        CanEventsBeCoalesced(*event_tasks_.back()->event, *event)) {
      event_tasks_.back()->event = CoalesceEvents(
          std::move(event_tasks_.back()->event), ui::Event::Clone(*event));
      event_tasks_.back()->event_location = event_location;
      return;
    }
    QueueEvent(*event, nullptr, event_location);
    return;
  }

  QueueEvent(*event, nullptr, event_location);
  ProcessEventTasks();
}

bool WindowManagerState::IsProcessingEvent() const {
  return in_flight_event_dispatch_details_ ||
         event_processor_.IsProcessingEvent();
}

void WindowManagerState::ScheduleCallbackWhenDoneProcessingEvents(
    base::OnceClosure closure) {
  DCHECK(closure);
  if (!IsProcessingEvent()) {
    std::move(closure).Run();
    return;
  }

  // TODO(sky): use make_unique (presubmit check fails on make_unique).
  std::unique_ptr<EventTask> event_task(new EventTask());
  event_task->done_closure = std::move(closure);
  event_tasks_.push(std::move(event_task));
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
    event_processor_.ProcessEvent(
        *details->event, EventLocation(details->display_id),
        EventProcessor::AcceleratorMatchPhase::POST_ONLY);
  } else {
    // We're not going to process the event any further, notify event observers.
    // We don't do this first to ensure we don't send an event twice to clients.
    window_server()->SendToPointerWatchers(*details->event, nullptr, nullptr,
                                           details->display_id);
    ProcessEventTasks();
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
                  *details->event, AcceleratorPhase::kPost);
  }

  ProcessEventTasks();
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
                                          const EventLocation& event_location) {
  DCHECK(!in_flight_event_dispatch_details_ &&
         !event_processor_.IsProcessingEvent());
  // Debug accelerators are always checked and don't interfere with processing.
  ProcessDebugAccelerator(event, event_location.display_id);
  event_processor_.ProcessEvent(event, event_location,
                                EventProcessor::AcceleratorMatchPhase::ANY);
}

void WindowManagerState::QueueEvent(
    const ui::Event& event,
    std::unique_ptr<ProcessedEventTarget> processed_event_target,
    const EventLocation& event_location) {
  std::unique_ptr<EventTask> queued_event(new EventTask);
  queued_event->event = ui::Event::Clone(event);
  queued_event->processed_target = std::move(processed_event_target);
  queued_event->event_location = event_location;
  event_tasks_.push(std::move(queued_event));
}

// TODO(riajiang): We might want to do event targeting for the next event while
// waiting for the current event to be dispatched. crbug.com/724521
void WindowManagerState::DispatchInputEventToWindowImpl(
    ServerWindow* target,
    ClientSpecificId client_id,
    const EventLocation& event_location,
    const ui::Event& event,
    base::WeakPtr<Accelerator> accelerator) {
  DCHECK(!in_flight_event_dispatch_details_);
  DCHECK(target);
  if (target->parent() == nullptr)
    target = GetWindowManagerRootForDisplayRoot(target);

  if (event.IsMousePointerEvent())
    UpdateNativeCursorFromEventProcessor();

  WindowTree* tree = window_server()->GetTreeWithId(client_id);
  DCHECK(tree);
  ScheduleInputEventTimeout(tree, target, event_location.display_id, event,
                            EventDispatchPhase::TARGET);
  in_flight_event_dispatch_details_->post_target_accelerator = accelerator;

  // Ignore |tree| because it will receive the event via normal dispatch.
  window_server()->SendToPointerWatchers(event, target, tree,
                                         event_location.display_id);

  tree->DispatchInputEvent(
      target, event, event_location,
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
        display->window_manager_display_root();
    if (display_root) {
      LOG(ERROR) << "ServerWindow hierarchy:\n"
                 << display_root->root()->GetDebugWindowHierarchy();
    }
  }
  ServerWindow* focused_window = GetFocusedWindowForEventProcessor(display_id);
  LOG(ERROR) << "Focused window: "
             << (focused_window ? focused_window->frame_sink_id().ToString()
                                : "(null)");
#endif
}

void WindowManagerState::ScheduleInputEventTimeout(WindowTree* tree,
                                                   ServerWindow* target,
                                                   int64_t display_id,
                                                   const Event& event,
                                                   EventDispatchPhase phase) {
  std::unique_ptr<InFlightEventDispatchDetails> details =
      std::make_unique<InFlightEventDispatchDetails>(this, tree, display_id,
                                                     event, phase);

  // TODO(sad): Adjust this delay, possibly make this dynamic.
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
  if (!display)
    return false;

  WindowManagerDisplayRoot* root = display->window_manager_display_root();
  if (!root)
    return false;

  const display::Display& originated_display = display->GetDisplay();
  gfx::Transform transform;
  transform.Scale(originated_display.device_scale_factor(),
                  originated_display.device_scale_factor());
  transform *= display->window_manager_display_root()
                   ->GetClientVisibleRoot()
                   ->transform();
  gfx::Transform invert;
  if (!transform.GetInverse(&invert))
    invert = transform;
  auto point_3f = gfx::Point3F(gfx::PointF(*point));
  invert.TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF()) +
           originated_display.bounds().origin().OffsetFromOrigin();
  return true;
}

Display* WindowManagerState::FindDisplayContainingPixelLocation(
    const gfx::Point& screen_pixels) {
  for (auto& display_root_ptr : window_manager_display_roots_) {
    if (GetDisplayBoundsInPixels(display_root_ptr->display())
            .Contains(screen_pixels)) {
      return display_root_ptr->display();
    }
  }
  return nullptr;
}

void WindowManagerState::AdjustEventLocation(int64_t display_id,
                                             LocatedEvent* event) {
  if (window_manager_display_roots_.empty())
    return;

  Display* display = display_manager()->GetDisplayById(display_id);
  if (!display)
    return;

  const gfx::Rect& display_bounds_in_pixels = GetDisplayBoundsInPixels(display);
  // Typical case is the display contains the location.
  if (gfx::Rect(display_bounds_in_pixels.size()).Contains(event->location())) {
    return;
  }

  // The location is outside the bounds of the specified display. This generally
  // happens when there is a grab and the mouse is moved to another display.
  // When this happens the location of the event is in terms of the pixel
  // display layout. Find the display using the pixel display layout.
  const gfx::Point screen_pixels =
      event->location() + display_bounds_in_pixels.origin().OffsetFromOrigin();
  Display* containing_display =
      FindDisplayContainingPixelLocation(screen_pixels);
  if (!containing_display) {
    DVLOG(1) << "Invalid event location " << event->location().ToString()
             << " / display id " << display_id;
    return;
  }

  // Adjust the location of the event to be in terms of the DIP display layout
  // (but in pixels). See EventLocation for details on this.
  const gfx::Point location_in_containing_display =
      screen_pixels -
      GetDisplayBoundsInPixels(containing_display).origin().OffsetFromOrigin();
  const gfx::Point screen_dip_location =
      containing_display->GetDisplay().bounds().origin() +
      PixelsToDips(containing_display, location_in_containing_display)
          .OffsetFromOrigin();
  const gfx::Point pixel_relative_location = DipsToPixels(
      display, screen_dip_location -
                   display->GetDisplay().bounds().origin().OffsetFromOrigin());
  event->set_location(pixel_relative_location);
  event->set_root_location(pixel_relative_location);
}

////////////////////////////////////////////////////////////////////////////////
// EventProcessorDelegate:

void WindowManagerState::OnAccelerator(uint32_t accelerator_id,
                                       int64_t display_id,
                                       const ui::Event& event,
                                       AcceleratorPhase phase) {
  const bool needs_ack = phase == AcceleratorPhase::kPre;
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

void WindowManagerState::SetFocusedWindowFromEventProcessor(
    ServerWindow* new_focused_window) {
  window_server()->SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowManagerState::GetFocusedWindowForEventProcessor(
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

  // Classic ash expects no native grab when in unified display.
  // See http://crbug.com/773348 for details.
  if (display_manager()->InUnifiedDisplayMode())
    return;

  WindowManagerDisplayRoot* display_root =
      display_manager()->GetWindowManagerDisplayRoot(window);
  DCHECK(display_root);
  platform_display_with_capture_ = display_root->display()->platform_display();
  platform_display_with_capture_->SetCapture();
}

void WindowManagerState::ReleaseNativeCapture() {
  // Classic ash expects no native grab when in unified display.
  // See http://crbug.com/773348 for details.
  if (display_manager()->InUnifiedDisplayMode())
    return;

  // Tests trigger calling this without a corresponding SetNativeCapture().
  // TODO(sky): maybe abstract this away so that DCHECK can be added?
  if (!platform_display_with_capture_)
    return;

  platform_display_with_capture_->ReleaseCapture();
  platform_display_with_capture_ = nullptr;
}

void WindowManagerState::UpdateNativeCursorFromEventProcessor() {
  const ui::CursorData cursor = event_processor_.GetCurrentMouseCursor();
  cursor_state_.SetCurrentWindowCursor(cursor);
}

void WindowManagerState::OnCaptureChanged(ServerWindow* new_capture,
                                          ServerWindow* old_capture) {
  window_server()->ProcessCaptureChanged(new_capture, old_capture);
}

void WindowManagerState::OnMouseCursorLocationChanged(
    const gfx::PointF& point_in_display,
    int64_t display_id) {
  gfx::Point point_in_screen = gfx::ToFlooredPoint(point_in_display);
  if (ConvertPointToScreen(display_id, &point_in_screen)) {
    window_server()
        ->display_manager()
        ->cursor_location_manager()
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

void WindowManagerState::DispatchInputEventToWindow(
    ServerWindow* target,
    ClientSpecificId client_id,
    const EventLocation& event_location,
    const ui::Event& event,
    Accelerator* accelerator) {
  if (in_flight_event_dispatch_details_) {
    std::unique_ptr<ProcessedEventTarget> processed_event_target =
        std::make_unique<ProcessedEventTarget>(window_server(), target,
                                               client_id, accelerator);
    QueueEvent(event, std::move(processed_event_target), event_location);
    return;
  }

  base::WeakPtr<Accelerator> weak_accelerator;
  if (accelerator)
    weak_accelerator = accelerator->GetWeakPtr();
  DispatchInputEventToWindowImpl(target, client_id, event_location, event,
                                 weak_accelerator);
}

void WindowManagerState::ProcessEventTasks() {
  // Loop through |event_tasks_| stopping after dispatching the first valid
  // event.
  while (!event_tasks_.empty() && !IsProcessingEvent()) {
    std::unique_ptr<EventTask> task = std::move(event_tasks_.front());
    event_tasks_.pop();

    switch (task->type()) {
      case EventTask::Type::kClosure:
        std::move(task->done_closure).Run();
        break;
      case EventTask::Type::kEvent:
        ProcessEventImpl(*task->event, task->event_location);
        break;
      case EventTask::Type::kProcessedEvent:
        if (task->processed_target->IsValid()) {
          DispatchInputEventToWindowImpl(task->processed_target->window(),
                                         task->processed_target->client_id(),
                                         task->event_location, *task->event,
                                         task->processed_target->accelerator());
        }
        break;
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
    tree = window_server()->GetTreeWithId(window->owning_tree_id());
  }
  DCHECK(tree);

  // Ascend to the first tree marked as not embedder_intercepts_events().
  const ServerWindow* embed_root =
      tree->HasRoot(window) ? window : GetEmbedRoot(window);
  while (tree && tree->embedder_intercepts_events()) {
    DCHECK(tree->HasRoot(embed_root));
    tree = window_server()->GetTreeWithId(embed_root->owning_tree_id());
    embed_root = GetEmbedRoot(embed_root);
  }
  DCHECK(tree);
  return tree->id();
}

ServerWindow* WindowManagerState::GetRootWindowForDisplay(int64_t display_id) {
  Display* display = display_manager()->GetDisplayById(display_id);
  if (!display)
    return nullptr;

  return display->window_manager_display_root()->GetClientVisibleRoot();
}

ServerWindow* WindowManagerState::GetRootWindowForEventDispatch(
    ServerWindow* window) {
  for (auto& display_root_ptr : window_manager_display_roots_) {
    ServerWindow* client_visible_root =
        display_root_ptr->GetClientVisibleRoot();
    if (client_visible_root->Contains(window))
      return client_visible_root;
  }
  return nullptr;
}

void WindowManagerState::OnEventTargetNotFound(const ui::Event& event,
                                               int64_t display_id) {
  window_server()->SendToPointerWatchers(event, nullptr, /* window */
                                         nullptr /* ignore_tree */, display_id);
  if (event.IsMousePointerEvent())
    UpdateNativeCursorFromEventProcessor();
}

ServerWindow* WindowManagerState::GetFallbackTargetForEventBlockedByModal(
    ServerWindow* window) {
  DCHECK(window);
  // TODO(sky): reevaluate when http://crbug.com/646998 is fixed.
  return GetWindowManagerRootForDisplayRoot(window);
}

void WindowManagerState::OnEventOccurredOutsideOfModalWindow(
    ServerWindow* modal_window) {
  window_tree_->OnEventOccurredOutsideOfModalWindow(modal_window);
}

viz::HitTestQuery* WindowManagerState::GetHitTestQueryForDisplay(
    int64_t display_id) {
  Display* display = display_manager()->GetDisplayById(display_id);
  if (!display)
    return nullptr;

  return window_server()->GetVizHostProxy()->GetHitTestQuery(
      display->root_window()->frame_sink_id());
}

ServerWindow* WindowManagerState::GetWindowFromFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK(frame_sink_id.is_valid());
  return window_tree()->GetWindowByClientId(frame_sink_id);
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

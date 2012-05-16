// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/screen.h"

using std::vector;

namespace aura {

namespace {

// These are the mouse events generated when a gesture goes unprocessed.
const ui::EventType kScrollBeginTypes[] = {
  ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_DRAGGED, ui::ET_UNKNOWN };

const ui::EventType kScrollEndTypes[] = {
  ui::ET_MOUSE_DRAGGED, ui::ET_MOUSE_RELEASED, ui::ET_UNKNOWN };

const ui::EventType kScrollUpdateTypes[] = {
  ui::ET_MOUSE_DRAGGED, ui::ET_UNKNOWN };

const ui::EventType kTapTypes[] = {
  ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_RELEASED, ui::ET_UNKNOWN };

const int kCompositorLockTimeoutMs = 67;

// Returns true if |target| has a non-client (frame) component at |location|,
// in window coordinates.
bool IsNonClientLocation(Window* target, const gfx::Point& location) {
  if (!target->delegate())
    return false;
  int hit_test_code = target->delegate()->GetNonClientComponent(location);
  return hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE;
}

typedef std::vector<EventFilter*> EventFilters;

void GetEventFiltersToNotify(Window* target, EventFilters* filters) {
  while (target) {
    if (target->event_filter())
      filters->push_back(target->event_filter());
    target = target->parent();
  }
}

float GetDeviceScaleFactorFromMonitor(const aura::Window* window) {
  MonitorManager* monitor_manager = Env::GetInstance()->monitor_manager();
  return monitor_manager->GetMonitorNearestWindow(window).device_scale_factor();
}

Window* ConsumerToWindow(ui::GestureConsumer* consumer) {
  return consumer && !consumer->ignores_events() ?
      static_cast<Window*>(consumer) : NULL;
}

}  // namespace

CompositorLock::CompositorLock(RootWindow* root_window)
    : root_window_(root_window) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CompositorLock::CancelLock, AsWeakPtr()),
      kCompositorLockTimeoutMs);
}

CompositorLock::~CompositorLock() {
  CancelLock();
}

void CompositorLock::CancelLock() {
  if (!root_window_)
    return;
  root_window_->UnlockCompositor();
  root_window_ = NULL;
}

bool RootWindow::hide_host_cursor_ = false;

////////////////////////////////////////////////////////////////////////////////
// RootWindow, public:

RootWindow::RootWindow(const gfx::Rect& initial_bounds)
    : Window(NULL),
      host_(aura::RootWindowHost::Create(initial_bounds)),
      ALLOW_THIS_IN_INITIALIZER_LIST(schedule_paint_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_factory_(this)),
      mouse_button_flags_(0),
      last_cursor_(ui::kCursorNull),
      cursor_shown_(true),
      capture_window_(NULL),
      mouse_pressed_handler_(NULL),
      mouse_moved_handler_(NULL),
      focused_window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          gesture_recognizer_(ui::GestureRecognizer::Create(this))),
      synthesize_mouse_move_(false),
      waiting_on_compositing_end_(false),
      draw_on_compositing_end_(false),
      defer_draw_scheduling_(false),
      mouse_move_hold_count_(0),
      should_hold_mouse_moves_(false),
      compositor_lock_(NULL),
      draw_on_compositor_unlock_(false),
      draw_trace_count_(0) {
  SetName("RootWindow");
  should_hold_mouse_moves_ = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAuraDisableHoldMouseMoves);

  compositor_.reset(new ui::Compositor(this, host_->GetAcceleratedWidget()));
  DCHECK(compositor_.get());
  compositor_->AddObserver(this);
}

RootWindow::~RootWindow() {
  if (compositor_lock_) {
    // No need to schedule a draw, we're going away.
    draw_on_compositor_unlock_ = false;
    compositor_lock_->CancelLock();
    DCHECK(!compositor_lock_);
  }
  compositor_->RemoveObserver(this);
  // Make sure to destroy the compositor before terminating so that state is
  // cleared and we don't hit asserts.
  compositor_.reset();

  // Tear down in reverse.  Frees any references held by the host.
  host_.reset(NULL);

  // An observer may have been added by an animation on the RootWindow.
  layer()->GetAnimator()->RemoveObserver(this);
}

void RootWindow::Init() {
  compositor()->SetScaleAndSize(GetDeviceScaleFactorFromMonitor(this),
                                host_->GetBounds().size());
  Window::Init(ui::LAYER_NOT_DRAWN);
  last_mouse_location_ =
      ui::ConvertPointToDIP(layer(), host_->QueryMouseLocation());
  compositor()->SetRootLayer(layer());
  SetBounds(
      ui::ConvertRectToDIP(layer(), gfx::Rect(host_->GetBounds().size())));
  Show();
  host_->SetRootWindow(this);
}

void RootWindow::ShowRootWindow() {
  host_->Show();
}

void RootWindow::SetHostSize(const gfx::Size& size_in_pixel) {
  DispatchHeldMouseMove();
  gfx::Rect bounds = host_->GetBounds();
  bounds.set_size(size_in_pixel);
  host_->SetBounds(bounds);
  // Requery the location to constrain it within the new root window size.
  last_mouse_location_ =
      ui::ConvertPointToDIP(layer(), host_->QueryMouseLocation());
  synthesize_mouse_move_ = false;
}

gfx::Size RootWindow::GetHostSize() const {
  return host_->GetBounds().size();
}

void RootWindow::SetHostBounds(const gfx::Rect& bounds_in_pixel) {
  DispatchHeldMouseMove();
  host_->SetBounds(bounds_in_pixel);
  // Requery the location to constrain it within the new root window size.
  last_mouse_location_ =
      ui::ConvertPointToDIP(layer(), host_->QueryMouseLocation());
  synthesize_mouse_move_ = false;
}

gfx::Point RootWindow::GetHostOrigin() const {
  return host_->GetBounds().origin();
}

void RootWindow::SetCursor(gfx::NativeCursor cursor) {
  last_cursor_ = cursor;
  // A lot of code seems to depend on NULL cursors actually showing an arrow,
  // so just pass everything along to the host.
  host_->SetCursor(cursor);
}

void RootWindow::ShowCursor(bool show) {
  cursor_shown_ = show;
  host_->ShowCursor(show);
}

void RootWindow::MoveCursorTo(const gfx::Point& location_in_dip) {
  host_->MoveCursorTo(ui::ConvertPointToPixel(layer(), location_in_dip));
}

bool RootWindow::ConfineCursorToWindow() {
  // We would like to be able to confine the cursor to that window. However,
  // currently, we do not have such functionality in X. So we just confine
  // to the root window. This is ok because this option is currently only
  // being used in fullscreen mode, so root_window bounds = window bounds.
  return host_->ConfineCursorToRootWindow();
}

void RootWindow::Draw() {
  if (waiting_on_compositing_end_) {
    draw_on_compositing_end_ = true;
    defer_draw_scheduling_ = false;
    return;
  }
  if (compositor_lock_) {
    draw_on_compositor_unlock_ = true;
    defer_draw_scheduling_ = false;
    return;
  }
  waiting_on_compositing_end_ = true;

  TRACE_EVENT_ASYNC_BEGIN0("ui", "RootWindow::Draw", draw_trace_count_++);

  compositor_->Draw(false);
  defer_draw_scheduling_ = false;
}

void RootWindow::ScheduleFullDraw() {
  compositor_->ScheduleFullDraw();
}

bool RootWindow::DispatchMouseEvent(MouseEvent* event) {
  if (mouse_move_hold_count_) {
    if (event->type() == ui::ET_MOUSE_DRAGGED ||
        (event->flags() & ui::EF_IS_SYNTHESIZED)) {
      held_mouse_move_.reset(new MouseEvent(*event, NULL, NULL));
      return true;
    } else {
      DispatchHeldMouseMove();
    }
  }
  return DispatchMouseEventImpl(event);
}

bool RootWindow::DispatchKeyEvent(KeyEvent* event) {
  DispatchHeldMouseMove();
  KeyEvent translated_event(*event);
  if (translated_event.key_code() == ui::VKEY_UNKNOWN)
    return false;
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(focused_window_)) {
    SetFocusedWindow(NULL, NULL);
    return false;
  }
  return ProcessKeyEvent(focused_window_, &translated_event);
}

bool RootWindow::DispatchScrollEvent(ScrollEvent* event) {
  DispatchHeldMouseMove();
  if (ui::IsDIPEnabled()) {
    float scale = ui::GetDeviceScaleFactor(layer());
    ui::Transform transform = layer()->transform();
    transform.ConcatScale(scale, scale);
    event->UpdateForRootTransform(transform);
  } else {
    event->UpdateForRootTransform(layer()->transform());
  }

  last_mouse_location_ = event->location();
  synthesize_mouse_move_ = false;

  Window* target =
      mouse_pressed_handler_ ? mouse_pressed_handler_ : capture_window_;
  if (!target)
    target = GetEventHandlerForPoint(event->location());

  if (target && target->delegate()) {
    int flags = event->flags();
    gfx::Point location_in_window = event->location();
    Window::ConvertPointToWindow(this, target, &location_in_window);
    if (IsNonClientLocation(target, location_in_window))
      flags |= ui::EF_IS_NON_CLIENT;
    ScrollEvent translated_event(*event, this, target, event->type(), flags);
    return ProcessMouseEvent(target, &translated_event);
  }
  return false;
}

bool RootWindow::DispatchTouchEvent(TouchEvent* event) {
  DispatchHeldMouseMove();
  if (ui::IsDIPEnabled()) {
    float scale = ui::GetDeviceScaleFactor(layer());
    ui::Transform transform = layer()->transform();
    transform.ConcatScale(scale, scale);
    event->UpdateForRootTransform(transform);
  } else {
    event->UpdateForRootTransform(layer()->transform());
  }
  bool handled = false;
  ui::TouchStatus status = ui::TOUCH_STATUS_UNKNOWN;
  Window* target = capture_window_;
  if (!target) {
    target = ConsumerToWindow(
        gesture_recognizer_->GetTouchLockedTarget(event));
    if (!target) {
      target = ConsumerToWindow(
          gesture_recognizer_->GetTargetForLocation(event->GetLocation()));
    }
  }

  if (!target && !bounds().Contains(event->location())) {
    // If the initial touch is outside the root window, target the root.
    target = this;
  } else {
    // We only come here when the first contact was within the root window.
    if (!target) {
      target = GetEventHandlerForPoint(event->location());
      if (!target)
        return false;
    }

    TouchEvent translated_event(*event, this, target);
    status = ProcessTouchEvent(target, &translated_event);
    handled = status != ui::TOUCH_STATUS_UNKNOWN;

    if (status == ui::TOUCH_STATUS_QUEUED ||
        status == ui::TOUCH_STATUS_QUEUED_END)
      gesture_recognizer_->QueueTouchEventForGesture(target, *event);
  }

  // Get the list of GestureEvents from GestureRecognizer.
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
      *event, status, target));

  return ProcessGestures(gestures.get()) ? true : handled;
}

bool RootWindow::DispatchGestureEvent(GestureEvent* event) {
  DispatchHeldMouseMove();

  Window* target = capture_window_;
  if (!target) {
    target = ConsumerToWindow(
        gesture_recognizer_->GetTargetForGestureEvent(event));
    if (!target)
      return false;
  }

  if (target) {
    GestureEvent translated_event(*event, this, target);
    ui::GestureStatus status = ProcessGestureEvent(target, &translated_event);
    return status != ui::GESTURE_STATUS_UNKNOWN;
  }

  return false;
}

void RootWindow::OnHostResized(const gfx::Size& size_in_pixel) {
  DispatchHeldMouseMove();
  // The compositor should have the same size as the native root window host.
  // Get the latest scale from monitor because it might have been changed.
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromMonitor(this),
                               size_in_pixel);
  gfx::Size old(bounds().size());
  // The layer, and all the observers should be notified of the
  // transformed size of the root window.
  gfx::Rect bounds(ui::ConvertSizeToDIP(layer(), size_in_pixel));
  layer()->transform().TransformRect(&bounds);
  SetBounds(bounds);
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowResized(this, old));
}

void RootWindow::OnWindowDestroying(Window* window) {
  OnWindowHidden(window, true);

  if (window->IsVisible() &&
      window->ContainsPointInRoot(last_mouse_location_)) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowBoundsChanged(Window* window,
                                       bool contained_mouse_point) {
  if (contained_mouse_point ||
      (window->IsVisible() &&
       window->ContainsPointInRoot(last_mouse_location_))) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowVisibilityChanged(Window* window, bool is_visible) {
  if (!is_visible)
    OnWindowHidden(window, false);

  if (window->ContainsPointInRoot(last_mouse_location_))
    PostMouseMoveEventAfterWindowChange();
}

void RootWindow::OnWindowTransformed(Window* window, bool contained_mouse) {
  if (contained_mouse ||
      (window->IsVisible() &&
       window->ContainsPointInRoot(last_mouse_location_))) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnKeyboardMappingChanged() {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnKeyboardMappingChanged(this));
}

void RootWindow::OnRootWindowHostClosed() {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowHostClosed(this));
}

void RootWindow::AddRootWindowObserver(RootWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void RootWindow::RemoveRootWindowObserver(RootWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RootWindow::PostNativeEvent(const base::NativeEvent& native_event) {
#if !defined(OS_MACOSX)
  host_->PostNativeEvent(native_event);
#endif
}

void RootWindow::ConvertPointToNativeScreen(gfx::Point* point) const {
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
}

void RootWindow::SetCapture(Window* window) {
  if (capture_window_ == window)
    return;

  gesture_recognizer_->CancelNonCapturedTouches(window);

  aura::Window* old_capture_window = capture_window_;
  capture_window_ = window;

  HandleMouseCaptureChanged(old_capture_window);

  if (capture_window_) {
    // Make all subsequent mouse events and touch go to the capture window. We
    // shouldn't need to send an event here as OnCaptureLost should take care of
    // that.
    if (mouse_moved_handler_ || mouse_button_flags_ != 0)
      mouse_moved_handler_ = capture_window_;
  } else {
    // When capture is lost, we must reset the event handlers.
    mouse_moved_handler_ = NULL;
  }
  mouse_pressed_handler_ = NULL;
}

void RootWindow::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

void RootWindow::AdvanceQueuedTouchEvent(Window* window, bool processed) {
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(gesture_recognizer_->AdvanceTouchQueue(window, processed));
  ProcessGestures(gestures.get());
}

void RootWindow::SetGestureRecognizerForTesting(ui::GestureRecognizer* gr) {
  gesture_recognizer_.reset(gr);
}

gfx::AcceleratedWidget RootWindow::GetAcceleratedWidget() {
  return host_->GetAcceleratedWidget();
}

#if !defined(NDEBUG)
void RootWindow::ToggleFullScreen() {
  host_->ToggleFullScreen();
}
#endif

void RootWindow::HoldMouseMoves() {
  if (should_hold_mouse_moves_)
    ++mouse_move_hold_count_;
}

void RootWindow::ReleaseMouseMoves() {
  if (should_hold_mouse_moves_) {
    --mouse_move_hold_count_;
    DCHECK_GE(mouse_move_hold_count_, 0);
    if (!mouse_move_hold_count_)
      DispatchHeldMouseMove();
  }
}

scoped_refptr<CompositorLock> RootWindow::GetCompositorLock() {
  if (!compositor_lock_)
    compositor_lock_ = new CompositorLock(this);
  return compositor_lock_;
}

void RootWindow::SetFocusWhenShown(bool focused) {
  host_->SetFocusWhenShown(focused);
}

bool RootWindow::GrabSnapshot(const gfx::Rect& snapshot_bounds,
                              std::vector<unsigned char>* png_representation) {
  DCHECK(bounds().Contains(snapshot_bounds));
  return host_->GrabSnapshot(snapshot_bounds, png_representation);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, Window overrides:

RootWindow* RootWindow::GetRootWindow() {
  return this;
}

const RootWindow* RootWindow::GetRootWindow() const {
  return this;
}

void RootWindow::SetTransform(const ui::Transform& transform) {
  Window::SetTransform(transform);

  // If the layer is not animating, then we need to update the host size
  // immediately.
  if (!layer()->GetAnimator()->is_animating())
    OnHostResized(host_->GetBounds().size());
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::CompositorDelegate implementation:

void RootWindow::ScheduleDraw() {
  if (compositor_lock_) {
    draw_on_compositor_unlock_ = true;
  } else if (!defer_draw_scheduling_) {
    defer_draw_scheduling_ = true;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RootWindow::Draw, schedule_paint_factory_.GetWeakPtr()));
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::CompositorObserver implementation:

void RootWindow::OnCompositingStarted(ui::Compositor*) {
}

void RootWindow::OnCompositingEnded(ui::Compositor*) {
  TRACE_EVENT_ASYNC_END0("ui", "RootWindow::Draw", draw_trace_count_);
  waiting_on_compositing_end_ = false;
  if (draw_on_compositing_end_) {
    draw_on_compositing_end_ = false;

    // Call ScheduleDraw() instead of Draw() in order to allow other
    // ui::CompositorObservers to be notified before starting another
    // draw cycle.
    ScheduleDraw();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

void RootWindow::HandleMouseCaptureChanged(Window* old_capture_window) {
  if (capture_window_)
    host_->SetCapture();
  else
    host_->ReleaseCapture();

  if (old_capture_window && old_capture_window->delegate()) {
    // Send a capture changed event with bogus location data.
    MouseEvent event(
        ui::ET_MOUSE_CAPTURE_CHANGED, gfx::Point(), gfx::Point(), 0);
    ProcessMouseEvent(old_capture_window, &event);

    old_capture_window->delegate()->OnCaptureLost();
  }
}

void RootWindow::HandleMouseMoved(const MouseEvent& event, Window* target) {
  if (target == mouse_moved_handler_)
    return;

  // Send an exited event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    MouseEvent translated_event(event, this, mouse_moved_handler_,
                                ui::ET_MOUSE_EXITED, event.flags());
    ProcessMouseEvent(mouse_moved_handler_, &translated_event);
  }
  mouse_moved_handler_ = target;
  // Send an entered event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    MouseEvent translated_event(event, this, mouse_moved_handler_,
                                ui::ET_MOUSE_ENTERED, event.flags());
    ProcessMouseEvent(mouse_moved_handler_, &translated_event);
  }
}

bool RootWindow::ProcessMouseEvent(Window* target, MouseEvent* event) {
  if (!target->IsVisible())
    return false;

  EventFilters filters;
  GetEventFiltersToNotify(target->parent(), &filters);
  for (EventFilters::const_reverse_iterator it = filters.rbegin(),
           rend = filters.rend();
       it != rend; ++it) {
    if ((*it)->PreHandleMouseEvent(target, event))
      return true;
  }

  if (!target->delegate())
    return false;
  return target->delegate()->OnMouseEvent(event);
}

bool RootWindow::ProcessKeyEvent(Window* target, KeyEvent* event) {
  EventFilters filters;

  if (!target) {
    // When no window is focused, send the key event to |this| so event filters
    // for the window could check if the key is a global shortcut like Alt+Tab.
    target = this;
    GetEventFiltersToNotify(this, &filters);
  } else {
    if (!target->IsVisible())
      return false;
    GetEventFiltersToNotify(target->parent(), &filters);
  }

  for (EventFilters::const_reverse_iterator it = filters.rbegin(),
           rend = filters.rend();
       it != rend; ++it) {
    if ((*it)->PreHandleKeyEvent(target, event))
      return true;
  }

  if (!target->delegate())
    return false;
  return target->delegate()->OnKeyEvent(event);
}

ui::TouchStatus RootWindow::ProcessTouchEvent(Window* target,
                                              TouchEvent* event) {
  if (!target->IsVisible())
    return ui::TOUCH_STATUS_UNKNOWN;

  EventFilters filters;
  if (target == this)
    GetEventFiltersToNotify(target, &filters);
  else
    GetEventFiltersToNotify(target->parent(), &filters);
  for (EventFilters::const_reverse_iterator it = filters.rbegin(),
           rend = filters.rend();
       it != rend; ++it) {
    ui::TouchStatus status = (*it)->PreHandleTouchEvent(target, event);
    if (status != ui::TOUCH_STATUS_UNKNOWN)
      return status;
  }

  if (target->delegate())
    return target->delegate()->OnTouchEvent(event);

  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus RootWindow::ProcessGestureEvent(Window* target,
                                                  GestureEvent* event) {
  if (!target->IsVisible())
    return ui::GESTURE_STATUS_UNKNOWN;

  EventFilters filters;
  if (target == this)
    GetEventFiltersToNotify(target, &filters);
  else
    GetEventFiltersToNotify(target->parent(), &filters);
  ui::GestureStatus status = ui::GESTURE_STATUS_UNKNOWN;
  for (EventFilters::const_reverse_iterator it = filters.rbegin(),
           rend = filters.rend();
       it != rend; ++it) {
    status = (*it)->PreHandleGestureEvent(target, event);
    if (status != ui::GESTURE_STATUS_UNKNOWN)
      return status;
  }

  if (target->delegate())
    status = target->delegate()->OnGestureEvent(event);
  if (status == ui::GESTURE_STATUS_UNKNOWN) {
    // The gesture was unprocessed. Generate corresponding mouse events here
    // (e.g. tap to click).
    const ui::EventType* types = NULL;
    bool generate_move = false;
    switch (event->type()) {
      case ui::ET_GESTURE_TAP:
      case ui::ET_GESTURE_DOUBLE_TAP:  // Double click is special cased below.
        generate_move = true;
        types = kTapTypes;
        break;

      case ui::ET_GESTURE_SCROLL_BEGIN:
        generate_move = true;
        types = kScrollBeginTypes;
        break;

      case ui::ET_GESTURE_SCROLL_UPDATE:
        types = kScrollUpdateTypes;
        break;

      case ui::ET_GESTURE_SCROLL_END:
        types = kScrollEndTypes;
        break;

      default:
        break;
    }
    if (types) {
      gfx::Point point_in_root(event->location());
      Window::ConvertPointToWindow(target, this, &point_in_root);
      // Move the mouse to the new location.
      if (generate_move && point_in_root != last_mouse_location_) {
        MouseEvent synth(ui::ET_MOUSE_MOVED, point_in_root,
                         event->root_location(), event->flags());
        if (DispatchMouseEventToTarget(&synth, target))
          status = ui::GESTURE_STATUS_SYNTH_MOUSE;
      }
      for (const ui::EventType* type = types; *type != ui::ET_UNKNOWN;
           ++type) {
        int flags = event->flags();
        if (event->type() == ui::ET_GESTURE_DOUBLE_TAP &&
            *type == ui::ET_MOUSE_PRESSED)
          flags |= ui::EF_IS_DOUBLE_CLICK;

        // It is necessary to set this explicitly when using XI2.2. When using
        // XI < 2.2, this is always set anyway.
        flags |= ui::EF_LEFT_MOUSE_BUTTON;

        // DispatchMouseEventToTarget() expects coordinates in root, it'll
        // convert back to |target|.
        MouseEvent synth(*type, point_in_root, event->root_location(), flags);
        if (DispatchMouseEventToTarget(&synth, target))
          status = ui::GESTURE_STATUS_SYNTH_MOUSE;
      }
    }
  }

  return status;
}

bool RootWindow::ProcessGestures(ui::GestureRecognizer::Gestures* gestures) {
  if (!gestures)
    return false;
  bool handled = false;
  for (unsigned int i = 0; i < gestures->size(); i++) {
    GestureEvent* gesture =
        static_cast<GestureEvent*>(gestures->get().at(i));
    if (DispatchGestureEvent(gesture) != ui::GESTURE_STATUS_UNKNOWN)
      handled = true;
  }
  return handled;
}

void RootWindow::OnWindowRemovedFromRootWindow(Window* detached) {
  DCHECK(capture_window_ != this);

  OnWindowHidden(detached, false);

  if (detached->IsVisible() &&
      detached->ContainsPointInRoot(last_mouse_location_)) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowHidden(Window* invisible, bool destroyed) {
  // Update the focused window state if the invisible window contains
  // focused_window.
  if (invisible->Contains(focused_window_)) {
    Window* focus_to = invisible->transient_parent();
    if (focus_to) {
      // Has to be removed from the transient parent before focusing, otherwise
      // |window| will be focused again.
      if (destroyed)
        focus_to->RemoveTransientChild(invisible);
    } else {
      // If the invisible view has no visible transient window, focus to the
      // topmost visible parent window.
      focus_to = invisible->parent();
    }
    if (focus_to &&
        (!focus_to->IsVisible() ||
         (client::GetActivationClient(this) &&
          !client::GetActivationClient(this)->OnWillFocusWindow(focus_to,
                                                                NULL)))) {
      focus_to = NULL;
    }
    SetFocusedWindow(focus_to, NULL);
  }
  // If the ancestor of the capture window is hidden,
  // release the capture.
  if (invisible->Contains(capture_window_) && invisible != this)
    ReleaseCapture(capture_window_);

  // If the ancestor of any event handler windows are invisible, release the
  // pointer to those windows.
  if (invisible->Contains(mouse_pressed_handler_))
    mouse_pressed_handler_ = NULL;
  if (invisible->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = NULL;
  gesture_recognizer_->FlushTouchQueue(invisible);
}

void RootWindow::OnWindowAddedToRootWindow(Window* attached) {
  if (attached->IsVisible() &&
      attached->ContainsPointInRoot(last_mouse_location_))
    PostMouseMoveEventAfterWindowChange();
}

bool RootWindow::CanFocus() const {
  return IsVisible();
}

bool RootWindow::CanReceiveEvents() const {
  return IsVisible();
}

internal::FocusManager* RootWindow::GetFocusManager() {
  return this;
}

bool RootWindow::DispatchLongPressGestureEvent(ui::GestureEvent* event) {
  return DispatchGestureEvent(static_cast<GestureEvent*>(event));
}

bool RootWindow::DispatchCancelTouchEvent(ui::TouchEvent* event) {
  return DispatchTouchEvent(static_cast<TouchEvent*>(event));
}

ui::GestureEvent* RootWindow::CreateGestureEvent(ui::EventType type,
    const gfx::Point& location,
    int flags,
    const base::Time time,
    float param_first,
    float param_second,
    unsigned int touch_id_bitfield) {
  return new GestureEvent(type, location.x(), location.y(), flags, time,
                          param_first, param_second, touch_id_bitfield);
}

ui::TouchEvent* RootWindow::CreateTouchEvent(ui::EventType type,
                                             const gfx::Point& location,
                                             int touch_id,
                                             base::TimeDelta time_stamp) {
  return new TouchEvent(type, location, touch_id, time_stamp);
}

void RootWindow::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* animation) {
  OnHostResized(host_->GetBounds().size());
}

void RootWindow::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* animation) {
}

void RootWindow::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* animation) {
}

void RootWindow::SetFocusedWindow(Window* focused_window,
                                  const aura::Event* event) {
  if (focused_window == focused_window_)
    return;
  if (focused_window && !focused_window->CanFocus())
    return;
  // The NULL-check of |focused_window| is essential here before asking the
  // activation client, since it is valid to clear the focus by calling
  // SetFocusedWindow() to NULL.
  if (focused_window && client::GetActivationClient(this) &&
      !client::GetActivationClient(this)->OnWillFocusWindow(focused_window,
                                                            event)) {
    return;
  }

  Window* old_focused_window = focused_window_;
  focused_window_ = focused_window;
  if (old_focused_window && old_focused_window->delegate())
    old_focused_window->delegate()->OnBlur();
  if (focused_window_ && focused_window_->delegate())
    focused_window_->delegate()->OnFocus();
  if (focused_window_) {
    FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                      OnWindowFocused(focused_window_));
  }
}

Window* RootWindow::GetFocusedWindow() {
  return focused_window_;
}

bool RootWindow::IsFocusedWindow(const Window* window) const {
  return focused_window_ == window;
}

bool RootWindow::DispatchMouseEventImpl(MouseEvent* event) {
  if (ui::IsDIPEnabled()) {
    float scale = ui::GetDeviceScaleFactor(layer());
    ui::Transform transform = layer()->transform();
    transform.ConcatScale(scale, scale);
    event->UpdateForRootTransform(transform);
  } else {
    event->UpdateForRootTransform(layer()->transform());
  }
  Window* target =
      mouse_pressed_handler_ ? mouse_pressed_handler_ : capture_window_;
  if (!target)
    target = GetEventHandlerForPoint(event->location());
  return DispatchMouseEventToTarget(event, target);
}

bool RootWindow::DispatchMouseEventToTarget(MouseEvent* event,
                                            Window* target) {
  static const int kMouseButtonFlagMask =
      ui::EF_LEFT_MOUSE_BUTTON |
      ui::EF_MIDDLE_MOUSE_BUTTON |
      ui::EF_RIGHT_MOUSE_BUTTON;
  last_mouse_location_ = event->location();
  synthesize_mouse_move_ = false;
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      HandleMouseMoved(*event, target);
      break;
    case ui::ET_MOUSE_PRESSED:
      if (!mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      mouse_button_flags_ = event->flags() & kMouseButtonFlagMask;
      Env::GetInstance()->set_mouse_button_flags(mouse_button_flags_);
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      mouse_button_flags_ = event->flags() & kMouseButtonFlagMask;
      Env::GetInstance()->set_mouse_button_flags(mouse_button_flags_);
      break;
    default:
      break;
  }
  if (target && target->delegate()) {
    int flags = event->flags();
    gfx::Point location_in_window = event->location();
    Window::ConvertPointToWindow(this, target, &location_in_window);
    if (IsNonClientLocation(target, location_in_window))
      flags |= ui::EF_IS_NON_CLIENT;
    MouseEvent translated_event(*event, this, target, event->type(), flags);
    return ProcessMouseEvent(target, &translated_event);
  }
  return false;
}

void RootWindow::DispatchHeldMouseMove() {
  if (held_mouse_move_.get()) {
    // If a mouse move has been synthesized, the target location is suspect,
    // so drop the held event.
    if (!synthesize_mouse_move_)
      DispatchMouseEventImpl(held_mouse_move_.get());
    held_mouse_move_.reset();
  }
}

void RootWindow::PostMouseMoveEventAfterWindowChange() {
  if (synthesize_mouse_move_)
    return;
  synthesize_mouse_move_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RootWindow::SynthesizeMouseMoveEvent,
                 event_factory_.GetWeakPtr()));
}

void RootWindow::SynthesizeMouseMoveEvent() {
  if (!synthesize_mouse_move_)
    return;
  synthesize_mouse_move_ = false;
#if !defined(OS_WIN)
  // Temporarily disabled for windows. See crbug.com/112222.
  gfx::Point orig_mouse_location = last_mouse_location_;
  layer()->transform().TransformPoint(orig_mouse_location);

  // TODO(derat|oshima): Don't use mouse_button_flags_ as it's
  // currently broken. See/ crbug.com/107931.
  MouseEvent event(ui::ET_MOUSE_MOVED,
                   orig_mouse_location,
                   orig_mouse_location,
                   ui::EF_IS_SYNTHESIZED);
  DispatchMouseEvent(&event);
#endif
}

void RootWindow::UnlockCompositor() {
  DCHECK(compositor_lock_);
  compositor_lock_ = NULL;
  if (draw_on_compositor_unlock_) {
    draw_on_compositor_unlock_ = false;
    ScheduleDraw();
  }
}

}  // namespace aura

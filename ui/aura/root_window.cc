// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/events/event.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point3.h"
#include "ui/gfx/screen.h"

using std::vector;

namespace aura {

namespace {

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
  if (Env::GetInstance()->event_filter())
    filters->push_back(Env::GetInstance()->event_filter());
}

float GetDeviceScaleFactorFromDisplay(const Window* window) {
  DisplayManager* display_manager = Env::GetInstance()->display_manager();
  return display_manager->GetDisplayNearestWindow(window).device_scale_factor();
}

Window* ConsumerToWindow(ui::GestureConsumer* consumer) {
  return consumer && !consumer->ignores_events() ?
      static_cast<Window*>(consumer) : NULL;
}

void SetLastMouseLocation(const Window* root_window,
                          const gfx::Point& location) {
  Env::GetInstance()->SetLastMouseLocation(*root_window, location);
}

RootWindowHost* CreateHost(RootWindow* root_window,
                           const RootWindow::CreateParams& params) {
  return params.host ? params.host :
      RootWindowHost::Create(root_window, params.initial_bounds);
}

}  // namespace

CompositorLock::CompositorLock(RootWindow* root_window)
    : root_window_(root_window) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CompositorLock::CancelLock, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
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

RootWindow::CreateParams::CreateParams(const gfx::Rect& a_initial_bounds)
    : initial_bounds(a_initial_bounds),
      host(NULL) {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, public:

RootWindow::RootWindow(const CreateParams& params)
    : Window(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(host_(CreateHost(this, params))),
      ALLOW_THIS_IN_INITIALIZER_LIST(schedule_paint_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_factory_(this)),
      mouse_button_flags_(0),
      touch_ids_down_(0),
      last_cursor_(ui::kCursorNull),
      // TODO(ivankr): this currently tracks the default state in
      // RootWindowHostLinux. Other platforms do not implement ShowCursor().
      cursor_shown_(true),
      mouse_pressed_handler_(NULL),
      mouse_moved_handler_(NULL),
      mouse_event_dispatch_target_(NULL),
      event_dispatch_target_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          gesture_recognizer_(ui::GestureRecognizer::Create(this))),
      synthesize_mouse_move_(false),
      waiting_on_compositing_end_(false),
      draw_on_compositing_end_(false),
      defer_draw_scheduling_(false),
      mouse_move_hold_count_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(held_mouse_event_factory_(this)),
      compositor_lock_(NULL),
      draw_on_compositor_unlock_(false) {
  SetName("RootWindow");

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

// static
RootWindow* RootWindow::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  RootWindowHost* host = RootWindowHost::GetForAcceleratedWidget(widget);
  return host ? host->GetRootWindow() : NULL;
}

void RootWindow::Init() {
  compositor()->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(this),
                                host_->GetBounds().size());
  Window::Init(ui::LAYER_NOT_DRAWN);

  gfx::Point point;
  if (host_->QueryMouseLocation(&point))
    SetLastMouseLocation(this, ui::ConvertPointToDIP(layer(), point));

  compositor()->SetRootLayer(layer());
  SetBounds(
      ui::ConvertRectToDIP(layer(), gfx::Rect(host_->GetBounds().size())));
  Show();
}

void RootWindow::ShowRootWindow() {
  host_->Show();
}

void RootWindow::HideRootWindow() {
  host_->Hide();
}

void RootWindow::PrepareForShutdown() {
  host_->PrepareForShutdown();
  // discard synthesize event request as well.
  synthesize_mouse_move_ = false;
}

RootWindowHostDelegate* RootWindow::AsRootWindowHostDelegate() {
  return this;
}

void RootWindow::SetHostSize(const gfx::Size& size_in_pixel) {
  DispatchHeldMouseMove();
  gfx::Rect bounds = host_->GetBounds();
  bounds.set_size(size_in_pixel);
  host_->SetBounds(bounds);

  // Requery the location to constrain it within the new root window size.
  gfx::Point point;
  if (host_->QueryMouseLocation(&point))
    SetLastMouseLocation(this, ui::ConvertPointToDIP(layer(), point));

  synthesize_mouse_move_ = false;
}

gfx::Size RootWindow::GetHostSize() const {
  return host_->GetBounds().size();
}

void RootWindow::SetHostBounds(const gfx::Rect& bounds_in_pixel) {
  DispatchHeldMouseMove();
  host_->SetBounds(bounds_in_pixel);

  // Requery the location to constrain it within the new root window size.
  gfx::Point point;
  if (host_->QueryMouseLocation(&point))
    SetLastMouseLocation(this, ui::ConvertPointToDIP(layer(), point));

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
  // Send entered / exited so that visual state can be updated to match
  // cursor state.
  if (show != cursor_shown_) {
    cursor_shown_ = show;
    host_->ShowCursor(show);
    Env::GetInstance()->SetCursorShown(show);
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::MoveCursorTo(const gfx::Point& location_in_dip) {
  gfx::Point location = location_in_dip;
  layer()->transform().TransformPoint(location);
  host_->MoveCursorTo(ui::ConvertPointToPixel(layer(), location));
  SetLastMouseLocation(this, location_in_dip);
}

bool RootWindow::ConfineCursorToWindow() {
  // We would like to be able to confine the cursor to that window. However,
  // currently, we do not have such functionality in X. So we just confine
  // to the root window. This is ok because this option is currently only
  // being used in fullscreen mode, so root_window bounds = window bounds.
  return host_->ConfineCursorToRootWindow();
}

void RootWindow::Draw() {
  defer_draw_scheduling_ = false;
  if (waiting_on_compositing_end_) {
    draw_on_compositing_end_ = true;
    return;
  }
  if (compositor_lock_) {
    draw_on_compositor_unlock_ = true;
    return;
  }
  waiting_on_compositing_end_ = true;

  TRACE_EVENT_ASYNC_BEGIN0("ui", "RootWindow::Draw",
                           compositor_->last_started_frame() + 1);

  compositor_->Draw(false);
}

void RootWindow::ScheduleFullDraw() {
  compositor_->ScheduleFullDraw();
}

bool RootWindow::DispatchGestureEvent(ui::GestureEvent* event) {
  DispatchHeldMouseMove();

  Window* target = client::GetCaptureWindow(this);
  if (!target) {
    target = ConsumerToWindow(
        gesture_recognizer_->GetTargetForGestureEvent(event));
    if (!target)
      return false;
  }

  if (target) {
    event->ConvertLocationToTarget(static_cast<Window*>(this), target);
    ui::EventResult status = ProcessGestureEvent(target, event);
    return status != ui::ER_UNHANDLED;
  }

  return false;
}

void RootWindow::OnWindowDestroying(Window* window) {
  OnWindowHidden(window, true);

  if (window->IsVisible() &&
      window->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowBoundsChanged(Window* window,
                                       bool contained_mouse_point) {
  if (contained_mouse_point ||
      (window->IsVisible() &&
       window->ContainsPointInRoot(GetLastMouseLocationInRoot()))) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowVisibilityChanged(Window* window, bool is_visible) {
  if (!is_visible)
    OnWindowHidden(window, false);

  if (window->ContainsPointInRoot(GetLastMouseLocationInRoot()))
    PostMouseMoveEventAfterWindowChange();
}

void RootWindow::OnWindowTransformed(Window* window, bool contained_mouse) {
  if (contained_mouse ||
      (window->IsVisible() &&
       window->ContainsPointInRoot(GetLastMouseLocationInRoot()))) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnKeyboardMappingChanged() {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnKeyboardMappingChanged(this));
}

void RootWindow::OnRootWindowHostCloseRequested() {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowHostCloseRequested(this));
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
  // TODO(oshima): Take the root window's transform into account.
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
  *point = point->Scale(ui::GetDeviceScaleFactor(layer()));
}

void RootWindow::ConvertPointFromNativeScreen(gfx::Point* point) const {
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(-location.x(), -location.y());
  *point = point->Scale(1 / ui::GetDeviceScaleFactor(layer()));
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

void RootWindow::ToggleFullScreen() {
  host_->ToggleFullScreen();
}

void RootWindow::HoldMouseMoves() {
  if (!mouse_move_hold_count_)
    held_mouse_event_factory_.InvalidateWeakPtrs();
  ++mouse_move_hold_count_;
}

void RootWindow::ReleaseMouseMoves() {
  --mouse_move_hold_count_;
  DCHECK_GE(mouse_move_hold_count_, 0);
  if (!mouse_move_hold_count_ && held_mouse_move_.get()) {
    // We don't want to call DispatchHeldMouseMove directly, because this might
    // be called from a deep stack while another event, in which case
    // dispatching another one may not be safe/expected.
    // Instead we post a task, that we may cancel if HoldMouseMoves is called
    // again before it executes.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RootWindow::DispatchHeldMouseMove,
                   held_mouse_event_factory_.GetWeakPtr()));
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
  gfx::Rect snapshot_pixels = ui::ConvertRectToPixel(layer(), snapshot_bounds);
  return host_->GrabSnapshot(snapshot_pixels, png_representation);
}

gfx::Point RootWindow::GetLastMouseLocationInRoot() const {
  gfx::Point location = Env::GetInstance()->last_mouse_location();
  client::ScreenPositionClient* client = client::GetScreenPositionClient(this);
  if (client)
    client->ConvertPointFromScreen(this, &location);
  return location;
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

void RootWindow::OnCompositingDidCommit(ui::Compositor*) {
}

void RootWindow::OnCompositingWillStart(ui::Compositor*) {
}

void RootWindow::OnCompositingStarted(ui::Compositor*) {
}

void RootWindow::OnCompositingEnded(ui::Compositor*) {
  TRACE_EVENT_ASYNC_END0("ui", "RootWindow::Draw",
                         compositor_->last_ended_frame());
  waiting_on_compositing_end_ = false;
  if (draw_on_compositing_end_) {
    draw_on_compositing_end_ = false;

    // Call ScheduleDraw() instead of Draw() in order to allow other
    // ui::CompositorObservers to be notified before starting another
    // draw cycle.
    ScheduleDraw();
  }
}

void RootWindow::OnCompositingAborted(ui::Compositor*) {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::LayerDelegate implementation:

void RootWindow::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  const bool cursor_is_in_bounds =
      GetBoundsInScreen().Contains(Env::GetInstance()->last_mouse_location());
  if (cursor_is_in_bounds && cursor_shown_)
    ShowCursor(false);
  host_->OnDeviceScaleFactorChanged(device_scale_factor);
  Window::OnDeviceScaleFactorChanged(device_scale_factor);
  // Update the device scale factor of the cursor client only when the last
  // mouse location is on this root window.
  if (cursor_is_in_bounds) {
    client::CursorClient* cursor_client = client::GetCursorClient(this);
    if (cursor_client)
      cursor_client->SetDeviceScaleFactor(device_scale_factor);
  }
  if (cursor_is_in_bounds && cursor_shown_)
    ShowCursor(true);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, overridden from aura::Window:

bool RootWindow::CanFocus() const {
  return IsVisible();
}

bool RootWindow::CanReceiveEvents() const {
  return IsVisible();
}

FocusManager* RootWindow::GetFocusManager() {
  return focus_manager_;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, overridden from aura::client::CaptureDelegate:

void RootWindow::UpdateCapture(Window* old_capture,
                               Window* new_capture) {
  if (old_capture && old_capture->GetRootWindow() == this &&
      old_capture->delegate()) {
    // Send a capture changed event with bogus location data.
    ui::MouseEvent event(ui::ET_MOUSE_CAPTURE_CHANGED, gfx::Point(),
                         gfx::Point(), 0);

    ProcessMouseEvent(old_capture, &event);

    old_capture->delegate()->OnCaptureLost();
  }

  // Reset the mouse_moved_handler_ if the mouse_moved_handler_ belongs
  // to another root window when losing the capture.
  if (mouse_moved_handler_ && old_capture &&
      old_capture->Contains(mouse_moved_handler_) &&
      old_capture->GetRootWindow() != this) {
    mouse_moved_handler_ = NULL;
  }

  if (new_capture) {
    // Make all subsequent mouse events and touch go to the capture window. We
    // shouldn't need to send an event here as OnCaptureLost should take care of
    // that.
    if (mouse_moved_handler_ || Env::GetInstance()->is_mouse_button_down())
      mouse_moved_handler_ = new_capture;
  } else {
    // Make sure mouse_moved_handler gets updated.
    SynthesizeMouseMoveEvent();
  }
  mouse_pressed_handler_ = NULL;
}

void RootWindow::SetNativeCapture() {
  host_->SetCapture();
}

void RootWindow::ReleaseNativeCapture() {
  host_->ReleaseCapture();
}

gfx::Point RootWindow::QueryMouseLocationForTest() const {
  gfx::Point point;
  host_->QueryMouseLocation(&point);
  return point;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

void RootWindow::HandleMouseMoved(const ui::MouseEvent& event, Window* target) {
  if (target == mouse_moved_handler_)
    return;

  // Send an exited event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    ui::MouseEvent translated_event(event,
                                    static_cast<Window*>(this),
                                    mouse_moved_handler_,
                                    ui::ET_MOUSE_EXITED,
                                    event.flags());
    ProcessMouseEvent(mouse_moved_handler_, &translated_event);
  }

  if (mouse_event_dispatch_target_ != target) {
    mouse_moved_handler_ = NULL;
    return;
  }

  mouse_moved_handler_ = target;
  // Send an entered event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    ui::MouseEvent translated_event(event,
                                    static_cast<Window*>(this),
                                    mouse_moved_handler_,
                                    ui::ET_MOUSE_ENTERED,
                                    event.flags());
    ProcessMouseEvent(mouse_moved_handler_, &translated_event);
  }
}

bool RootWindow::ProcessMouseEvent(Window* target, ui::MouseEvent* event) {
  AutoReset<Window*> reset(&event_dispatch_target_, target);
  if (ProcessEvent(target, event) != ui::ER_UNHANDLED)
    return true;
  return false;
}

bool RootWindow::ProcessKeyEvent(Window* target, ui::KeyEvent* event) {
  if (!target)
    target = this;
  AutoReset<Window*> reset(&event_dispatch_target_, target);
  if (ProcessEvent(target, event) != ui::ER_UNHANDLED)
    return true;
  return false;
}

ui::TouchStatus RootWindow::ProcessTouchEvent(Window* target,
                                              ui::TouchEvent* event) {
  if (ProcessEvent(NULL, event) != ui::ER_UNHANDLED)
    return ui::TOUCH_STATUS_CONTINUE;

  if (!target->IsVisible())
    return ui::TOUCH_STATUS_UNKNOWN;

  EventFilters filters;
  if (target == this)
    GetEventFiltersToNotify(target, &filters);
  else
    GetEventFiltersToNotify(target->parent(), &filters);

  // |target| can be deleted by any of the handlers below.
  WindowTracker tracker;
  tracker.Add(target);

  ui::Event::DispatcherApi dispatcher(event);
  dispatcher.set_target(target);
  for (EventFilters::const_reverse_iterator it = filters.rbegin(),
           rend = filters.rend();
       it != rend; ++it) {
    ui::TouchStatus status = (*it)->PreHandleTouchEvent(target, event);
    if (status != ui::TOUCH_STATUS_UNKNOWN)
      return status;
  }

  if (tracker.Contains(target) && target->delegate()) {
    ui::TouchStatus status = target->delegate()->OnTouchEvent(event);
    if (status != ui::TOUCH_STATUS_UNKNOWN)
      return status;
  }

  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult RootWindow::ProcessGestureEvent(Window* target,
                                                ui::GestureEvent* event) {
  if (!target)
    target = this;
  AutoReset<Window*> reset(&event_dispatch_target_, target);
  return static_cast<ui::EventResult>(ProcessEvent(target, event));
}

bool RootWindow::ProcessGestures(ui::GestureRecognizer::Gestures* gestures) {
  if (!gestures)
    return false;
  bool handled = false;
  for (unsigned int i = 0; i < gestures->size(); i++) {
    ui::GestureEvent* gesture = gestures->get().at(i);
    if (DispatchGestureEvent(gesture) != ui::ER_UNHANDLED)
      handled = true;
  }
  return handled;
}

void RootWindow::OnWindowRemovedFromRootWindow(Window* detached) {
  DCHECK(aura::client::GetCaptureWindow(this) != this);

  OnWindowHidden(detached, false);

  if (detached->IsVisible() &&
      detached->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowHidden(Window* invisible, bool destroyed) {
  // Update the focused window state if the invisible window contains
  // focused_window.
  Window* focused_window = focus_manager_->GetFocusedWindow();
  if (invisible->Contains(focused_window)) {
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
         !focus_to->CanFocus() ||
         (client::GetActivationClient(this) &&
          !client::GetActivationClient(this)->OnWillFocusWindow(focus_to,
                                                                NULL)))) {
      focus_to = NULL;
    }
    GetFocusManager()->SetFocusedWindow(focus_to, NULL);
  }
  Window* capture_window = aura::client::GetCaptureWindow(this);
  // If the ancestor of the capture window is hidden,
  // release the capture.
  if (invisible->Contains(capture_window) && invisible != this)
    capture_window->ReleaseCapture();

  // If the ancestor of any event handler windows are invisible, release the
  // pointer to those windows.
  if (invisible->Contains(mouse_pressed_handler_))
    mouse_pressed_handler_ = NULL;
  if (invisible->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = NULL;
  if (invisible->Contains(mouse_event_dispatch_target_))
    mouse_event_dispatch_target_ = NULL;
  if (invisible->Contains(event_dispatch_target_))
    event_dispatch_target_ = NULL;

  CleanupGestureRecognizerState(invisible);
}

void RootWindow::CleanupGestureRecognizerState(Window* window) {
  gesture_recognizer_->FlushTouchQueue(window);
  Windows windows = window->children();
  for (Windows::const_iterator iter = windows.begin();
      iter != windows.end();
      ++iter) {
    CleanupGestureRecognizerState(*iter);
  }
}

void RootWindow::OnWindowAddedToRootWindow(Window* attached) {
  if (attached->IsVisible() &&
      attached->ContainsPointInRoot(GetLastMouseLocationInRoot()))
    PostMouseMoveEventAfterWindowChange();
}

bool RootWindow::CanDispatchToTarget(ui::EventTarget* target) {
  return event_dispatch_target_ == target;
}

void RootWindow::ProcessPreTargetList(ui::EventHandlerList* list) {
  if (Env::GetInstance()->event_filter())
    list->insert(list->begin(), Env::GetInstance()->event_filter());
}

void RootWindow::ProcessPostTargetList(ui::EventHandlerList* list) {
  // TODO(sad):
}

bool RootWindow::DispatchLongPressGestureEvent(ui::GestureEvent* event) {
  return DispatchGestureEvent(event);
}

bool RootWindow::DispatchCancelTouchEvent(ui::TouchEvent* event) {
  return OnHostTouchEvent(event);
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

////////////////////////////////////////////////////////////////////////////////
// RootWindow, RootWindowHostDelegate implementation:

bool RootWindow::OnHostKeyEvent(ui::KeyEvent* event) {
  DispatchHeldMouseMove();
  if (event->key_code() == ui::VKEY_UNKNOWN)
    return false;
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  Window* focused_window = focus_manager_->GetFocusedWindow();
  if (client && !client->CanProcessEventsWithinSubtree(focused_window)) {
    GetFocusManager()->SetFocusedWindow(NULL, NULL);
    return false;
  }
  return ProcessKeyEvent(focused_window, event);
}

bool RootWindow::OnHostMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_DRAGGED ||
      (event->flags() & ui::EF_IS_SYNTHESIZED)) {
    if (mouse_move_hold_count_) {
      Window* null_window = static_cast<Window*>(NULL);
      held_mouse_move_.reset(
          new ui::MouseEvent(*event, null_window, null_window));
      return true;
    } else {
      // We may have a held event for a period between the time
      // mouse_move_hold_count_ fell to 0 and the DispatchHeldMouseMove
      // executes. Since we're going to dispatch the new event directly below,
      // we can reset the old one.
      held_mouse_move_.reset();
    }
  }
  DispatchHeldMouseMove();
  return DispatchMouseEventImpl(event);
}

bool RootWindow::OnHostScrollEvent(ui::ScrollEvent* event) {
  DispatchHeldMouseMove();
  float scale = ui::GetDeviceScaleFactor(layer());
  ui::Transform transform = layer()->transform();
  transform.ConcatScale(scale, scale);
  event->UpdateForRootTransform(transform);

  SetLastMouseLocation(this, event->location());
  synthesize_mouse_move_ = false;

  Window* target = mouse_pressed_handler_ ?
      mouse_pressed_handler_ : client::GetCaptureWindow(this);
  if (!target)
    target = GetEventHandlerForPoint(event->location());

  if (target && target->delegate()) {
    int flags = event->flags();
    gfx::Point location_in_window = event->location();
    Window::ConvertPointToTarget(this, target, &location_in_window);
    if (IsNonClientLocation(target, location_in_window))
      flags |= ui::EF_IS_NON_CLIENT;
    event->set_flags(flags);
    event->ConvertLocationToTarget(static_cast<Window*>(this), target);
    return ProcessMouseEvent(target, event);
  }
  return false;
}

bool RootWindow::OnHostTouchEvent(ui::TouchEvent* event) {
  DispatchHeldMouseMove();
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      touch_ids_down_ |= (1 << event->touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

    // Don't handle ET_TOUCH_CANCELLED since we always get a ET_TOUCH_RELEASED.
    case ui::ET_TOUCH_RELEASED:
      touch_ids_down_ = (touch_ids_down_ | (1 << event->touch_id())) ^
                        (1 << event->touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

    default:
      break;
  }
  float scale = ui::GetDeviceScaleFactor(layer());
  ui::Transform transform = layer()->transform();
  transform.ConcatScale(scale, scale);
  event->UpdateForRootTransform(transform);
  bool handled = false;
  ui::TouchStatus status = ui::TOUCH_STATUS_UNKNOWN;
  Window* target = client::GetCaptureWindow(this);
  if (!target) {
    target = ConsumerToWindow(
        gesture_recognizer_->GetTouchLockedTarget(event));
    if (!target) {
      target = ConsumerToWindow(
          gesture_recognizer_->GetTargetForLocation(event->location()));
    }
  }

  if (!target && !bounds().Contains(event->location())) {
    // If the initial touch is outside the root window, target the root.
    target = this;
    status = ProcessTouchEvent(target, event);
    CHECK_EQ(ui::TOUCH_STATUS_UNKNOWN, status);
  } else {
    // We only come here when the first contact was within the root window.
    if (!target) {
      target = GetEventHandlerForPoint(event->location());
      if (!target)
        return false;
    }

    ui::TouchEvent translated_event(
        *event, static_cast<Window*>(this), target);
    status = ProcessTouchEvent(target, &translated_event);
    handled = status != ui::TOUCH_STATUS_UNKNOWN;

    if (status == ui::TOUCH_STATUS_QUEUED ||
        status == ui::TOUCH_STATUS_QUEUED_END) {
      gesture_recognizer_->QueueTouchEventForGesture(target, *event);
      return true;
    }
  }

  // Get the list of GestureEvents from GestureRecognizer.
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
      *event, status, target));

  return ProcessGestures(gestures.get()) ? true : handled;
}

void RootWindow::OnHostLostCapture() {
  Window* capture_window = client::GetCaptureWindow(this);
  if (capture_window && capture_window->GetRootWindow() == this)
    capture_window->ReleaseCapture();
}

void RootWindow::OnHostPaint() {
  Draw();
}

void RootWindow::OnHostMoved(const gfx::Point& origin) {
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowMoved(this, origin));
}

void RootWindow::OnHostResized(const gfx::Size& size) {
  DispatchHeldMouseMove();
  // The compositor should have the same size as the native root window host.
  // Get the latest scale from display because it might have been changed.
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(this),
                               size);
  gfx::Size old(bounds().size());
  // The layer, and all the observers should be notified of the
  // transformed size of the root window.
  gfx::Rect bounds(ui::ConvertSizeToDIP(layer(), size));
  layer()->transform().TransformRect(&bounds);
  SetBounds(bounds);
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowResized(this, old));
}

float RootWindow::GetDeviceScaleFactor() {
  return compositor()->device_scale_factor();
}

RootWindow* RootWindow::AsRootWindow() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

bool RootWindow::DispatchMouseEventImpl(ui::MouseEvent* event) {
  float scale = ui::GetDeviceScaleFactor(layer());
  ui::Transform transform = layer()->transform();
  transform.ConcatScale(scale, scale);
  event->UpdateForRootTransform(transform);
  Window* target = mouse_pressed_handler_ ?
      mouse_pressed_handler_ : client::GetCaptureWindow(this);
  if (!target)
    target = GetEventHandlerForPoint(event->location());
  return DispatchMouseEventToTarget(event, target);
}

bool RootWindow::DispatchMouseEventToTarget(ui::MouseEvent* event,
                                            Window* target) {
  static const int kMouseButtonFlagMask =
      ui::EF_LEFT_MOUSE_BUTTON |
      ui::EF_MIDDLE_MOUSE_BUTTON |
      ui::EF_RIGHT_MOUSE_BUTTON;
  AutoReset<Window*> reset(&mouse_event_dispatch_target_, target);
  SetLastMouseLocation(this, event->location());
  synthesize_mouse_move_ = false;
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      mouse_event_dispatch_target_ = target;
      HandleMouseMoved(*event, target);
      if (mouse_event_dispatch_target_ != target)
        return false;
      break;
    case ui::ET_MOUSE_PRESSED:
      if (!mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      mouse_button_flags_ = event->flags() & kMouseButtonFlagMask;
      Env::GetInstance()->set_mouse_button_flags(mouse_button_flags_);
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      mouse_button_flags_ = event->flags() & kMouseButtonFlagMask &
          ~event->changed_button_flags();
      Env::GetInstance()->set_mouse_button_flags(mouse_button_flags_);
      break;
    default:
      break;
  }
  if (target) {
    int flags = event->flags();
    gfx::Point location_in_window = event->location();
    Window::ConvertPointToTarget(this, target, &location_in_window);
    if (IsNonClientLocation(target, location_in_window))
      flags |= ui::EF_IS_NON_CLIENT;
    event->set_flags(flags);
    event->ConvertLocationToTarget(static_cast<Window*>(this), target);
    return ProcessMouseEvent(target, event);
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
  gfx::Point3f point(GetLastMouseLocationInRoot());
  ui::Transform transform = layer()->transform();
  float scale = ui::GetDeviceScaleFactor(layer());
  transform.ConcatScale(scale, scale);
  transform.TransformPoint(point);
  gfx::Point orig_mouse_location = point.AsPoint();

  // TODO(derat|oshima): Don't use mouse_button_flags_ as it's
  // currently broken. See/ crbug.com/107931.
  ui::MouseEvent event(ui::ET_MOUSE_MOVED,
                       orig_mouse_location,
                       orig_mouse_location,
                       ui::EF_IS_SYNTHESIZED);
  event.set_system_location(Env::GetInstance()->last_mouse_location());
  OnHostMouseEvent(&event);
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

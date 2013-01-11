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
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/events/event.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/hit_test.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"

using std::vector;

namespace aura {

namespace {

const char kRootWindowForAcceleratedWidget[] =
    "__AURA_ROOT_WINDOW_ACCELERATED_WIDGET__";

// Returns true if |target| has a non-client (frame) component at |location|,
// in window coordinates.
bool IsNonClientLocation(Window* target, const gfx::Point& location) {
  if (!target->delegate())
    return false;
  int hit_test_code = target->delegate()->GetNonClientComponent(location);
  return hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE;
}

float GetDeviceScaleFactorFromDisplay(Window* window) {
  return gfx::Screen::GetScreenFor(window)->
      GetDisplayNearestWindow(window).device_scale_factor();
}

Window* ConsumerToWindow(ui::GestureConsumer* consumer) {
  return consumer && !consumer->ignores_events() ?
      static_cast<Window*>(consumer) : NULL;
}

void SetLastMouseLocation(const RootWindow* root_window,
                          const gfx::Point& location_in_root) {
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(root_window);
  if (client) {
    gfx::Point location_in_screen = location_in_root;
    client->ConvertPointToScreen(root_window, &location_in_screen);
    Env::GetInstance()->set_last_mouse_location(location_in_screen);
  } else {
    Env::GetInstance()->set_last_mouse_location(location_in_root);
  }
}

RootWindowHost* CreateHost(RootWindow* root_window,
                           const RootWindow::CreateParams& params) {
  RootWindowHost* host = params.host ?
      params.host : RootWindowHost::Create(params.initial_bounds);
  host->SetDelegate(root_window);
  return host;
}

}  // namespace

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
      ALLOW_THIS_IN_INITIALIZER_LIST(held_mouse_event_factory_(this)) {
  SetName("RootWindow");

  compositor_.reset(new ui::Compositor(this, host_->GetAcceleratedWidget()));
  DCHECK(compositor_.get());
  compositor_->AddObserver(this);

  prop_.reset(new ui::ViewProp(host_->GetAcceleratedWidget(),
                               kRootWindowForAcceleratedWidget,
                               this));
}

RootWindow::~RootWindow() {
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
  return reinterpret_cast<RootWindow*>(
      ui::ViewProp::GetValue(widget, kRootWindowForAcceleratedWidget));
}

void RootWindow::Init() {
  compositor()->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(this),
                                host_->GetBounds().size());
  Window::Init(ui::LAYER_NOT_DRAWN);
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

void RootWindow::OnCursorVisibilityChanged(bool show) {
  host_->OnCursorVisibilityChanged(show);
}

void RootWindow::OnMouseEventsEnableStateChanged(bool enabled) {
  // Send entered / exited so that visual state can be updated to match
  // mouse events state.
  PostMouseMoveEventAfterWindowChange();
  // TODO(mazda): Add code to disable mouse events when |enabled| == false.
}

void RootWindow::MoveCursorTo(const gfx::Point& location_in_dip) {
  gfx::Point location = location_in_dip;
  layer()->transform().TransformPoint(location);
  host_->MoveCursorTo(ui::ConvertPointToPixel(layer(), location));
  SetLastMouseLocation(this, location_in_dip);
  client::CursorClient* cursor_client = client::GetCursorClient(this);
  if (cursor_client)
    cursor_client->SetDeviceScaleFactor(GetDeviceScaleFactor());
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
    ProcessEvent(target ? target : this, event);
    return event->handled();
  }

  return false;
}

void RootWindow::OnWindowDestroying(Window* window) {
  OnWindowHidden(window, WINDOW_DESTROYED, NULL);

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
    OnWindowHidden(window, WINDOW_HIDDEN, NULL);

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
  *point = gfx::ToFlooredPoint(
      gfx::ScalePoint(*point, ui::GetDeviceScaleFactor(layer())));
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
}

void RootWindow::ConvertPointFromNativeScreen(gfx::Point* point) const {
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(-location.x(), -location.y());
  *point = gfx::ToFlooredPoint(
      gfx::ScalePoint(*point, 1 / ui::GetDeviceScaleFactor(layer())));
}

void RootWindow::ProcessedTouchEvent(ui::TouchEvent* event,
                                     Window* window,
                                     ui::EventResult result) {
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
      *event, result, window));
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
  TRACE_EVENT_ASYNC_BEGIN0("ui", "RootWindow::HoldMouseMoves", this);
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
  TRACE_EVENT_ASYNC_END0("ui", "RootWindow::HoldMouseMoves", this);
}

void RootWindow::SetFocusWhenShown(bool focused) {
  host_->SetFocusWhenShown(focused);
}

bool RootWindow::CopyAreaToSkCanvas(const gfx::Rect& source_bounds,
                                    const gfx::Point& dest_offset,
                                    SkCanvas* canvas) {
  DCHECK(canvas);
  DCHECK(bounds().Contains(source_bounds));
  gfx::Rect source_pixels = ui::ConvertRectToPixel(layer(), source_bounds);
  return host_->CopyAreaToSkCanvas(source_pixels, dest_offset, canvas);
}

bool RootWindow::GrabSnapshot(const gfx::Rect& snapshot_bounds,
                              std::vector<unsigned char>* png_representation) {
  DCHECK(png_representation);
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

void RootWindow::SetTransform(const gfx::Transform& transform) {
  Window::SetTransform(transform);

  // If the layer is not animating, then we need to update the host size
  // immediately.
  if (!layer()->GetAnimator()->is_animating())
    OnHostResized(host_->GetBounds().size());
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::EventTarget implementation:

ui::EventTarget* RootWindow::GetParentTarget() {
  return client::GetEventClient(this) ?
      client::GetEventClient(this)->GetToplevelEventTarget() :
          Env::GetInstance();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::CompositorDelegate implementation:

void RootWindow::ScheduleDraw() {
  if (!defer_draw_scheduling_) {
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

void RootWindow::OnCompositingLockStateChanged(ui::Compositor*) {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::LayerDelegate implementation:

void RootWindow::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  const bool cursor_is_in_bounds =
      GetBoundsInScreen().Contains(Env::GetInstance()->last_mouse_location());
  bool cursor_visible = false;
  client::CursorClient* cursor_client = client::GetCursorClient(this);
  if (cursor_is_in_bounds && cursor_client) {
    cursor_visible = cursor_client->IsCursorVisible();
    if (cursor_visible)
      cursor_client->HideCursor();
  }
  host_->OnDeviceScaleFactorChanged(device_scale_factor);
  Window::OnDeviceScaleFactorChanged(device_scale_factor);
  // Update the device scale factor of the cursor client only when the last
  // mouse location is on this root window.
  if (cursor_is_in_bounds) {
    if (cursor_client)
      cursor_client->SetDeviceScaleFactor(device_scale_factor);
  }
  if (cursor_is_in_bounds && cursor_client && cursor_visible)
    cursor_client->ShowCursor();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, overridden from aura::Window:

bool RootWindow::CanFocus() const {
  return IsVisible();
}

bool RootWindow::CanReceiveEvents() const {
  return IsVisible();
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

    ProcessEvent(old_capture, &event);

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

bool RootWindow::QueryMouseLocationForTest(gfx::Point* point) const {
  return host_->QueryMouseLocation(point);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

void RootWindow::TransformEventForDeviceScaleFactor(ui::LocatedEvent* event) {
  float scale = ui::GetDeviceScaleFactor(layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= layer()->transform();
  event->UpdateForRootTransform(transform);
}

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
    ProcessEvent(mouse_moved_handler_, &translated_event);
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
    ProcessEvent(mouse_moved_handler_, &translated_event);
  }
}

void RootWindow::ProcessEvent(Window* target, ui::Event* event) {
  Window* old_target = event_dispatch_target_;
  event_dispatch_target_ = target;
  if (DispatchEvent(target, event))
    event_dispatch_target_ = old_target;
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

void RootWindow::OnWindowRemovedFromRootWindow(Window* detached,
                                               RootWindow* new_root) {
  DCHECK(aura::client::GetCaptureWindow(this) != this);

  OnWindowHidden(detached, new_root ? WINDOW_MOVING : WINDOW_HIDDEN, new_root);

  if (detached->IsVisible() &&
      detached->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowHidden(Window* invisible,
                                WindowHiddenReason reason,
                                RootWindow* new_root) {
  // TODO(beng): This should be removed once FocusController is turned on.
  if (client::GetFocusClient(this)) {
    client::GetFocusClient(this)->OnWindowHiddenInRootWindow(
        invisible, this, reason == WINDOW_DESTROYED);
  }

  // Do not clear the capture, and the dispatch targets if the window is moving
  // across root windows, because the target itself is actually still visible
  // and clearing them stops further event processing, which can cause
  // unexpected behaviors. See crbug.com/157583
  if (reason != WINDOW_MOVING) {
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
  }

  CleanupGestureRecognizerState(invisible);
}

void RootWindow::CleanupGestureRecognizerState(Window* window) {
  gesture_recognizer_->CleanupStateForConsumer(window);
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
  Window* focused_window = client::GetFocusClient(this)->GetFocusedWindow();
  if (client && !client->CanProcessEventsWithinSubtree(focused_window)) {
    client::GetFocusClient(this)->FocusWindow(NULL, NULL);
    return false;
  }
  ProcessEvent(focused_window ? focused_window : this, event);
  return event->handled();
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

  TransformEventForDeviceScaleFactor(event);
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
    ProcessEvent(target, event);
    return event->handled();
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

    // Handle ET_TOUCH_CANCELLED only if it has a native event.
    case ui::ET_TOUCH_CANCELLED:
      if (!event->HasNativeEvent())
        break;
      // fallthrough
    case ui::ET_TOUCH_RELEASED:
      touch_ids_down_ = (touch_ids_down_ | (1 << event->touch_id())) ^
                        (1 << event->touch_id());
      Env::GetInstance()->set_touch_down(touch_ids_down_ != 0);
      break;

    default:
      break;
  }
  TransformEventForDeviceScaleFactor(event);
  bool handled = false;
  Window* target = client::GetCaptureWindow(this);
  if (!target) {
    target = ConsumerToWindow(
        gesture_recognizer_->GetTouchLockedTarget(event));
    if (!target) {
      target = ConsumerToWindow(
          gesture_recognizer_->GetTargetForLocation(event->location()));
    }
  }

  ui::EventResult result = ui::ER_UNHANDLED;
  if (!target && !bounds().Contains(event->location())) {
    // If the initial touch is outside the root window, target the root.
    target = this;
    ProcessEvent(target ? target : NULL, event);
    result = event->result();
  } else {
    // We only come here when the first contact was within the root window.
    if (!target) {
      target = GetEventHandlerForPoint(event->location());
      if (!target)
        return false;
    }

    ui::TouchEvent translated_event(
        *event, static_cast<Window*>(this), target);
    ProcessEvent(target ? target : NULL, &translated_event);
    handled = translated_event.handled();
    result = translated_event.result();
  }

  // Get the list of GestureEvents from GestureRecognizer.
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
      *event, result, target));

  return ProcessGestures(gestures.get()) ? true : handled;
}

void RootWindow::OnHostActivated() {
  Env::GetInstance()->RootWindowActivated(this);
}

void RootWindow::OnHostLostWindowCapture() {
  Window* capture_window = client::GetCaptureWindow(this);
  if (capture_window && capture_window->GetRootWindow() == this)
    capture_window->ReleaseCapture();
}

void RootWindow::OnHostLostMouseGrab() {
  mouse_pressed_handler_ = NULL;
  mouse_moved_handler_ = NULL;
  mouse_event_dispatch_target_ = NULL;
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
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(this), size);

  // The layer, and all the observers should be notified of the
  // transformed size of the root window.
  gfx::Size old(bounds().size());
  gfx::RectF bounds(ui::ConvertSizeToDIP(layer(), size));
  layer()->transform().TransformRect(&bounds);
  // The transform is expected to produce an integer rect as its output.
  SetBounds(gfx::ToNearestRect(bounds));
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
  TransformEventForDeviceScaleFactor(event);
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
  base::AutoReset<Window*> reset(&mouse_event_dispatch_target_, target);
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
    event->ConvertLocationToTarget(static_cast<Window*>(this), target);
    if (IsNonClientLocation(target, event->location()))
      event->set_flags(event->flags() | ui::EF_IS_NON_CLIENT);
    ProcessEvent(target, event);
    return event->handled();
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
  gfx::Point3F point(GetLastMouseLocationInRoot());
  float scale = ui::GetDeviceScaleFactor(layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= layer()->transform();
  transform.TransformPoint(point);
  gfx::Point orig_mouse_location = gfx::ToFlooredPoint(point.AsPointF());

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

}  // namespace aura

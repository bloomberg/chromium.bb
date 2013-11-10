// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

using std::vector;

typedef ui::EventDispatchDetails DispatchDetails;

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
  gfx::Display display = gfx::Screen::GetScreenFor(window)->
      GetDisplayNearestWindow(window);
  DCHECK(display.is_valid());
  return display.device_scale_factor();
}

Window* ConsumerToWindow(ui::GestureConsumer* consumer) {
  return consumer ? static_cast<Window*>(consumer) : NULL;
}

void SetLastMouseLocation(const Window* root_window,
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

class SimpleRootWindowTransformer : public RootWindowTransformer {
 public:
  SimpleRootWindowTransformer(const Window* root_window,
                              const gfx::Transform& transform)
      : root_window_(root_window),
        transform_(transform) {
  }

  // RootWindowTransformer overrides:
  virtual gfx::Transform GetTransform() const OVERRIDE {
    return transform_;
  }

  virtual gfx::Transform GetInverseTransform() const OVERRIDE {
    gfx::Transform invert;
    if (!transform_.GetInverse(&invert))
      return transform_;
    return invert;
  }

  virtual gfx::Rect GetRootWindowBounds(
      const gfx::Size& host_size) const OVERRIDE {
    gfx::Rect bounds(host_size);
    gfx::RectF new_bounds(ui::ConvertRectToDIP(root_window_->layer(), bounds));
    transform_.TransformRect(&new_bounds);
    return gfx::Rect(gfx::ToFlooredSize(new_bounds.size()));
  }

  virtual gfx::Insets GetHostInsets() const OVERRIDE {
    return gfx::Insets();
  }

 private:
  virtual ~SimpleRootWindowTransformer() {}

  const Window* root_window_;
  const gfx::Transform transform_;

  DISALLOW_COPY_AND_ASSIGN(SimpleRootWindowTransformer);
};

}  // namespace

RootWindow::CreateParams::CreateParams(const gfx::Rect& a_initial_bounds)
    : initial_bounds(a_initial_bounds),
      host(NULL) {
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, public:

RootWindow::RootWindow(const CreateParams& params)
    : Window(NULL),
      host_(CreateHost(this, params)),
      touch_ids_down_(0),
      last_cursor_(ui::kCursorNull),
      mouse_pressed_handler_(NULL),
      mouse_moved_handler_(NULL),
      event_dispatch_target_(NULL),
      synthesize_mouse_move_(false),
      move_hold_count_(0),
      repost_event_factory_(this),
      held_event_factory_(this) {
  window()->set_dispatcher(this);
  window()->SetName("RootWindow");

  // Some GPUs have trouble drawing windows that are less than 64 pixels wide
  // or tall, so force those windows to use the software renderer.
  // http://crbug.com/286609
  bool use_software_renderer =
      params.initial_bounds.width() < 64 || params.initial_bounds.height() < 64;
  compositor_.reset(
      new ui::Compositor(use_software_renderer, host_->GetAcceleratedWidget()));
  DCHECK(compositor_.get());

  prop_.reset(new ui::ViewProp(host_->GetAcceleratedWidget(),
                               kRootWindowForAcceleratedWidget,
                               this));
  ui::GestureRecognizer::Get()->AddGestureEventHelper(this);
}

RootWindow::~RootWindow() {
  TRACE_EVENT0("shutdown", "RootWindow::Destructor");

  ui::GestureRecognizer::Get()->RemoveGestureEventHelper(this);

  // Make sure to destroy the compositor before terminating so that state is
  // cleared and we don't hit asserts.
  compositor_.reset();

  // An observer may have been added by an animation on the RootWindow.
  window()->layer()->GetAnimator()->RemoveObserver(this);

  // Destroy child windows while we're still valid. This is also done by
  // ~Window, but by that time any calls to virtual methods overriden here (such
  // as GetRootWindow()) result in Window's implementation. By destroying here
  // we ensure GetRootWindow() still returns this.
  RemoveOrDestroyChildren();

  // Destroying/removing child windows may try to access |host_| (eg.
  // GetAcceleratedWidget())
  host_.reset(NULL);

  window()->set_dispatcher(NULL);
}

// static
RootWindow* RootWindow::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return reinterpret_cast<RootWindow*>(
      ui::ViewProp::GetValue(widget, kRootWindowForAcceleratedWidget));
}

void RootWindow::Init() {
  compositor()->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(window()),
                                host_->GetBounds().size());
  Window::Init(ui::LAYER_NOT_DRAWN);
  compositor()->SetRootLayer(window()->layer());
  transformer_.reset(
      new SimpleRootWindowTransformer(window(), gfx::Transform()));
  UpdateRootWindowSize(GetHostSize());
  Env::GetInstance()->NotifyRootWindowInitialized(this);
  window()->Show();
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

void RootWindow::RepostEvent(const ui::LocatedEvent& event) {
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_GESTURE_TAP_DOWN);
  // We allow for only one outstanding repostable event. This is used
  // in exiting context menus.  A dropped repost request is allowed.
  if (event.type() == ui::ET_MOUSE_PRESSED) {
    held_repostable_event_.reset(
        new ui::MouseEvent(
            static_cast<const ui::MouseEvent&>(event),
            static_cast<aura::Window*>(event.target()),
            window()));
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RootWindow::DispatchHeldEventsAsync,
                   repost_event_factory_.GetWeakPtr()));
  } else {
    DCHECK(event.type() == ui::ET_GESTURE_TAP_DOWN);
    held_repostable_event_.reset();
    // TODO(rbyers): Reposing of gestures is tricky to get
    // right, so it's not yet supported.  crbug.com/170987.
  }
}

RootWindowHostDelegate* RootWindow::AsRootWindowHostDelegate() {
  return this;
}

void RootWindow::SetHostSize(const gfx::Size& size_in_pixel) {
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;
  gfx::Rect bounds = host_->GetBounds();
  bounds.set_size(size_in_pixel);
  host_->SetBounds(bounds);

  // Requery the location to constrain it within the new root window size.
  gfx::Point point;
  if (host_->QueryMouseLocation(&point)) {
    SetLastMouseLocation(window(),
                         ui::ConvertPointToDIP(window()->layer(), point));
  }

  synthesize_mouse_move_ = false;
}

gfx::Size RootWindow::GetHostSize() const {
  return host_->GetBounds().size();
}

void RootWindow::SetHostBounds(const gfx::Rect& bounds_in_pixel) {
  DCHECK(!bounds_in_pixel.IsEmpty());
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;
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
  // Clear any existing mouse hover effects when the cursor becomes invisible.
  // Note we do not need to dispatch a mouse enter when the cursor becomes
  // visible because that can only happen in response to a mouse event, which
  // will trigger its own mouse enter.
  if (!show)
    DispatchMouseExitAtPoint(GetLastMouseLocationInRoot());

  host_->OnCursorVisibilityChanged(show);
}

void RootWindow::OnMouseEventsEnableStateChanged(bool enabled) {
  // Send entered / exited so that visual state can be updated to match
  // mouse events state.
  PostMouseMoveEventAfterWindowChange();
  // TODO(mazda): Add code to disable mouse events when |enabled| == false.
}

void RootWindow::MoveCursorTo(const gfx::Point& location_in_dip) {
  gfx::Point host_location(location_in_dip);
  ConvertPointToHost(&host_location);
  MoveCursorToInternal(location_in_dip, host_location);
}

void RootWindow::MoveCursorToHostLocation(const gfx::Point& host_location) {
  gfx::Point root_location(host_location);
  ConvertPointFromHost(&root_location);
  MoveCursorToInternal(root_location, host_location);
}

bool RootWindow::ConfineCursorToWindow() {
  // We would like to be able to confine the cursor to that window. However,
  // currently, we do not have such functionality in X. So we just confine
  // to the root window. This is ok because this option is currently only
  // being used in fullscreen mode, so root_window bounds = window bounds.
  return host_->ConfineCursorToRootWindow();
}

void RootWindow::UnConfineCursor() {
  host_->UnConfineCursor();
}

void RootWindow::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  compositor_->ScheduleRedrawRect(damage_rect);
}

Window* RootWindow::GetGestureTarget(ui::GestureEvent* event) {
  Window* target = client::GetCaptureWindow(window());
  if (!target) {
    target = ConsumerToWindow(
        ui::GestureRecognizer::Get()->GetTargetForGestureEvent(event));
  }

  return target;
}

void RootWindow::DispatchGestureEvent(ui::GestureEvent* event) {
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;

  Window* target = GetGestureTarget(event);
  if (target) {
    event->ConvertLocationToTarget(window(), target);
    DispatchDetails details = ProcessEvent(target, event);
    if (details.dispatcher_destroyed)
      return;
  }
}

void RootWindow::OnWindowDestroying(Window* window) {
  DispatchMouseExitToHidingWindow(window);
  OnWindowHidden(window, WINDOW_DESTROYED);

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

void RootWindow::DispatchMouseExitToHidingWindow(Window* window) {
  // The mouse capture is intentionally ignored. Think that a mouse enters
  // to a window, the window sets the capture, the mouse exits the window,
  // and then it releases the capture. In that case OnMouseExited won't
  // be called. So it is natural not to emit OnMouseExited even though
  // |window| is the capture window.
  gfx::Point last_mouse_location = GetLastMouseLocationInRoot();
  if (window->Contains(mouse_moved_handler_) &&
      window->ContainsPointInRoot(last_mouse_location))
    DispatchMouseExitAtPoint(last_mouse_location);
}

void RootWindow::DispatchMouseExitAtPoint(const gfx::Point& point) {
  ui::MouseEvent event(ui::ET_MOUSE_EXITED, point, point, ui::EF_NONE);
  DispatchDetails details =
      DispatchMouseEnterOrExit(event, ui::ET_MOUSE_EXITED);
  if (details.dispatcher_destroyed)
    return;
}

void RootWindow::OnWindowVisibilityChanged(Window* window, bool is_visible) {
  if (!is_visible)
    OnWindowHidden(window, WINDOW_HIDDEN);

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
  host_->PostNativeEvent(native_event);
}

void RootWindow::ConvertPointToNativeScreen(gfx::Point* point) const {
  ConvertPointToHost(point);
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
}

void RootWindow::ConvertPointFromNativeScreen(gfx::Point* point) const {
  gfx::Point location = host_->GetLocationOnNativeScreen();
  point->Offset(-location.x(), -location.y());
  ConvertPointFromHost(point);
}

void RootWindow::ConvertPointToHost(gfx::Point* point) const {
  gfx::Point3F point_3f(*point);
  GetRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void RootWindow::ConvertPointFromHost(gfx::Point* point) const {
  gfx::Point3F point_3f(*point);
  GetInverseRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void RootWindow::ProcessedTouchEvent(ui::TouchEvent* event,
                                     Window* window,
                                     ui::EventResult result) {
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(ui::GestureRecognizer::Get()->
      ProcessTouchEventForGesture(*event, result, window));
  DispatchDetails details = ProcessGestures(gestures.get());
  if (details.dispatcher_destroyed)
    return;
}

gfx::AcceleratedWidget RootWindow::GetAcceleratedWidget() {
  return host_->GetAcceleratedWidget();
}

void RootWindow::ToggleFullScreen() {
  host_->ToggleFullScreen();
}

void RootWindow::HoldPointerMoves() {
  if (!move_hold_count_)
    held_event_factory_.InvalidateWeakPtrs();
  ++move_hold_count_;
  TRACE_EVENT_ASYNC_BEGIN0("ui", "RootWindow::HoldPointerMoves", this);
}

void RootWindow::ReleasePointerMoves() {
  --move_hold_count_;
  DCHECK_GE(move_hold_count_, 0);
  if (!move_hold_count_ && held_move_event_) {
    // We don't want to call DispatchHeldEvents directly, because this might be
    // called from a deep stack while another event, in which case dispatching
    // another one may not be safe/expected.  Instead we post a task, that we
    // may cancel if HoldPointerMoves is called again before it executes.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RootWindow::DispatchHeldEventsAsync,
                   held_event_factory_.GetWeakPtr()));
  }
  TRACE_EVENT_ASYNC_END0("ui", "RootWindow::HoldPointerMoves", this);
}

void RootWindow::SetFocusWhenShown(bool focused) {
  host_->SetFocusWhenShown(focused);
}

gfx::Point RootWindow::GetLastMouseLocationInRoot() const {
  gfx::Point location = Env::GetInstance()->last_mouse_location();
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(window());
  if (client)
    client->ConvertPointFromScreen(window(), &location);
  return location;
}

bool RootWindow::QueryMouseLocationForTest(gfx::Point* point) const {
  return host_->QueryMouseLocation(point);
}

void RootWindow::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_ = transformer.Pass();
  host_->SetInsets(transformer_->GetHostInsets());
  Window::SetTransform(transformer_->GetTransform());
  // If the layer is not animating, then we need to update the root window
  // size immediately.
  if (!window()->layer()->GetAnimator()->is_animating())
    UpdateRootWindowSize(GetHostSize());
}

gfx::Transform RootWindow::GetRootTransform() const {
  float scale = ui::GetDeviceScaleFactor(window()->layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= transformer_->GetTransform();
  return transform;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, Window overrides:

Window* RootWindow::GetRootWindow() {
  return this;
}

const Window* RootWindow::GetRootWindow() const {
  return this;
}

void RootWindow::SetTransform(const gfx::Transform& transform) {
  scoped_ptr<RootWindowTransformer> transformer(
      new SimpleRootWindowTransformer(window(), transform));
  SetRootWindowTransformer(transformer.Pass());
}

bool RootWindow::CanFocus() const {
  return IsVisible();
}

bool RootWindow::CanReceiveEvents() const {
  return IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

void RootWindow::TransformEventForDeviceScaleFactor(ui::LocatedEvent* event) {
  event->UpdateForRootTransform(GetInverseRootTransform());
}

void RootWindow::MoveCursorToInternal(const gfx::Point& root_location,
                                      const gfx::Point& host_location) {
  host_->MoveCursorTo(host_location);
  SetLastMouseLocation(window(), root_location);
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client) {
    const gfx::Display& display =
        gfx::Screen::GetScreenFor(this)->GetDisplayNearestWindow(window());
    cursor_client->SetDisplay(display);
  }
  synthesize_mouse_move_ = false;
}

ui::EventDispatchDetails RootWindow::DispatchMouseEnterOrExit(
    const ui::MouseEvent& event,
    ui::EventType type) {
  if (!mouse_moved_handler_ || !mouse_moved_handler_->delegate())
    return DispatchDetails();

  ui::MouseEvent translated_event(event,
                                  window(),
                                  mouse_moved_handler_,
                                  type,
                                  event.flags() | ui::EF_IS_SYNTHESIZED);
  return ProcessEvent(mouse_moved_handler_, &translated_event);
}

ui::EventDispatchDetails RootWindow::ProcessEvent(Window* target,
                                                  ui::Event* event) {
  Window* old_target = event_dispatch_target_;
  event_dispatch_target_ = target;
  DispatchDetails details = DispatchEvent(target, event);
  if (!details.dispatcher_destroyed) {
    if (event_dispatch_target_ != target)
      details.target_destroyed = true;
    event_dispatch_target_ = old_target;
  }
  return details;
}

ui::EventDispatchDetails RootWindow::ProcessGestures(
    ui::GestureRecognizer::Gestures* gestures) {
  DispatchDetails details;
  if (!gestures || gestures->empty())
    return details;

  Window* target = GetGestureTarget(gestures->get().at(0));
  for (size_t i = 0; i < gestures->size(); ++i) {
    ui::GestureEvent* event = gestures->get().at(i);
    event->ConvertLocationToTarget(window(), target);
    details = ProcessEvent(target, event);
    if (details.dispatcher_destroyed || details.target_destroyed)
      break;
  }
  return details;
}

void RootWindow::OnWindowAddedToRootWindow(Window* attached) {
  if (attached->IsVisible() &&
      attached->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowRemovedFromRootWindow(Window* detached,
                                               Window* new_root) {
  DCHECK(aura::client::GetCaptureWindow(window()) != window());

  DispatchMouseExitToHidingWindow(detached);
  OnWindowHidden(detached, new_root ? WINDOW_MOVING : WINDOW_HIDDEN);

  if (detached->IsVisible() &&
      detached->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostMouseMoveEventAfterWindowChange();
  }
}

void RootWindow::OnWindowHidden(Window* invisible, WindowHiddenReason reason) {
  // Do not clear the capture, and the |event_dispatch_target_| if the
  // window is moving across root windows, because the target itself
  // is actually still visible and clearing them stops further event
  // processing, which can cause unexpected behaviors. See
  // crbug.com/157583
  if (reason != WINDOW_MOVING) {
    Window* capture_window = aura::client::GetCaptureWindow(window());
    // If the ancestor of the capture window is hidden,
    // release the capture.
    if (invisible->Contains(capture_window) && invisible != window())
      capture_window->ReleaseCapture();

    if (invisible->Contains(event_dispatch_target_))
      event_dispatch_target_ = NULL;
  }

  // If the ancestor of any event handler windows are invisible, release the
  // pointer to those windows.
  if (invisible->Contains(mouse_pressed_handler_))
    mouse_pressed_handler_ = NULL;
  if (invisible->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = NULL;

  CleanupGestureRecognizerState(invisible);
}

void RootWindow::CleanupGestureRecognizerState(Window* window) {
  ui::GestureRecognizer::Get()->CleanupStateForConsumer(window);
  const Window::Windows& windows = window->children();
  for (Window::Windows::const_iterator iter = windows.begin();
      iter != windows.end();
      ++iter) {
    CleanupGestureRecognizerState(*iter);
  }
}

void RootWindow::UpdateRootWindowSize(const gfx::Size& host_size) {
  window()->SetBounds(transformer_->GetRootWindowBounds(host_size));
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::EventTarget implementation:

ui::EventTarget* RootWindow::GetParentTarget() {
  return client::GetEventClient(this) ?
      client::GetEventClient(this)->GetToplevelEventTarget() :
          Env::GetInstance();
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
    if (cursor_client) {
      const gfx::Display& display =
          gfx::Screen::GetScreenFor(this)->GetDisplayNearestWindow(this);
      cursor_client->SetDisplay(display);
    }
  }
  if (cursor_is_in_bounds && cursor_client && cursor_visible)
    cursor_client->ShowCursor();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, aura::client::CaptureDelegate implementation:

void RootWindow::UpdateCapture(Window* old_capture,
                               Window* new_capture) {
  // |mouse_moved_handler_| may have been set to a Window in a different root
  // (see below). Clear it here to ensure we don't end up referencing a stale
  // Window.
  if (mouse_moved_handler_ && !window()->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = NULL;

  if (old_capture && old_capture->GetRootWindow() == window() &&
      old_capture->delegate()) {
    // Send a capture changed event with bogus location data.
    ui::MouseEvent event(ui::ET_MOUSE_CAPTURE_CHANGED, gfx::Point(),
                         gfx::Point(), 0);

    DispatchDetails details = ProcessEvent(old_capture, &event);
    if (details.dispatcher_destroyed)
      return;

    old_capture->delegate()->OnCaptureLost();
  }

  if (new_capture) {
    // Make all subsequent mouse events go to the capture window. We shouldn't
    // need to send an event here as OnCaptureLost() should take care of that.
    if (mouse_moved_handler_ || Env::GetInstance()->IsMouseButtonDown())
      mouse_moved_handler_ = new_capture;
  } else {
    // Make sure mouse_moved_handler gets updated.
    DispatchDetails details = SynthesizeMouseMoveEvent();
    if (details.dispatcher_destroyed)
      return;
  }
  mouse_pressed_handler_ = NULL;
}

void RootWindow::OnOtherRootGotCapture() {
  mouse_moved_handler_ = NULL;
  mouse_pressed_handler_ = NULL;
}

void RootWindow::SetNativeCapture() {
  host_->SetCapture();
}

void RootWindow::ReleaseNativeCapture() {
  host_->ReleaseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::EventDispatcherDelegate implementation:

bool RootWindow::CanDispatchToTarget(ui::EventTarget* target) {
  return event_dispatch_target_ == target;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::GestureEventHelper implementation:

bool RootWindow::CanDispatchToConsumer(ui::GestureConsumer* consumer) {
  Window* consumer_window = ConsumerToWindow(consumer);;
  return (consumer_window && consumer_window->GetRootWindow() == window());
}

void RootWindow::DispatchPostponedGestureEvent(ui::GestureEvent* event) {
  DispatchGestureEvent(event);
}

void RootWindow::DispatchCancelTouchEvent(ui::TouchEvent* event) {
  OnHostTouchEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, ui::LayerAnimationObserver implementation:

void RootWindow::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* animation) {
  UpdateRootWindowSize(GetHostSize());
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
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return false;
  if (event->key_code() == ui::VKEY_UNKNOWN)
    return false;
  client::EventClient* client = client::GetEventClient(window());
  Window* focused_window = client::GetFocusClient(window())->GetFocusedWindow();
  if (client && !client->CanProcessEventsWithinSubtree(focused_window)) {
    client::GetFocusClient(window())->FocusWindow(NULL);
    return false;
  }
  details = ProcessEvent(focused_window ? focused_window : window(), event);
  if (details.dispatcher_destroyed)
    return true;
  return event->handled();
}

bool RootWindow::OnHostMouseEvent(ui::MouseEvent* event) {
  DispatchDetails details = OnHostMouseEventImpl(event);
  return event->handled() || details.dispatcher_destroyed;
}

bool RootWindow::OnHostScrollEvent(ui::ScrollEvent* event) {
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return false;

  TransformEventForDeviceScaleFactor(event);
  SetLastMouseLocation(window(), event->location());
  synthesize_mouse_move_ = false;

  Window* target = mouse_pressed_handler_ ?
      mouse_pressed_handler_ : client::GetCaptureWindow(window());

  if (!target)
    target = window()->GetEventHandlerForPoint(event->location());

  if (!target)
    target = window();

  event->ConvertLocationToTarget(window(), target);
  int flags = event->flags();
  if (IsNonClientLocation(target, event->location()))
    flags |= ui::EF_IS_NON_CLIENT;
  event->set_flags(flags);

  details = ProcessEvent(target, event);
  if (details.dispatcher_destroyed)
    return true;
  return event->handled();
}

bool RootWindow::OnHostTouchEvent(ui::TouchEvent* event) {
  if ((event->type() == ui::ET_TOUCH_MOVED)) {
    if (move_hold_count_) {
      Window* null_window = static_cast<Window*>(NULL);
      held_move_event_.reset(
          new ui::TouchEvent(*event, null_window, null_window));
      return true;
    } else {
      // We may have a held event for a period between the time move_hold_count_
      // fell to 0 and the DispatchHeldEvents executes. Since we're going to
      // dispatch the new event directly below, we can reset the old one.
      held_move_event_.reset();
    }
  }
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return false;
  details = DispatchTouchEventImpl(event);
  if (details.dispatcher_destroyed)
    return true;
  return event->handled();
}

void RootWindow::OnHostCancelMode() {
  ui::CancelModeEvent event;
  Window* focused_window = client::GetFocusClient(window())->GetFocusedWindow();
  DispatchDetails details =
      ProcessEvent(focused_window ? focused_window : window(), &event);
  if (details.dispatcher_destroyed)
    return;
}

void RootWindow::OnHostActivated() {
  Env::GetInstance()->RootWindowActivated(this);
}

void RootWindow::OnHostLostWindowCapture() {
  Window* capture_window = client::GetCaptureWindow(window());
  if (capture_window && capture_window->GetRootWindow() == window())
    capture_window->ReleaseCapture();
}

void RootWindow::OnHostLostMouseGrab() {
  mouse_pressed_handler_ = NULL;
  mouse_moved_handler_ = NULL;
}

void RootWindow::OnHostPaint(const gfx::Rect& damage_rect) {
  compositor_->ScheduleRedrawRect(damage_rect);
}

void RootWindow::OnHostMoved(const gfx::Point& origin) {
  TRACE_EVENT1("ui", "RootWindow::OnHostMoved",
               "origin", origin.ToString());

  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowHostMoved(this, origin));
}

void RootWindow::OnHostResized(const gfx::Size& size) {
  TRACE_EVENT1("ui", "RootWindow::OnHostResized",
               "size", size.ToString());

  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;
  // The compositor should have the same size as the native root window host.
  // Get the latest scale from display because it might have been changed.
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(window()), size);

  // The layer, and the observers should be notified of the
  // transformed size of the root window.
  UpdateRootWindowSize(size);
  FOR_EACH_OBSERVER(RootWindowObserver, observers_,
                    OnRootWindowHostResized(this));
}

float RootWindow::GetDeviceScaleFactor() {
  return compositor()->device_scale_factor();
}

RootWindow* RootWindow::AsRootWindow() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindow, private:

ui::EventDispatchDetails RootWindow::OnHostMouseEventImpl(
    ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_DRAGGED ||
      (event->flags() & ui::EF_IS_SYNTHESIZED)) {
    if (move_hold_count_) {
      Window* null_window = static_cast<Window*>(NULL);
      held_move_event_.reset(
          new ui::MouseEvent(*event, null_window, null_window));
      event->SetHandled();
      return DispatchDetails();
    } else {
      // We may have a held event for a period between the time move_hold_count_
      // fell to 0 and the DispatchHeldEvents executes. Since we're going to
      // dispatch the new event directly below, we can reset the old one.
      held_move_event_.reset();
    }
  }
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return details;
  return DispatchMouseEventImpl(event);
}

ui::EventDispatchDetails RootWindow::DispatchMouseEventImpl(
    ui::MouseEvent* event) {
  TransformEventForDeviceScaleFactor(event);
  Window* target = mouse_pressed_handler_ ?
      mouse_pressed_handler_ : client::GetCaptureWindow(window());
  if (!target)
    target = window()->GetEventHandlerForPoint(event->location());
  return DispatchMouseEventToTarget(event, target);
}

ui::EventDispatchDetails RootWindow::DispatchMouseEventRepost(
    ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return DispatchDetails();
  Window* target = client::GetCaptureWindow(window());
  WindowEventDispatcher* dispatcher = this;
  if (!target) {
    target = window()->GetEventHandlerForPoint(event->location());
  } else {
    dispatcher = target->GetDispatcher();
    CHECK(dispatcher);  // Capture window better be in valid root.
  }
  dispatcher->mouse_pressed_handler_ = NULL;
  return dispatcher->DispatchMouseEventToTarget(event, target);
}

ui::EventDispatchDetails RootWindow::DispatchMouseEventToTarget(
    ui::MouseEvent* event,
    Window* target) {
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client &&
      !cursor_client->IsMouseEventsEnabled() &&
      (event->flags() & ui::EF_IS_SYNTHESIZED))
    return DispatchDetails();

  static const int kMouseButtonFlagMask =
      ui::EF_LEFT_MOUSE_BUTTON |
      ui::EF_MIDDLE_MOUSE_BUTTON |
      ui::EF_RIGHT_MOUSE_BUTTON;
  // WARNING: because of nested message loops |this| may be deleted after
  // dispatching any event. Do not use AutoReset or the like here.
  SetLastMouseLocation(window(), event->location());
  synthesize_mouse_move_ = false;
  switch (event->type()) {
    case ui::ET_MOUSE_EXITED:
      if (!target) {
        DispatchDetails details =
            DispatchMouseEnterOrExit(*event, ui::ET_MOUSE_EXITED);
        if (details.dispatcher_destroyed)
          return details;
        mouse_moved_handler_ = NULL;
      }
      break;
    case ui::ET_MOUSE_MOVED:
      // Send an exit to the current |mouse_moved_handler_| and an enter to
      // |target|. Take care that both us and |target| aren't destroyed during
      // dispatch.
      if (target != mouse_moved_handler_) {
        aura::Window* old_mouse_moved_handler = mouse_moved_handler_;
        WindowTracker destroyed_tracker;
        if (target)
          destroyed_tracker.Add(target);
        DispatchDetails details =
            DispatchMouseEnterOrExit(*event, ui::ET_MOUSE_EXITED);
        if (details.dispatcher_destroyed)
          return details;
        // If the |mouse_moved_handler_| changes out from under us, assume a
        // nested message loop ran and we don't need to do anything.
        if (mouse_moved_handler_ != old_mouse_moved_handler)
          return DispatchDetails();
        if (destroyed_tracker.Contains(target)) {
          destroyed_tracker.Remove(target);
          mouse_moved_handler_ = target;
          DispatchDetails details =
              DispatchMouseEnterOrExit(*event, ui::ET_MOUSE_ENTERED);
          if (details.dispatcher_destroyed)
            return details;
        } else {
          mouse_moved_handler_ = NULL;
          return DispatchDetails();
        }
      }
      break;
    case ui::ET_MOUSE_PRESSED:
      // Don't set the mouse pressed handler for non client mouse down events.
      // These are only sent by Windows and are not always followed with non
      // client mouse up events which causes subsequent mouse events to be
      // sent to the wrong target.
      if (!(event->flags() & ui::EF_IS_NON_CLIENT) && !mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      Env::GetInstance()->set_mouse_button_flags(
          event->flags() & kMouseButtonFlagMask);
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      Env::GetInstance()->set_mouse_button_flags(event->flags() &
          kMouseButtonFlagMask & ~event->changed_button_flags());
      break;
    default:
      break;
  }
  if (target) {
    event->ConvertLocationToTarget(window(), target);
    if (IsNonClientLocation(target, event->location()))
      event->set_flags(event->flags() | ui::EF_IS_NON_CLIENT);
    return ProcessEvent(target, event);
  }
  return DispatchDetails();
}

ui::EventDispatchDetails RootWindow::DispatchTouchEventImpl(
    ui::TouchEvent* event) {
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
  Window* target = client::GetCaptureWindow(window());
  if (!target) {
    target = ConsumerToWindow(
        ui::GestureRecognizer::Get()->GetTouchLockedTarget(event));
    if (!target) {
      target = ConsumerToWindow(ui::GestureRecognizer::Get()->
          GetTargetForLocation(event->location()));
    }
  }

  // The gesture recognizer processes touch events in the system coordinates. So
  // keep a copy of the touch event here before possibly converting the event to
  // a window's local coordinate system.
  ui::TouchEvent event_for_gr(*event);

  ui::EventResult result = ui::ER_UNHANDLED;
  if (!target && !window()->bounds().Contains(event->location())) {
    // If the initial touch is outside the root window, target the root.
    target = window();
    DispatchDetails details = ProcessEvent(target ? target : NULL, event);
    if (details.dispatcher_destroyed)
      return details;
    result = event->result();
  } else {
    // We only come here when the first contact was within the root window.
    if (!target) {
      target = window()->GetEventHandlerForPoint(event->location());
      if (!target)
        return DispatchDetails();
    }

    event->ConvertLocationToTarget(window(), target);
    DispatchDetails details = ProcessEvent(target, event);
    if (details.dispatcher_destroyed)
      return details;
    result = event->result();
  }

  // Get the list of GestureEvents from GestureRecognizer.
  scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
  gestures.reset(ui::GestureRecognizer::Get()->
      ProcessTouchEventForGesture(event_for_gr, result, target));

  return ProcessGestures(gestures.get());
}

ui::EventDispatchDetails RootWindow::DispatchHeldEvents() {
  DispatchDetails dispatch_details;
  if (held_repostable_event_) {
    if (held_repostable_event_->type() == ui::ET_MOUSE_PRESSED) {
      scoped_ptr<ui::MouseEvent> mouse_event(
          static_cast<ui::MouseEvent*>(held_repostable_event_.release()));
      dispatch_details = DispatchMouseEventRepost(mouse_event.get());
    } else {
      // TODO(rbyers): GESTURE_TAP_DOWN not yet supported: crbug.com/170987.
      NOTREACHED();
    }
    if (dispatch_details.dispatcher_destroyed)
      return dispatch_details;
  }

  if (held_move_event_ && held_move_event_->IsMouseEvent()) {
    // If a mouse move has been synthesized, the target location is suspect,
    // so drop the held event.
    if (!synthesize_mouse_move_)
      dispatch_details = DispatchMouseEventImpl(
          static_cast<ui::MouseEvent*>(held_move_event_.get()));
    if (!dispatch_details.dispatcher_destroyed)
      held_move_event_.reset();
  } else if (held_move_event_ && held_move_event_->IsTouchEvent()) {
    dispatch_details = DispatchTouchEventImpl(
        static_cast<ui::TouchEvent*>(held_move_event_.get()));
    if (!dispatch_details.dispatcher_destroyed)
      held_move_event_.reset();
  }

  return dispatch_details;
}

void RootWindow::DispatchHeldEventsAsync() {
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;
}

void RootWindow::PostMouseMoveEventAfterWindowChange() {
  if (synthesize_mouse_move_)
    return;
  synthesize_mouse_move_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RootWindow::SynthesizeMouseMoveEventAsync,
                 held_event_factory_.GetWeakPtr()));
}

ui::EventDispatchDetails RootWindow::SynthesizeMouseMoveEvent() {
  DispatchDetails details;
  if (!synthesize_mouse_move_)
    return details;
  synthesize_mouse_move_ = false;
  gfx::Point root_mouse_location = GetLastMouseLocationInRoot();
  if (!window()->bounds().Contains(root_mouse_location))
    return details;
  gfx::Point host_mouse_location = root_mouse_location;
  ConvertPointToHost(&host_mouse_location);

  ui::MouseEvent event(ui::ET_MOUSE_MOVED,
                       host_mouse_location,
                       host_mouse_location,
                       ui::EF_IS_SYNTHESIZED);
  return OnHostMouseEventImpl(&event);
}

void RootWindow::SynthesizeMouseMoveEventAsync() {
  DispatchDetails details = SynthesizeMouseMoveEvent();
  if (details.dispatcher_destroyed)
    return;
}

gfx::Transform RootWindow::GetInverseRootTransform() const {
  float scale = ui::GetDeviceScaleFactor(window()->layer());
  gfx::Transform transform;
  transform.Scale(1.0f / scale, 1.0f / scale);
  return transformer_->GetInverseTransform() * transform;
}

}  // namespace aura

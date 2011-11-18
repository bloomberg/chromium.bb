// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/desktop_host.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/screen_rotation.h"
#include "ui/gfx/interpolated_transform.h"

using std::string;
using std::vector;

namespace aura {

namespace {

// Default bounds for the host window.
static const int kDefaultHostWindowX = 200;
static const int kDefaultHostWindowY = 200;
static const int kDefaultHostWindowWidth = 1280;
static const int kDefaultHostWindowHeight = 1024;

// Returns true if |target| has a non-client (frame) component at |location|,
// in window coordinates.
bool IsNonClientLocation(Window* target, const gfx::Point& location) {
  if (!target->delegate())
    return false;
  int hit_test_code = target->delegate()->GetNonClientComponent(location);
  return hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE;
}

class DefaultStackingClient : public StackingClient {
 public:
  explicit DefaultStackingClient(Desktop* desktop) : desktop_(desktop) {}
  virtual ~DefaultStackingClient() {}

 private:

  // Overridden from StackingClient:
  virtual void AddChildToDefaultParent(Window* window) OVERRIDE {
    desktop_->AddChild(window);
  }
  virtual bool CanActivateWindow(Window* window) const OVERRIDE {
    return window->parent() == desktop_;
  }
  virtual Window* GetTopmostWindowToActivate(Window* ignore) const OVERRIDE {
    Window::Windows::const_reverse_iterator i;
    for (i = desktop_->children().rbegin();
         i != desktop_->children().rend();
         ++i) {
      if (*i == ignore)
        continue;
      return *i;
    }
    return NULL;
  }

  Desktop* desktop_;

  DISALLOW_COPY_AND_ASSIGN(DefaultStackingClient);
};

typedef std::vector<EventFilter*> EventFilters;

void GetEventFiltersToNotify(Window* target, EventFilters* filters) {
  Window* window = target->parent();
  while (window) {
    if (window->event_filter())
      filters->push_back(window->event_filter());
    window = window->parent();
  }
}

#if !defined(NDEBUG)
bool MaybeFullScreen(DesktopHost* host, KeyEvent* event) {
  if (event->key_code() == ui::VKEY_F11) {
    host->ToggleFullScreen();
    return true;
  }
  return false;
}

bool MaybeRotate(Desktop* desktop, KeyEvent* event) {
  if ((event->flags() & ui::EF_CONTROL_DOWN) &&
      event->key_code() == ui::VKEY_HOME) {
    static int i = 0;
    int delta = 0;
    switch (i) {
      case 0: delta = 90; break;
      case 1: delta = 90; break;
      case 2: delta = 90; break;
      case 3: delta = 90; break;
      case 4: delta = -90; break;
      case 5: delta = -90; break;
      case 6: delta = -90; break;
      case 7: delta = -90; break;
      case 8: delta = -90; break;
      case 9: delta = 180; break;
      case 10: delta = 180; break;
      case 11: delta = 90; break;
      case 12: delta = 180; break;
      case 13: delta = 180; break;
    }
    i = (i + 1) % 14;
    desktop->layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    scoped_ptr<ui::LayerAnimationSequence> screen_rotation(
        new ui::LayerAnimationSequence(new ui::ScreenRotation(delta)));
    screen_rotation->AddObserver(desktop);
    desktop->layer()->GetAnimator()->ScheduleAnimation(
        screen_rotation.release());
    return true;
  }
  return false;
}
#endif

}  // namespace

Desktop* Desktop::instance_ = NULL;
bool Desktop::use_fullscreen_host_window_ = false;

Desktop::Desktop()
    : Window(NULL),
      host_(aura::DesktopHost::Create(GetInitialHostWindowBounds())),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          stacking_client_(new DefaultStackingClient(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(schedule_paint_factory_(this)),
      active_window_(NULL),
      mouse_button_flags_(0),
      last_cursor_(kCursorNull),
      in_destructor_(false),
      screen_(new ScreenAura),
      capture_window_(NULL),
      mouse_pressed_handler_(NULL),
      mouse_moved_handler_(NULL),
      focused_window_(NULL),
      touch_event_handler_(NULL) {
  set_name("RootWindow");
  gfx::Screen::SetInstance(screen_);
  host_->SetDesktop(this);
  last_mouse_location_ = host_->QueryMouseLocation();

  if (ui::Compositor::compositor_factory()) {
    compositor_ = (*ui::Compositor::compositor_factory())(this);
  } else {
    compositor_ = ui::Compositor::Create(this, host_->GetAcceleratedWidget(),
                                         host_->GetSize());
  }
  DCHECK(compositor_.get());
}

Desktop::~Desktop() {
  in_destructor_ = true;
  if (instance_ == this)
    instance_ = NULL;
}

// static
Desktop* Desktop::GetInstance() {
  if (!instance_) {
    instance_ = new Desktop;
    instance_->Init();
  }
  return instance_;
}

// static
void Desktop::DeleteInstanceForTesting() {
  delete instance_;
  instance_ = NULL;
}

void Desktop::SetStackingClient(StackingClient* stacking_client) {
  stacking_client_.reset(stacking_client);
}

void Desktop::ShowDesktop() {
  host_->Show();
}

void Desktop::SetHostSize(const gfx::Size& size) {
  host_->SetSize(size);
  // Requery the location to constrain it within the new desktop size.
  last_mouse_location_ = host_->QueryMouseLocation();
}

gfx::Size Desktop::GetHostSize() const {
  gfx::Rect rect(host_->GetSize());
  layer()->transform().TransformRect(&rect);
  return rect.size();
}

void Desktop::SetCursor(gfx::NativeCursor cursor) {
  last_cursor_ = cursor;
  // A lot of code seems to depend on NULL cursors actually showing an arrow,
  // so just pass everything along to the host.
  host_->SetCursor(cursor);
}

void Desktop::Run() {
  ShowDesktop();
  MessageLoopForUI::current()->RunWithDispatcher(host_.get());
}

void Desktop::Draw() {
  compositor_->Draw(false);
}

bool Desktop::DispatchMouseEvent(MouseEvent* event) {
  static const int kMouseButtonFlagMask =
      ui::EF_LEFT_BUTTON_DOWN |
      ui::EF_MIDDLE_BUTTON_DOWN |
      ui::EF_RIGHT_BUTTON_DOWN;

  event->UpdateForTransform(layer()->transform());

  last_mouse_location_ = event->location();

  Window* target =
      mouse_pressed_handler_ ? mouse_pressed_handler_ : capture_window_;
  if (!target)
    target = GetEventHandlerForPoint(event->location());
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      HandleMouseMoved(*event, target);
      break;
    case ui::ET_MOUSE_PRESSED:
      if (!mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      mouse_button_flags_ = event->flags() & kMouseButtonFlagMask;
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      mouse_button_flags_ = event->flags() & kMouseButtonFlagMask;
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

bool Desktop::DispatchKeyEvent(KeyEvent* event) {
#if !defined(NDEBUG)
  // TODO(beng): replace this hack with global keyboard event handlers.
  if (event->type() == ui::ET_KEY_PRESSED) {
    if (MaybeFullScreen(host_.get(), event) || MaybeRotate(this, event))
      return true;
  }
#endif

  if (focused_window_) {
    KeyEvent translated_event(*event);
    return ProcessKeyEvent(focused_window_, &translated_event);
  }
  return false;
}

bool Desktop::DispatchTouchEvent(TouchEvent* event) {
  event->UpdateForTransform(layer()->transform());
  bool handled = false;
  Window* target =
      touch_event_handler_ ? touch_event_handler_ : capture_window_;
  if (!target)
    target = GetEventHandlerForPoint(event->location());
  if (target) {
    TouchEvent translated_event(*event, this, target);
    ui::TouchStatus status = ProcessTouchEvent(target, &translated_event);
    if (status == ui::TOUCH_STATUS_START)
      touch_event_handler_ = target;
    else if (status == ui::TOUCH_STATUS_END ||
             status == ui::TOUCH_STATUS_CANCEL)
      touch_event_handler_ = NULL;
    handled = status != ui::TOUCH_STATUS_UNKNOWN;
  }
  return handled;
}

void Desktop::OnHostResized(const gfx::Size& size) {
  // The compositor should have the same size as the native desktop host.
  compositor_->WidgetSizeChanged(size);

  // The layer, and all the observers should be notified of the
  // transformed size of the desktop.
  gfx::Rect bounds(size);
  layer()->transform().TransformRect(&bounds);
  SetBounds(gfx::Rect(bounds.size()));
  FOR_EACH_OBSERVER(DesktopObserver, observers_,
                    OnDesktopResized(bounds.size()));
}

void Desktop::SetActiveWindow(Window* window, Window* to_focus) {
  if (!window)
    return;
  // The stacking client may impose rules on what window configurations can be
  // activated or deactivated.
  if (!stacking_client_->CanActivateWindow(window))
    return;
  // The window may not be activate-able.
  if (!window->CanActivate())
    return;
  // Nothing may actually have changed.
  if (active_window_ == window)
    return;

  Window* old_active = active_window_;
  active_window_ = window;
  // Invoke OnLostActive after we've changed the active window. That way if the
  // delegate queries for active state it doesn't think the window is still
  // active.
  if (old_active && old_active->delegate())
    old_active->delegate()->OnLostActive();
  if (active_window_) {
    active_window_->parent()->MoveChildToFront(active_window_);
    if (active_window_->delegate())
      active_window_->delegate()->OnActivated();
    active_window_->GetFocusManager()->SetFocusedWindow(
        to_focus ? to_focus : active_window_);
  }
  FOR_EACH_OBSERVER(DesktopObserver, observers_,
                    OnActiveWindowChanged(active_window_));
}

void Desktop::ActivateTopmostWindow() {
  SetActiveWindow(stacking_client_->GetTopmostWindowToActivate(NULL), NULL);
}

void Desktop::Deactivate(Window* window) {
  // The stacking client may impose rules on what window configurations can be
  // activated or deactivated.
  if (!window || !stacking_client_->CanActivateWindow(window))
    return;
  if (active_window_ != window)
    return;

  Window* to_activate = stacking_client_->GetTopmostWindowToActivate(window);
  if (to_activate)
    SetActiveWindow(to_activate, NULL);
}

void Desktop::WindowDestroying(Window* window) {
  // Update the focused window state if the window was focused.
  if (focused_window_ == window)
    SetFocusedWindow(NULL);

  // When a window is being destroyed it's likely that the WindowDelegate won't
  // want events, so we reset the mouse_pressed_handler_ and capture_window_ and
  // don't sent it release/capture lost events.
  if (mouse_pressed_handler_ == window)
    mouse_pressed_handler_ = NULL;
  if (mouse_moved_handler_ == window)
    mouse_moved_handler_ = NULL;
  if (capture_window_ == window)
    capture_window_ = NULL;
  if (touch_event_handler_ == window)
    touch_event_handler_ = NULL;

  if (in_destructor_ || window != active_window_)
    return;

  // Reset active_window_ before invoking SetActiveWindow so that we don't
  // attempt to notify it while running its destructor.
  active_window_ = NULL;
  SetActiveWindow(stacking_client_->GetTopmostWindowToActivate(window), NULL);
}

MessageLoop::Dispatcher* Desktop::GetDispatcher() {
  return host_.get();
}

void Desktop::AddObserver(DesktopObserver* observer) {
  observers_.AddObserver(observer);
}

void Desktop::RemoveObserver(DesktopObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Desktop::IsMouseButtonDown() const {
  return mouse_button_flags_ != 0;
}

void Desktop::PostNativeEvent(const base::NativeEvent& native_event) {
  host_->PostNativeEvent(native_event);
}

void Desktop::SetCapture(Window* window) {
  if (capture_window_ == window)
    return;

  if (capture_window_ && capture_window_->delegate())
    capture_window_->delegate()->OnCaptureLost();
  capture_window_ = window;

  if (capture_window_) {
    // Make all subsequent mouse events and touch go to the capture window. We
    // shouldn't need to send an event here as OnCaptureLost should take care of
    // that.
    if (mouse_pressed_handler_)
      mouse_pressed_handler_ = capture_window_;
    if (touch_event_handler_)
      touch_event_handler_ = capture_window_;
  } else {
    // When capture is lost, we must reset the event handlers.
    mouse_pressed_handler_ = NULL;
    touch_event_handler_ = NULL;
  }
}

void Desktop::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

void Desktop::SetTransform(const ui::Transform& transform) {
  Window::SetTransform(transform);

  // If the layer is not animating, then we need to update the host size
  // immediately.
  if (!layer()->GetAnimator()->is_animating())
    OnHostResized(host_->GetSize());
}

void Desktop::HandleMouseMoved(const MouseEvent& event, Window* target) {
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

bool Desktop::ProcessMouseEvent(Window* target, MouseEvent* event) {
  if (!target->IsVisible())
    return false;

  EventFilters filters;
  GetEventFiltersToNotify(target, &filters);
  for (EventFilters::const_reverse_iterator it = filters.rbegin();
       it != filters.rend(); ++it) {
    if ((*it)->PreHandleMouseEvent(target, event))
      return true;
  }

  return target->delegate()->OnMouseEvent(event);
}

bool Desktop::ProcessKeyEvent(Window* target, KeyEvent* event) {
  if (!target->IsVisible())
    return false;

  EventFilters filters;
  GetEventFiltersToNotify(target, &filters);
  for (EventFilters::const_reverse_iterator it = filters.rbegin();
       it != filters.rend(); ++it) {
    if ((*it)->PreHandleKeyEvent(target, event))
      return true;
  }

  return target->delegate()->OnKeyEvent(event);
}

ui::TouchStatus Desktop::ProcessTouchEvent(Window* target, TouchEvent* event) {
  if (!target->IsVisible())
    return ui::TOUCH_STATUS_UNKNOWN;

  EventFilters filters;
  GetEventFiltersToNotify(target, &filters);
  for (EventFilters::const_reverse_iterator it = filters.rbegin();
       it != filters.rend(); ++it) {
    ui::TouchStatus status = (*it)->PreHandleTouchEvent(target, event);
    if (status != ui::TOUCH_STATUS_UNKNOWN)
      return status;
  }

  return target->delegate()->OnTouchEvent(event);
}

void Desktop::ScheduleDraw() {
  if (!schedule_paint_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&Desktop::Draw, schedule_paint_factory_.GetWeakPtr()));
  }
}

bool Desktop::CanFocus() const {
  return IsVisible();
}

internal::FocusManager* Desktop::GetFocusManager() {
  return this;
}

Desktop* Desktop::GetDesktop() {
  return this;
}

void Desktop::WindowDetachedFromDesktop(Window* detached) {
  DCHECK(capture_window_ != this);

  // If the ancestor of the capture window is detached,
  // release the capture.
  aura::Window* window = capture_window_;
  while (window && window != detached)
    window = window->parent();
  if (window && window != this)
    ReleaseCapture(capture_window_);

  // If the ancestor of the capture window is detached,
  // release the focus.
  window = focused_window_;
  while (window && window != detached)
    window = window->parent();
  if (window)
    SetFocusedWindow(NULL);
}

void Desktop::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* animation) {
  OnHostResized(host_->GetSize());
}

void Desktop::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* animation) {
}

void Desktop::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* animation) {
}

void Desktop::SetFocusedWindow(Window* focused_window) {
  if (focused_window == focused_window_ ||
      (focused_window && !focused_window->CanFocus())) {
    return;
  }
  if (focused_window_ && focused_window_->delegate())
    focused_window_->delegate()->OnBlur();
  focused_window_ = focused_window;
  if (focused_window_ && focused_window_->delegate())
    focused_window_->delegate()->OnFocus();
}

Window* Desktop::GetFocusedWindow() {
  return focused_window_;
}

bool Desktop::IsFocusedWindow(const Window* window) const {
  return focused_window_ == window;
}

void Desktop::Init() {
  Window::Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  SetBounds(gfx::Rect(host_->GetSize()));
  Show();
  compositor()->SetRootLayer(layer());
}

gfx::Rect Desktop::GetInitialHostWindowBounds() const {
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);

  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  vector<string> parts;
  base::SplitString(size_str, 'x', &parts);
  int parsed_width = 0, parsed_height = 0;
  if (parts.size() == 2 &&
      base::StringToInt(parts[0], &parsed_width) && parsed_width > 0 &&
      base::StringToInt(parts[1], &parsed_height) && parsed_height > 0) {
    bounds.set_size(gfx::Size(parsed_width, parsed_height));
  } else if (use_fullscreen_host_window_) {
    bounds = gfx::Rect(DesktopHost::GetNativeDisplaySize());
  }

  return bounds;
}

}  // namespace aura

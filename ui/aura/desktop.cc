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
#include "ui/aura/desktop_delegate.h"
#include "ui/aura/desktop_host.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

using std::string;
using std::vector;

namespace aura {

namespace {

// Default bounds for the host window.
static const int kDefaultHostWindowX = 200;
static const int kDefaultHostWindowY = 200;
static const int kDefaultHostWindowWidth = 1280;
static const int kDefaultHostWindowHeight = 1024;

class DefaultDesktopDelegate : public DesktopDelegate {
 public:
  explicit DefaultDesktopDelegate(Desktop* desktop) : desktop_(desktop) {}
  virtual ~DefaultDesktopDelegate() {}

 private:
  virtual void AddChildToDefaultParent(Window* window) OVERRIDE {
    desktop_->AddChild(window);
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

  DISALLOW_COPY_AND_ASSIGN(DefaultDesktopDelegate);
};

}  // namespace

Desktop* Desktop::instance_ = NULL;
bool Desktop::use_fullscreen_host_window_ = false;

Desktop::Desktop()
    : Window(NULL),
      host_(aura::DesktopHost::Create(GetInitialHostWindowBounds())),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          delegate_(new DefaultDesktopDelegate(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(schedule_paint_factory_(this)),
      active_window_(NULL),
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

void Desktop::SetDelegate(DesktopDelegate* delegate) {
  delegate_.reset(delegate);
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
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      break;
    default:
      break;
  }
  if (target && target->delegate()) {
    MouseEvent translated_event(*event, this, target);
    return target->OnMouseEvent(&translated_event);
  }
  return false;
}

bool Desktop::DispatchKeyEvent(KeyEvent* event) {
#if !defined(NDEBUG)
  // Press Home key to rotate the screen. Primarily used for testing.
  if (event->type() == ui::ET_KEY_PRESSED &&
      (event->flags() & ui::EF_CONTROL_DOWN) &&
      event->key_code() == ui::VKEY_HOME) {
    ui::Transform transform;
    static int count = 0;
    gfx::Size size = host_->GetSize();
    switch (count) {
      case 0:
        transform.ConcatRotate(-90.0f);
        transform.ConcatTranslate(0, size.height());
        break;
      case 1:
        transform.ConcatRotate(180.0f);
        transform.ConcatTranslate(size.width(), size.height());
        break;
      case 2:
        transform.ConcatRotate(90.0f);
        transform.ConcatTranslate(size.width(), 0);
        break;
    }
    layer()->SetAnimation(CreateDefaultAnimation());
    SetTransform(transform);
    count = (count + 1) % 4;
    return true;
  }
#endif

  if (focused_window_) {
    KeyEvent translated_event(*event);
    return focused_window_->OnKeyEvent(&translated_event);
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
    ui::TouchStatus status = target->OnTouchEvent(&translated_event);
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
  // We only allow top level windows to be active.
  if (window && window != window->GetToplevelWindow()) {
    // Ignore requests to activate windows that aren't in a top level window.
    return;
  }

  if (active_window_ == window)
    return;
  if (active_window_ && active_window_->delegate())
    active_window_->delegate()->OnLostActive();
  active_window_ = window;
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
  SetActiveWindow(delegate_->GetTopmostWindowToActivate(NULL), NULL);
}

void Desktop::Deactivate(Window* window) {
  if (!window)
    return;

  Window* toplevel_ancestor = window->GetToplevelWindow();
  if (!toplevel_ancestor || toplevel_ancestor != window)
    return;  // Not a top level window.

  if (active_window() != toplevel_ancestor)
    return;  // Top level ancestor is already not active.

  Window* to_activate =
      delegate_->GetTopmostWindowToActivate(toplevel_ancestor);
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
  SetActiveWindow(delegate_->GetTopmostWindowToActivate(window), NULL);
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
  }
}

void Desktop::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;

  if (capture_window_ && capture_window_->delegate())
    capture_window_->delegate()->OnCaptureLost();
  capture_window_ = NULL;
}

void Desktop::SetTransform(const ui::Transform& transform) {
  Window::SetTransform(transform);

  // If the layer is not animating, then we need to update the host size
  // immediately.
  if (!layer()->has_animation())
    OnHostResized(host_->GetSize());
}

void Desktop::HandleMouseMoved(const MouseEvent& event, Window* target) {
  if (target == mouse_moved_handler_)
    return;

  // Send an exited event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    MouseEvent translated_event(event, this, mouse_moved_handler_,
                                ui::ET_MOUSE_EXITED);
    mouse_moved_handler_->OnMouseEvent(&translated_event);
  }
  mouse_moved_handler_ = target;
  // Send an entered event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    MouseEvent translated_event(event, this, mouse_moved_handler_,
                                ui::ET_MOUSE_ENTERED);
    mouse_moved_handler_->OnMouseEvent(&translated_event);
  }
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

void Desktop::OnLayerAnimationEnded(const ui::Animation* animation) {
  OnHostResized(host_->GetSize());
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
  Window::Init();
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

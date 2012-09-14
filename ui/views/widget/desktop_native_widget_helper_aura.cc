// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_native_widget_helper_aura.h"

#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_cursor_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/aura/shared/root_window_capture_client.h"
#include "ui/aura/window_property.h"
#include "ui/views/widget/native_widget_aura.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_subclass.h"
#include "ui/views/widget/widget_message_filter.h"
#elif defined(USE_X11)
#include "ui/base/x/x11_util.h"
#include "ui/views/widget/x11_desktop_handler.h"
#include "ui/views/widget/x11_desktop_window_move_client.h"
#include "ui/views/widget/x11_window_event_filter.h"
#endif

namespace views {

DEFINE_WINDOW_PROPERTY_KEY(
    aura::Window*, kViewsWindowForRootWindow, NULL);

namespace {

// Client that always offsets by the toplevel RootWindow of the passed
// in child NativeWidgetAura.
class DesktopScreenPositionClient
    : public aura::client::ScreenPositionClient {
 public:
  DesktopScreenPositionClient() {}
  virtual ~DesktopScreenPositionClient() {}

  // aura::client::ScreenPositionClient overrides:
  virtual void ConvertPointToScreen(const aura::Window* window,
                                    gfx::Point* point) OVERRIDE {
    const aura::RootWindow* root_window = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window, root_window, point);
    gfx::Point origin = root_window->GetHostOrigin();
    point->Offset(origin.x(), origin.y());
  }

  virtual void ConvertPointFromScreen(const aura::Window* window,
                                      gfx::Point* point) OVERRIDE {
    const aura::RootWindow* root_window = window->GetRootWindow();
    gfx::Point origin = root_window->GetHostOrigin();
    point->Offset(-origin.x(), -origin.y());
    aura::Window::ConvertPointToTarget(root_window, window, point);
  }

  virtual void SetBounds(aura::Window* window,
                         const gfx::Rect& bounds,
                         const gfx::Display& display) OVERRIDE {
    // TODO: Use the 3rd parameter, |display|.
    gfx::Point origin = bounds.origin();
    aura::RootWindow* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window->parent(), root, &origin);

#if !defined(OS_WIN)
    if  (window->type() == aura::client::WINDOW_TYPE_CONTROL) {
      window->SetBounds(gfx::Rect(origin, bounds.size()));
      return;
    } else if (window->type() == aura::client::WINDOW_TYPE_POPUP) {
      // The caller expects windows we consider "embedded" to be placed in the
      // screen coordinate system. So we need to offset the root window's
      // position (which is in screen coordinates) from these bounds.
      gfx::Point host_origin = root->GetHostOrigin();
      origin.Offset(-host_origin.x(), -host_origin.y());
      window->SetBounds(gfx::Rect(origin, bounds.size()));
      return;
    }
#endif  // !defined(OS_WIN)
    root->SetHostBounds(bounds);
    window->SetBounds(gfx::Rect(bounds.size()));
  }
};

}  // namespace

DesktopNativeWidgetHelperAura::DesktopNativeWidgetHelperAura(
    NativeWidgetAura* widget)
    : widget_(widget),
      window_(NULL),
      root_window_event_filter_(NULL),
      is_embedded_window_(false) {
}

DesktopNativeWidgetHelperAura::~DesktopNativeWidgetHelperAura() {
  if (window_)
    window_->RemoveObserver(this);

  if (root_window_event_filter_) {
#if defined(USE_X11)
    root_window_event_filter_->RemoveFilter(x11_window_move_client_.get());
    root_window_event_filter_->RemoveFilter(x11_window_event_filter_.get());
#endif

    root_window_event_filter_->RemoveFilter(input_method_filter_.get());
  }
}

// static
aura::Window* DesktopNativeWidgetHelperAura::GetViewsWindowForRootWindow(
    aura::RootWindow* root) {
  return root ? root->GetProperty(kViewsWindowForRootWindow) : NULL;
}

void DesktopNativeWidgetHelperAura::PreInitialize(
    aura::Window* window,
    const Widget::InitParams& params) {
#if !defined(OS_WIN)
  // We don't want the status bubble or the omnibox to get their own root
  // window on the desktop; on Linux
  //
  // TODO(erg): This doesn't map perfectly to what I want to do. TYPE_POPUP is
  // used for lots of stuff, like dragged tabs, and I only want this to trigger
  // for the status bubble and the omnibox.
  if (params.type == Widget::InitParams::TYPE_POPUP ||
      params.type == Widget::InitParams::TYPE_BUBBLE) {
    is_embedded_window_ = true;
    return;
  } else if (params.type == Widget::InitParams::TYPE_CONTROL) {
    return;
  }
#endif

  gfx::Rect bounds = params.bounds;
  if (bounds.IsEmpty()) {
    // We must pass some non-zero value when we initialize a RootWindow. This
    // will probably be SetBounds()ed soon.
    bounds.set_size(gfx::Size(100, 100));
  }

  aura::FocusManager* focus_manager = NULL;
  aura::DesktopActivationClient* activation_client = NULL;
#if defined(USE_X11)
  focus_manager = X11DesktopHandler::get()->get_focus_manager();
  activation_client = X11DesktopHandler::get()->get_activation_client();
#else
  // TODO(ben): This is almost certainly wrong; I suspect that the windows
  // build will need a singleton like above.
  focus_manager = new aura::FocusManager;
  activation_client = new aura::DesktopActivationClient(focus_manager);
#endif

  root_window_.reset(
      new aura::RootWindow(aura::RootWindow::CreateParams(bounds)));
  root_window_->SetProperty(kViewsWindowForRootWindow, window);
  root_window_->Init();
  root_window_->set_focus_manager(focus_manager);

  // No event filter for aura::Env. Create CompoundEvnetFilter per RootWindow.
  root_window_event_filter_ = new aura::shared::CompoundEventFilter;
  // Pass ownership of the filter to the root_window.
  root_window_->SetEventFilter(root_window_event_filter_);

  input_method_filter_.reset(new aura::shared::InputMethodEventFilter());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window_.get());
  root_window_event_filter_->AddFilter(input_method_filter_.get());

  capture_client_.reset(
      new aura::shared::RootWindowCaptureClient(root_window_.get()));

  cursor_client_.reset(new aura::DesktopCursorClient(root_window_.get()));
  aura::client::SetCursorClient(root_window_.get(), cursor_client_.get());

#if defined(USE_X11)
  x11_window_event_filter_.reset(
      new X11WindowEventFilter(root_window_.get(), activation_client, widget_));
  x11_window_event_filter_->SetUseHostWindowBorders(false);
  root_window_event_filter_->AddFilter(x11_window_event_filter_.get());

  if (params.type == Widget::InitParams::TYPE_MENU) {
    ::Window window = root_window_->GetAcceleratedWidget();
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    XChangeWindowAttributes(ui::GetXDisplay(), window, CWOverrideRedirect,
        &attributes);
  }
#endif

  root_window_->AddRootWindowObserver(this);

  window_ = window;
  window_->AddObserver(this);

  aura::client::SetActivationClient(root_window_.get(), activation_client);
  aura::client::SetDispatcherClient(root_window_.get(),
                                    new aura::DesktopDispatcherClient);
#if defined(USE_X11)
  // TODO(ben): A window implementation of this will need to be written.
  x11_window_move_client_.reset(new X11DesktopWindowMoveClient);
  root_window_event_filter_->AddFilter(x11_window_move_client_.get());
  aura::client::SetWindowMoveClient(root_window_.get(),
                                    x11_window_move_client_.get());
#endif

  position_client_.reset(new DesktopScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_.get(),
                                        position_client_.get());
}

void DesktopNativeWidgetHelperAura::PostInitialize() {
#if defined(OS_WIN)
  DCHECK(root_window_->GetAcceleratedWidget());
  hwnd_message_filter_.reset(new WidgetMessageFilter(root_window_.get(),
      widget_->GetWidget()));
  ui::HWNDSubclass::AddFilterToTarget(root_window_->GetAcceleratedWidget(),
                                      hwnd_message_filter_.get());
#endif
}

aura::RootWindow* DesktopNativeWidgetHelperAura::GetRootWindow() {
  return root_window_.get();
}

gfx::Rect DesktopNativeWidgetHelperAura::ModifyAndSetBounds(
    const gfx::Rect& bounds) {
  gfx::Rect out_bounds = bounds;
  if (root_window_.get() && !out_bounds.IsEmpty()) {
    // TODO(scottmg): This avoids the AdjustWindowRect that ash wants to
    // adjust the top level on Windows.
#if !defined(OS_WIN)
    root_window_->SetHostBounds(out_bounds);
#endif
    out_bounds.set_x(0);
    out_bounds.set_y(0);
  } else if (is_embedded_window_) {
    // The caller expects windows we consider "embedded" to be placed in the
    // screen coordinate system. So we need to offset the root window's
    // position (which is in screen coordinates) from these bounds.
    aura::RootWindow* root =
        widget_->GetNativeWindow()->GetRootWindow()->GetRootWindow();
    gfx::Point point = root->GetHostOrigin();
    out_bounds.set_x(out_bounds.x() - point.x());
    out_bounds.set_y(out_bounds.y() - point.y());
  }

  return out_bounds;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetHelperAura, aura::WindowObserver implementation:
void DesktopNativeWidgetHelperAura::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  DCHECK_EQ(window, window_);

  // Since we're trying to hide the main window, hide the OS level root as well.
  if (visible)
    root_window_->ShowRootWindow();
  else
    root_window_->HideRootWindow();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetHelperAura, aura::RootWindowObserver implementation:

void DesktopNativeWidgetHelperAura::OnRootWindowResized(
    const aura::RootWindow* root,
    const gfx::Size& old_size) {
  DCHECK_EQ(root, root_window_.get());
  widget_->SetBounds(gfx::Rect(root->GetHostOrigin(),
                               root->GetHostSize()));
}

void DesktopNativeWidgetHelperAura::OnRootWindowHostCloseRequested(
    const aura::RootWindow* root) {
  DCHECK_EQ(root, root_window_.get());
  widget_->GetWidget()->Close();
}

void DesktopNativeWidgetHelperAura::OnRootWindowMoved(
    const aura::RootWindow* root,
    const gfx::Point& new_origin) {
  widget_->GetWidget()->OnNativeWidgetMove();
}

}  // namespace views

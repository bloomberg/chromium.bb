// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/x11_desktop_handler.h"

#include "base/message_loop.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/base/x/x11_util.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/ime/input_method.h"
#include "ui/views/widget/desktop_native_widget_helper_aura.h"
#include "ui/views/widget/desktop_root_window_host_linux.h"
#endif

namespace {

const char* kAtomsToCache[] = {
  "_NET_ACTIVE_WINDOW",
  NULL
};

// Our global instance. Deleted when our Env() is deleted.
views::X11DesktopHandler* g_handler = NULL;

}  // namespace

namespace views {

// static
X11DesktopHandler* X11DesktopHandler::get() {
  if (!g_handler)
    g_handler = new X11DesktopHandler;

  return g_handler;
}

X11DesktopHandler::X11DesktopHandler()
    : xdisplay_(base::MessagePumpAuraX11::GetDefaultXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      atom_cache_(xdisplay_, kAtomsToCache),
      focus_manager_(new aura::FocusManager),
      desktop_activation_client_(
          new aura::DesktopActivationClient(focus_manager_.get())) {
  base::MessagePumpAuraX11::Current()->AddDispatcherForRootWindow(this);
  aura::Env::GetInstance()->AddObserver(this);

  XWindowAttributes attr;
  XGetWindowAttributes(xdisplay_, x_root_window_, &attr);
  XSelectInput(xdisplay_, x_root_window_,
               attr.your_event_mask | PropertyChangeMask |
               StructureNotifyMask | SubstructureNotifyMask);
}

X11DesktopHandler::~X11DesktopHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
  base::MessagePumpAuraX11::Current()->RemoveDispatcherForRootWindow(this);
}

bool X11DesktopHandler::Dispatch(const base::NativeEvent& event) {
  // Check for a change to the active window.
  switch (event->type) {
    case PropertyNotify: {
      ::Atom active_window = atom_cache_.GetAtom("_NET_ACTIVE_WINDOW");

      if (event->xproperty.window == x_root_window_ &&
          event->xproperty.atom == active_window) {
        int window;
        if (ui::GetIntProperty(x_root_window_, "_NET_ACTIVE_WINDOW", &window) &&
            window) {
          OnActiveWindowChanged(static_cast< ::Window>(window));
        }
      }
      break;
    }
  }

  return true;
}

void X11DesktopHandler::OnWindowInitialized(aura::Window* window) {
}

void X11DesktopHandler::OnWillDestroyEnv() {
  g_handler = NULL;
  delete this;
}

// This code should live elsewhere and should only trigger if a non-RootWindow
// has been selected.
void X11DesktopHandler::OnActiveWindowChanged(::Window xid) {
#if defined(OS_CHROMEOS)
  // This is a temporary hack because it looks like chromeos both does and
  // doesn't include DNWHA in different targets. It'll be awesome when I can
  // rip that out.
  aura::Window* window = NULL;
#else
  aura::RootWindow* root_window =
      aura::RootWindow::GetForAcceleratedWidget(xid);
  // TODO(erg): Rip out DesktopNativeWidgetHelperAura and replace with the if
  // block below.
  aura::Window* window = root_window ?
      views::DesktopNativeWidgetHelperAura::GetViewsWindowForRootWindow(
          root_window) : NULL;

  if (!window) {
    window = root_window ?
             views::DesktopRootWindowHostLinux::GetContentWindowForXID(xid) :
             NULL;
  }
#endif

  desktop_activation_client_->ActivateWindow(window);
}

}  // namespace views

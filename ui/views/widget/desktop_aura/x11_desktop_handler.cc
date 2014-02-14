// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "base/message_loop/message_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/x/x11_util.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/ime/input_method.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#endif

namespace {

const char* kAtomsToCache[] = {
  "_NET_ACTIVE_WINDOW",
  "_NET_WM_STATE_HIDDEN",
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
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      current_window_(None),
      atom_cache_(xdisplay_, kAtomsToCache),
      wm_supports_active_window_(false) {
  base::MessagePumpX11::Current()->AddDispatcherForRootWindow(this);
  aura::Env::GetInstance()->AddObserver(this);

  XWindowAttributes attr;
  XGetWindowAttributes(xdisplay_, x_root_window_, &attr);
  XSelectInput(xdisplay_, x_root_window_,
               attr.your_event_mask | PropertyChangeMask |
               StructureNotifyMask | SubstructureNotifyMask);

  ::Window active_window;
  wm_supports_active_window_ =
    ui::GetXIDProperty(x_root_window_, "_NET_ACTIVE_WINDOW", &active_window) &&
    active_window;
}

X11DesktopHandler::~X11DesktopHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
  base::MessagePumpX11::Current()->RemoveDispatcherForRootWindow(this);
}

void X11DesktopHandler::ActivateWindow(::Window window) {
  if (wm_supports_active_window_) {
    DCHECK_EQ(gfx::GetXDisplay(), xdisplay_);

    XEvent xclient;
    memset(&xclient, 0, sizeof(xclient));
    xclient.type = ClientMessage;
    xclient.xclient.window = window;
    xclient.xclient.message_type = atom_cache_.GetAtom("_NET_ACTIVE_WINDOW");
    xclient.xclient.format = 32;
    xclient.xclient.data.l[0] = 1;  // Specified we are an app.
    xclient.xclient.data.l[1] = CurrentTime;
    xclient.xclient.data.l[2] = None;
    xclient.xclient.data.l[3] = 0;
    xclient.xclient.data.l[4] = 0;

    XSendEvent(xdisplay_, x_root_window_, False,
               SubstructureRedirectMask | SubstructureNotifyMask,
               &xclient);
  } else {
    XRaiseWindow(xdisplay_, window);

    // XRaiseWindow will not give input focus to the window. We now need to ask
    // the X server to do that. Note that the call will raise an X error if the
    // window is not mapped.
    XSetInputFocus(xdisplay_, window, RevertToParent, CurrentTime);

    OnActiveWindowChanged(window);
  }
}

void X11DesktopHandler::DeactivateWindow(::Window window) {
  DCHECK(IsActiveWindow(window));

  std::vector< ::Window > windows;
  if (!ui::GetXWindowStack(x_root_window_, &windows)) {
    // TODO(mlamouri): use XQueryTree in case of _NET_CLIENT_LIST_STACKING is
    // not supported by the WM.
    NOTIMPLEMENTED();
    return;
  }

  // To deactivate the window, we want to activate the window below in z-order.
  ::Window to_activate = GetNextToActivateInStack(windows);
  // TODO(mlamouri): some WM might not have a "Desktop" to activate when there
  // are no other windows. In which case, we should handle the case where
  // |to_activate| == 0.
  if (!to_activate) {
    DLOG(WARNING) << "Not deactivating because there are no other window to "
                  << "activate. If you see this please file a bug and mention "
                  << "the window manager you are using.";
    return;
  }

  ActivateWindow(to_activate);
}

bool X11DesktopHandler::IsActiveWindow(::Window window) const {
  return window == current_window_;
}

void X11DesktopHandler::ProcessXEvent(const base::NativeEvent& event) {
  switch (event->type) {
    case EnterNotify:
      if (event->xcrossing.focus == True &&
          current_window_ != event->xcrossing.window)
        OnActiveWindowChanged(event->xcrossing.window);
      break;
    case LeaveNotify:
      if (event->xcrossing.focus == False &&
          current_window_ == event->xcrossing.window)
        OnActiveWindowChanged(None);
      break;
    case FocusIn:
      if (event->xfocus.mode == NotifyNormal &&
          current_window_ != event->xfocus.window)
        OnActiveWindowChanged(event->xfocus.window);
      break;
    default:
      NOTREACHED();
  }
}

uint32_t X11DesktopHandler::Dispatch(const base::NativeEvent& event) {
  // Check for a change to the active window.
  switch (event->type) {
    case PropertyNotify: {
      ::Atom active_window_atom = atom_cache_.GetAtom("_NET_ACTIVE_WINDOW");

      if (event->xproperty.window == x_root_window_ &&
          event->xproperty.atom == active_window_atom) {
        ::Window window;
        if (ui::GetXIDProperty(x_root_window_, "_NET_ACTIVE_WINDOW", &window) &&
            window) {
          OnActiveWindowChanged(window);
        }
      }
      break;
    }
  }

  return POST_DISPATCH_NONE;
}

void X11DesktopHandler::OnWindowInitialized(aura::Window* window) {
}

void X11DesktopHandler::OnWillDestroyEnv() {
  g_handler = NULL;
  delete this;
}

void X11DesktopHandler::OnActiveWindowChanged(::Window xid) {
  if (current_window_ == xid)
    return;
  DesktopWindowTreeHostX11* old_host =
      views::DesktopWindowTreeHostX11::GetHostForXID(current_window_);
  if (old_host)
    old_host->HandleNativeWidgetActivationChanged(false);

  DesktopWindowTreeHostX11* new_host =
      views::DesktopWindowTreeHostX11::GetHostForXID(xid);
  if (new_host)
    new_host->HandleNativeWidgetActivationChanged(true);

  current_window_ = xid;
}

::Window X11DesktopHandler::GetNextToActivateInStack(
    const std::vector< ::Window >& windows) {
  DCHECK(current_window_);

  // We start by doing a fast forward in the stack to find the active window.
  std::vector< ::Window >::const_iterator it =
      std::find(windows.begin(), windows.end(), current_window_);

  // We expect to find the currently active window in the |windows| list. Not
  // finding it is an unexpected behavior.
  CHECK(it != windows.end());

  // After that, we want to activate the next window that is not minimized.
  for (++it; it != windows.end(); ++it) {
    std::vector<Atom> wm_states;
    if (!ui::GetAtomArrayProperty(*it, "_NET_WM_STATE", &wm_states))
      continue;

    std::vector<Atom>::const_iterator hidden_atom =
        std::find(wm_states.begin(),
                  wm_states.end(),
                  atom_cache_.GetAtom("_NET_WM_STATE_HIDDEN"));

    if (hidden_atom != wm_states.end())
      continue;

    // We could do more checks like verifying that _NET_WM_WINDOW_TYPE is a
    // value we are happy with or that _NET_WM_STATE does not contain
    // _NET_WM_STATE_SKIP_PAGER or _NET_WM_STATE_SKIP_TASKBAR but those calls
    // would cost and so far, the tested WM put the special window before the
    // active one in the stack and the desktop at the end of the stack. That
    // matches pretty well the behavior we are looking for.

    return *it;
  }

  // If we reached that point, that means we have not found an appropriate
  // window to activate. There is nothing we can do about it and the caller
  // should take care of doing the right thing.
  return 0;
}

}  // namespace views

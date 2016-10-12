// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client_managed.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/views/widget/desktop_aura/x11_pointer_grab.h"

namespace views {

namespace {

bool IsButtonReleaseEvent(XEvent* xev) {
  if (xev->type == ButtonRelease)
    return xev->xbutton.button == Button1;
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    if (xievent->evtype == XI_RawButtonRelease)
      return ui::EventButtonFromNative(xev) == Button1;
  }
  return false;
}

void SelectButtonReleaseEvents(bool select) {
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));
  if (select)
    XISetMask(mask, XI_RawButtonRelease);
  XIEventMask evmask;
  evmask.deviceid = XIAllMasterDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XDisplay* display = gfx::GetXDisplay();
  XISelectEvents(display, DefaultRootWindow(display), &evmask, 1);
}

}  // anonymous namespace

X11DesktopWindowMoveClientManaged::X11DesktopWindowMoveClientManaged()
    : weak_factory_(this) {}
X11DesktopWindowMoveClientManaged::~X11DesktopWindowMoveClientManaged() {}

bool X11DesktopWindowMoveClientManaged::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return in_move_loop_;
}

uint32_t X11DesktopWindowMoveClientManaged::DispatchEvent(
    const ui::PlatformEvent& event) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  XEvent* xev = event;

  // This method processes all events while the move loop is active.
  if (!in_move_loop_)
    return ui::POST_DISPATCH_PERFORM_DEFAULT;

  if (IsButtonReleaseEvent(xev)) {
    EndMoveLoop();
    return ui::POST_DISPATCH_NONE;
  }
  return ui::POST_DISPATCH_PERFORM_DEFAULT;
}

aura::client::WindowMoveResult X11DesktopWindowMoveClientManaged::RunMoveLoop(
    aura::Window* window,
    const gfx::Vector2d& drag_offset,
    aura::client::WindowMoveSource move_source) {
  auto screen_location = window->GetBoundsInScreen().origin() + drag_offset;
  window_ = window->GetHost()->GetAcceleratedWidget();

  SelectButtonReleaseEvents(true);

  window->ReleaseCapture();
  UngrabPointer();
  MoveResizeManagedWindow(window_, screen_location, ui::NetWmMoveResize::MOVE);

  std::unique_ptr<ui::ScopedEventDispatcher> old_dispatcher =
      std::move(nested_dispatcher_);
  nested_dispatcher_ =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  base::WeakPtr<X11DesktopWindowMoveClientManaged> alive(
      weak_factory_.GetWeakPtr());

  in_move_loop_ = true;
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  if (!alive)
    return aura::client::MOVE_CANCELED;

  nested_dispatcher_ = std::move(old_dispatcher);
  return aura::client::MOVE_SUCCESSFUL;
}

void X11DesktopWindowMoveClientManaged::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  MoveResizeManagedWindow(window_, gfx::Point(), ui::NetWmMoveResize::CANCEL);
  SelectButtonReleaseEvents(false);

  quit_closure_.Run();
}

}  // namespace views

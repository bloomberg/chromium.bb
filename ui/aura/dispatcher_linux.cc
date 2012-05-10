// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/dispatcher_linux.h"

#include <X11/extensions/XInput2.h>

#include "ui/base/events.h"

namespace {

// Pro-processes an XEvent before it is handled. The pre-processings include:
// - Map Alt+Button1 to Button3
void PreprocessXEvent(XEvent* xevent) {
  if (!xevent || xevent->type != GenericEvent)
    return;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xevent->xcookie.data);
  if ((xievent->evtype == XI_ButtonPress ||
      xievent->evtype == XI_ButtonRelease) &&
        (xievent->mods.effective & Mod1Mask) &&
        xievent->detail == 1) {
    xievent->mods.effective &= ~Mod1Mask;
    xievent->detail = 3;
    if (xievent->evtype == XI_ButtonRelease) {
      // On the release clear the left button from the existing state and the
      // mods, and set the right button.
      XISetMask(xievent->buttons.mask, 3);
      XIClearMask(xievent->buttons.mask, 1);
      xievent->mods.effective &= ~Button1Mask;
    }
  }
}

}  // namespace

namespace aura {

DispatcherLinux::DispatcherLinux() {
  base::MessagePumpX::SetDefaultDispatcher(this);
}

DispatcherLinux::~DispatcherLinux() {
  base::MessagePumpX::SetDefaultDispatcher(NULL);
}

void DispatcherLinux::WindowDispatcherCreated(
    ::Window window,
    MessageLoop::Dispatcher* dispatcher) {
  dispatchers_.insert(std::make_pair(window, dispatcher));
}

void DispatcherLinux::WindowDispatcherDestroying(::Window window) {
  dispatchers_.erase(window);
}

bool DispatcherLinux::Dispatch(const base::NativeEvent& xev) {
  PreprocessXEvent(xev);

  // XI_HierarchyChanged events are special. There is no window associated with
  // these events. So process them directly from here.
  if (xev->type == GenericEvent &&
      xev->xgeneric.evtype == XI_HierarchyChanged) {
    ui::UpdateDeviceList();
    return true;
  }

  // MappingNotify events (meaning that the keyboard or pointer buttons have
  // been remapped) aren't associated with a window; send them to all
  // dispatchers.
  if (xev->type == MappingNotify) {
    for (DispatchersMap::const_iterator it = dispatchers_.begin();
         it != dispatchers_.end(); ++it) {
      it->second->Dispatch(xev);
    }
    return true;
  }

  MessageLoop::Dispatcher* dispatcher = GetDispatcherForXEvent(xev);
  return dispatcher ? dispatcher->Dispatch(xev) : true;
}

MessageLoop::Dispatcher* DispatcherLinux::GetDispatcherForXEvent(
    XEvent* xev) const {
  ::Window window = xev->xany.window;
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    window = xievent->event;
  }
  DispatchersMap::const_iterator it = dispatchers_.find(window);
  return it != dispatchers_.end() ? it->second : NULL;
}

MessageLoop::Dispatcher* CreateDispatcher() {
  return new DispatcherLinux;
}

}  // namespace aura

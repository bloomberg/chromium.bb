// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/dispatcher_linux.h"

#include <X11/extensions/XInput2.h>

#include "ui/base/events.h"

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

base::MessagePumpDispatcher::DispatchStatus DispatcherLinux::Dispatch(
    XEvent* xev) {
  // XI_HierarchyChanged events are special. There is no window associated with
  // these events. So process them directly from here.
  if (xev->type == GenericEvent &&
      xev->xgeneric.evtype == XI_HierarchyChanged) {
    ui::UpdateDeviceList();
    return EVENT_PROCESSED;
  }
  MessageLoop::Dispatcher* dispatcher = GetDispatcherForXEvent(xev);
  return dispatcher ? dispatcher->Dispatch(xev) : EVENT_IGNORED;
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

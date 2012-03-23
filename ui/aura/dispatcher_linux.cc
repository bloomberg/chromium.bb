// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/dispatcher_linux.h"

#include <X11/extensions/XInput2.h>

#include "ui/aura/root_window_host_linux.h"
#include "ui/base/events.h"

namespace aura {

DispatcherLinux::DispatcherLinux() {
  base::MessagePumpX::SetDefaultDispatcher(this);
}

DispatcherLinux::~DispatcherLinux() {
  base::MessagePumpX::SetDefaultDispatcher(NULL);
}

void DispatcherLinux::RootWindowHostCreated(::Window window,
                                            ::Window root,
                                            RootWindowHostLinux* host) {
  hosts_.insert(std::make_pair(window, host));
  hosts_.insert(std::make_pair(root, host));
}

void DispatcherLinux::RootWindowHostDestroying(::Window window, ::Window root) {
  hosts_.erase(window);
  hosts_.erase(root);
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
  RootWindowHostLinux* host = GetRootWindowHostForXEvent(xev);
  return host ? host->Dispatch(xev) : EVENT_IGNORED;
}

RootWindowHostLinux* DispatcherLinux::GetRootWindowHostForXEvent(
    XEvent* xev) const {
  ::Window window = xev->xany.window;
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    window = xievent->event;
  }
  HostsMap::const_iterator it = hosts_.find(window);
  return it != hosts_.end() ? it->second : NULL;
}

Dispatcher* CreateDispatcher() {
  return new DispatcherLinux;
}

}  // namespace aura

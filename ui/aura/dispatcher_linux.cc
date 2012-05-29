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

DispatcherLinux::DispatcherLinux()
    : x_root_window_(
        DefaultRootWindow(base::MessagePumpAuraX11::GetDefaultXDisplay())) {
  base::MessagePumpAuraX11::SetDefaultDispatcher(this);
  MessageLoopForUI::current()->AddObserver(this);
}

DispatcherLinux::~DispatcherLinux() {
  MessageLoopForUI::current()->RemoveObserver(this);
  base::MessagePumpAuraX11::SetDefaultDispatcher(NULL);
}

void DispatcherLinux::AddDispatcherForWindow(
    MessageLoop::Dispatcher* dispatcher,
    ::Window x_window) {
  dispatchers_.insert(std::make_pair(x_window, dispatcher));
}

void DispatcherLinux::RemoveDispatcherForWindow(::Window x_window) {
  dispatchers_.erase(x_window);
}

void DispatcherLinux::AddDispatcherForRootWindow(
    MessageLoop::Dispatcher* dispatcher) {
  DCHECK(std::find(root_window_dispatchers_.begin(),
                   root_window_dispatchers_.end(),
                   dispatcher) ==
         root_window_dispatchers_.end());
  root_window_dispatchers_.push_back(dispatcher);
}

void DispatcherLinux::RemoveDispatcherForRootWindow(
    MessageLoop::Dispatcher* dispatcher) {
  root_window_dispatchers_.erase(
      std::remove(root_window_dispatchers_.begin(),
                  root_window_dispatchers_.end(),
                  dispatcher));
}

bool DispatcherLinux::Dispatch(const base::NativeEvent& xev) {
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
  if (xev->xany.window == x_root_window_) {
    for (Dispatchers::const_iterator it = root_window_dispatchers_.begin();
         it != root_window_dispatchers_.end();
         ++it) {
      (*it)->Dispatch(xev);
    }
    return true;
  }
  MessageLoop::Dispatcher* dispatcher = GetDispatcherForXEvent(xev);
  return dispatcher ? dispatcher->Dispatch(xev) : true;
}

base::EventStatus DispatcherLinux::WillProcessEvent(
    const base::NativeEvent& event) {
  PreprocessXEvent(event);
  return base::EVENT_CONTINUE;
}

void DispatcherLinux::DidProcessEvent(const base::NativeEvent& event) {
}

MessageLoop::Dispatcher* DispatcherLinux::GetDispatcherForXEvent(
    XEvent* xev) const {
  ::Window x_window = xev->xany.window;
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    x_window = xievent->event;
  }
  DispatchersMap::const_iterator it = dispatchers_.find(x_window);
  return it != dispatchers_.end() ? it->second : NULL;
}

MessageLoop::Dispatcher* CreateDispatcher() {
  return new DispatcherLinux;
}

}  // namespace aura

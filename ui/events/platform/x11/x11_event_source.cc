// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source.h"

#include <X11/extensions/XInput2.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "base/logging.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_hotplug_event_handler.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

int g_xinput_opcode = -1;

bool InitializeXInput2(XDisplay* display) {
  if (!display)
    return false;

  int event, err;

  int xiopcode;
  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    DVLOG(1) << "X Input extension not available.";
    return false;
  }
  g_xinput_opcode = xiopcode;

  int major = 2, minor = 2;
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    DVLOG(1) << "XInput2 not supported in the server.";
    return false;
  }
  if (major < 2 || (major == 2 && minor < 2)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2.2 is required.";
    return false;
  }

  return true;
}

bool InitializeXkb(XDisplay* display) {
  if (!display)
    return false;

  int opcode, event, error;
  int major = XkbMajorVersion;
  int minor = XkbMinorVersion;
  if (!XkbQueryExtension(display, &opcode, &event, &error, &major, &minor)) {
    DVLOG(1) << "Xkb extension not available.";
    return false;
  }

  // Ask the server not to send KeyRelease event when the user holds down a key.
  // crbug.com/138092
  Bool supported_return;
  if (!XkbSetDetectableAutoRepeat(display, True, &supported_return)) {
    DVLOG(1) << "XKB not supported in the server.";
    return false;
  }

  return true;
}

}  // namespace

X11EventSource::X11EventSource(XDisplay* display)
    : display_(display),
      continue_stream_(true) {
  CHECK(display_);
  DeviceDataManagerX11::CreateInstance();
  InitializeXInput2(display_);
  InitializeXkb(display_);
}

X11EventSource::~X11EventSource() {
}

// static
X11EventSource* X11EventSource::GetInstance() {
  return static_cast<X11EventSource*>(PlatformEventSource::GetInstance());
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, public

void X11EventSource::DispatchXEvents() {
  DCHECK(display_);
  // Handle all pending events.
  // It may be useful to eventually align this event dispatch with vsync, but
  // not yet.
  continue_stream_ = true;
  while (XPending(display_) && continue_stream_) {
    XEvent xevent;
    XNextEvent(display_, &xevent);
    ExtractCookieDataDispatchEvent(&xevent);
  }
}

void X11EventSource::BlockUntilWindowMapped(XID window) {
  XEvent event;
  do {
    // Block until there's a message of |event_mask| type on |w|. Then remove
    // it from the queue and stuff it in |event|.
    XWindowEvent(display_, window, StructureNotifyMask, &event);
    ExtractCookieDataDispatchEvent(&event);
  } while (event.type != MapNotify);
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, private

uint32_t X11EventSource::ExtractCookieDataDispatchEvent(XEvent* xevent) {
  bool have_cookie = false;
  if (xevent->type == GenericEvent &&
      XGetEventData(xevent->xgeneric.display, &xevent->xcookie)) {
    have_cookie = true;
  }
  uint32_t action = DispatchEvent(xevent);
  if (have_cookie)
    XFreeEventData(xevent->xgeneric.display, &xevent->xcookie);
  return action;
}

uint32_t X11EventSource::DispatchEvent(XEvent* xevent) {
  uint32_t action = PlatformEventSource::DispatchEvent(xevent);
  if (xevent->type == GenericEvent &&
      (xevent->xgeneric.evtype == XI_HierarchyChanged ||
       xevent->xgeneric.evtype == XI_DeviceChanged)) {
    ui::UpdateDeviceList();
    hotplug_event_handler_->OnHotplugEvent();
  }

  if (xevent->type == EnterNotify &&
      xevent->xcrossing.detail != NotifyInferior &&
      xevent->xcrossing.mode != NotifyUngrab) {
    // Clear stored scroll data
    ui::DeviceDataManagerX11::GetInstance()->InvalidateScrollClasses();
  }
  return action;
}

void X11EventSource::StopCurrentEventStream() {
  continue_stream_ = false;
}

void X11EventSource::OnDispatcherListChanged() {
  if (!hotplug_event_handler_) {
    hotplug_event_handler_.reset(new X11HotplugEventHandler());
    // Force the initial device query to have an update list of active devices.
    hotplug_event_handler_->OnHotplugEvent();
  }
}

}  // namespace ui

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_ozone.h"

#include <X11/Xlib.h>

#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

X11WindowOzone::X11WindowOzone(X11EventSourceLibevent* event_source,
                               PlatformWindowDelegate* delegate)
    : X11WindowBase(delegate), event_source_(event_source) {
  DCHECK(event_source_);
  event_source_->AddPlatformEventDispatcher(this);
  event_source_->AddXEventDispatcher(this);
}

X11WindowOzone::~X11WindowOzone() {
  event_source_->RemovePlatformEventDispatcher(this);
  event_source_->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!IsEventForXWindow(*xev))
    return false;

  ProcessXWindowEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& event) {
  // TODO(kylechar): This is broken, there is no way to include XID of XWindow
  // in ui::Event. Fix or use similar hack to DrmWindowHost.
  return xwindow() != None;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& platform_event) {
  Event* event = static_cast<Event*>(platform_event);
  delegate()->DispatchEvent(event);
  return POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui

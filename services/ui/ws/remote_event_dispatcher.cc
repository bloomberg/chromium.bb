// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/remote_event_dispatcher.h"

#include "services/ui/ws/display.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/window_server.h"

namespace ui {
namespace ws {

RemoteEventDispatcherImpl::RemoteEventDispatcherImpl(WindowServer* server)
    : window_server_(server) {}

RemoteEventDispatcherImpl::~RemoteEventDispatcherImpl() {}

void RemoteEventDispatcherImpl::DispatchEvent(int64_t display_id,
                                              std::unique_ptr<ui::Event> event,
                                              const DispatchEventCallback& cb) {
  DisplayManager* manager = window_server_->display_manager();
  if (!manager) {
    DVLOG(1) << "No display manager in DispatchEvent.";
    cb.Run(false);
    return;
  }

  Display* display = manager->GetDisplayById(display_id);
  if (!display) {
    DVLOG(1) << "Invalid display_id in DispatchEvent.";
    cb.Run(false);
    return;
  }

  if (event->IsLocatedEvent()) {
    LocatedEvent* located_event = event->AsLocatedEvent();
    if (located_event->root_location_f() != located_event->location_f()) {
      DVLOG(1) << "RemoteEventDispatcher::DispatchEvent locations must match";
      cb.Run(false);
      return;
    }

    // If this is a mouse pointer event, then we have to also update the
    // location of the cursor on the screen.
    if (event->IsMousePointerEvent())
      display->platform_display()->MoveCursorTo(located_event->location());
  }
  ignore_result(static_cast<PlatformDisplayDelegate*>(display)
                    ->GetEventSink()
                    ->OnEventFromSource(event.get()));
  cb.Run(true);
}

}  // namespace ws
}  // namespace ui

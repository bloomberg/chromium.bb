// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source.h"

#include <X11/Xlib.h>

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_libevent.h"

namespace ui {

namespace {

class X11EventSourceLibevent : public X11EventSource,
                               public base::MessagePumpLibevent::Watcher {
 public:
  explicit X11EventSourceLibevent(XDisplay* display)
      : X11EventSource(display) {
    int fd = ConnectionNumber(display);
    base::MessageLoopForUI::current()->WatchFileDescriptor(fd, true,
        base::MessagePumpLibevent::WATCH_READ, &watcher_controller_, this);
  }

  virtual ~X11EventSourceLibevent() {
  }

 private:
  // base::MessagePumpLibevent::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    DispatchXEvents();
  }

  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {
    NOTREACHED();
  }

  base::MessagePumpLibevent::FileDescriptorWatcher watcher_controller_;

  DISALLOW_COPY_AND_ASSIGN(X11EventSourceLibevent);
};

}  // namespace

scoped_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
  return scoped_ptr<PlatformEventSource>(
      new X11EventSourceLibevent(gfx::GetXDisplay()));
}

}  // namespace ui

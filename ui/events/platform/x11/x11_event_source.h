// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/events_export.h"
#include "ui/gfx/x/x11_types.h"

using Time = unsigned long;
using XEvent = union _XEvent;
using XID = unsigned long;
using XWindow = unsigned long;

namespace gfx {
class Point;
}

namespace ui {

class X11HotplugEventHandler;
class XScopedEventSelector;

// Responsible for notifying X11EventSource when new XEvents are available and
// processing/dispatching XEvents. Implementations will likely be a
// PlatformEventSource.
class X11EventSourceDelegate {
 public:
  X11EventSourceDelegate() = default;

  // Processes (if necessary) and handles dispatching XEvents.
  virtual void ProcessXEvent(XEvent* xevent) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(X11EventSourceDelegate);
};

// Receives X11 events and sends them to X11EventSourceDelegate. Handles
// receiving, pre-process and post-processing XEvents.
class EVENTS_EXPORT X11EventSource {
 public:
  X11EventSource(X11EventSourceDelegate* delegate, XDisplay* display);
  ~X11EventSource();

  static bool HasInstance();

  static X11EventSource* GetInstance();

  // Called when there is a new XEvent available. Processes all (if any)
  // available X events.
  void DispatchXEvents();

  // Dispatches a given event immediately. This is to facilitate sequential
  // interaction between the gtk event loop (used for IME) and the
  // main X11 event loop.
  void DispatchXEventNow(XEvent* event);

  XDisplay* display() { return display_; }

  // Returns the timestamp of the event currently being dispatched.  Falls back
  // on GetCurrentServerTime() if there's no event being dispatched, or if the
  // current event does not have a timestamp.
  Time GetTimestamp();

  // Returns the root pointer location only if there is an event being
  // dispatched that contains that information.
  base::Optional<gfx::Point> GetRootCursorLocationFromCurrentEvent() const;

  void StopCurrentEventStream();
  void OnDispatcherListChanged();

 protected:
  // Extracts cookie data from |xevent| if it's of GenericType, and dispatches
  // the event. This function also frees up the cookie data after dispatch is
  // complete.
  void ExtractCookieDataDispatchEvent(XEvent* xevent);

  // Handles updates after event has been dispatched.
  void PostDispatchEvent(XEvent* xevent);

  // Explicitly asks the X11 server for the current timestamp, and updates
  // |last_seen_server_time_| with this value.
  Time GetCurrentServerTime();

 private:
  static X11EventSource* instance_;

  X11EventSourceDelegate* delegate_;

  // The connection to the X11 server used to receive the events.
  XDisplay* display_;

  // Event currently being dispatched.
  XEvent* dispatching_event_;

  // State necessary for UpdateLastSeenServerTime
  bool dummy_initialized_;
  XWindow dummy_window_;
  XAtom dummy_atom_;
  std::unique_ptr<XScopedEventSelector> dummy_window_events_;

  // Keeps track of whether this source should continue to dispatch all the
  // available events.
  bool continue_stream_ = true;

  std::unique_ptr<X11HotplugEventHandler> hotplug_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(X11EventSource);
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_

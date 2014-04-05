// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_PLATFORM_EVENT_SOURCE_H_
#define UI_EVENTS_PLATFORM_PLATFORM_EVENT_SOURCE_H_

#include <map>
#include <vector>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/events/events_export.h"
#include "ui/events/platform/platform_event_types.h"

namespace ui {

class Event;
class PlatformEventDispatcher;
class PlatformEventObserver;
class ScopedEventDispatcher;

// PlatformEventSource receives events from a source and dispatches the events
// to the appropriate dispatchers.
class EVENTS_EXPORT PlatformEventSource {
 public:
  virtual ~PlatformEventSource();

  static PlatformEventSource* GetInstance();

  void AddPlatformEventDispatcher(PlatformEventDispatcher* dispatcher);
  void RemovePlatformEventDispatcher(PlatformEventDispatcher* dispatcher);

  // Installs a PlatformEventDispatcher that receives all the events. The
  // dispatcher can process the event, or request that the default dispatchers
  // be invoked by setting |POST_DISPATCH_PERFORM_DEFAULT| flag from the
  // |DispatchEvent()| override.
  // The returned |ScopedEventDispatcher| object is a handler for the overridden
  // dispatcher. When this handler is destroyed, it removes the overridden
  // dispatcher, and restores the previous override-dispatcher (or NULL if there
  // wasn't any).
  scoped_ptr<ScopedEventDispatcher> OverrideDispatcher(
      PlatformEventDispatcher* dispatcher);

  void AddPlatformEventObserver(PlatformEventObserver* observer);
  void RemovePlatformEventObserver(PlatformEventObserver* observer);

  static scoped_ptr<PlatformEventSource> CreateDefault();

 protected:
  PlatformEventSource();

  // Dispatches |platform_event| to the dispatchers. If there is an override
  // dispatcher installed using |OverrideDispatcher()|, then that dispatcher
  // receives the event first. |POST_DISPATCH_QUIT_LOOP| flag is set in the
  // returned value if the event-source should stop dispatching events at the
  // current message-loop iteration.
  virtual uint32_t DispatchEvent(PlatformEvent platform_event);

 private:
  friend class ScopedEventDispatcher;
  static PlatformEventSource* instance_;

  void OnOverriddenDispatcherRestored();

  // Invokes the corresponding methods on the PlatformEventObservers added to
  // the event-source.
  // Returns true from |WillProcessEvent()| if any of the observers in the list
  // consumes the event and returns true from |WillProcessEvent()|.
  bool WillProcessEvent(PlatformEvent platform_event);
  void DidProcessEvent(PlatformEvent platform_event);

  typedef std::vector<PlatformEventDispatcher*> PlatformEventDispatcherList;
  PlatformEventDispatcherList dispatchers_;
  PlatformEventDispatcher* overridden_dispatcher_;

  // Used to keep track of whether the current override-dispatcher has been
  // reset and a previous override-dispatcher has been restored.
  bool overridden_dispatcher_restored_;

  ObserverList<PlatformEventObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PlatformEventSource);
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_PLATFORM_EVENT_SOURCE_H_

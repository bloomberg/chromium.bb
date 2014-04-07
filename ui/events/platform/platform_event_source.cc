// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/platform_event_source.h"

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/scoped_event_dispatcher.h"

namespace ui {

// static
PlatformEventSource* PlatformEventSource::instance_ = NULL;

PlatformEventSource::PlatformEventSource()
    : overridden_dispatcher_(NULL),
      overridden_dispatcher_restored_(false) {
  CHECK(!instance_) << "Only one platform event source can be created.";
  instance_ = this;
}

PlatformEventSource::~PlatformEventSource() {
  CHECK_EQ(this, instance_);
  instance_ = NULL;
}

PlatformEventSource* PlatformEventSource::GetInstance() { return instance_; }

void PlatformEventSource::AddPlatformEventDispatcher(
    PlatformEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  DCHECK(std::find(dispatchers_.begin(), dispatchers_.end(), dispatcher) ==
         dispatchers_.end());
  dispatchers_.push_back(dispatcher);
}

void PlatformEventSource::RemovePlatformEventDispatcher(
    PlatformEventDispatcher* dispatcher) {
  PlatformEventDispatcherList::iterator remove =
      std::remove(dispatchers_.begin(), dispatchers_.end(), dispatcher);
  if (remove != dispatchers_.end())
    dispatchers_.erase(remove);
}

scoped_ptr<ScopedEventDispatcher> PlatformEventSource::OverrideDispatcher(
    PlatformEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  overridden_dispatcher_restored_ = false;
  return make_scoped_ptr(
      new ScopedEventDispatcher(&overridden_dispatcher_, dispatcher));
}

void PlatformEventSource::AddPlatformEventObserver(
    PlatformEventObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PlatformEventSource::RemovePlatformEventObserver(
    PlatformEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

uint32_t PlatformEventSource::DispatchEvent(PlatformEvent platform_event) {
  uint32_t action = POST_DISPATCH_PERFORM_DEFAULT;
  bool should_quit = false;

  FOR_EACH_OBSERVER(PlatformEventObserver, observers_,
                    WillProcessEvent(platform_event));
  // Give the overridden dispatcher a chance to dispatch the event first.
  if (overridden_dispatcher_)
    action = overridden_dispatcher_->DispatchEvent(platform_event);
  should_quit = !!(action & POST_DISPATCH_QUIT_LOOP);

  if (action & POST_DISPATCH_PERFORM_DEFAULT) {
    for (PlatformEventDispatcherList::iterator i = dispatchers_.begin();
         i != dispatchers_.end();
         ++i) {
      PlatformEventDispatcher* dispatcher = *(i);
      if (dispatcher->CanDispatchEvent(platform_event))
        action = dispatcher->DispatchEvent(platform_event);
      if (action & POST_DISPATCH_QUIT_LOOP)
        should_quit = true;
      if (action & POST_DISPATCH_STOP_PROPAGATION)
        break;
    }
  }
  FOR_EACH_OBSERVER(PlatformEventObserver, observers_,
                    DidProcessEvent(platform_event));

  // Terminate the message-loop if the dispatcher requested for it.
  if (should_quit) {
    base::MessageLoop::current()->QuitNow();
    action |= POST_DISPATCH_QUIT_LOOP;
  }

  // If an overridden dispatcher has been destroyed, then the platform
  // event-source should halt dispatching the current stream of events, and wait
  // until the next message-loop iteration for dispatching events. This lets any
  // nested message-loop to unwind correctly and any new dispatchers to receive
  // the correct sequence of events.
  if (overridden_dispatcher_restored_)
    action |= POST_DISPATCH_QUIT_LOOP;

  overridden_dispatcher_restored_ = false;

  return action;
}

void PlatformEventSource::OnOverriddenDispatcherRestored() {
  CHECK(overridden_dispatcher_);
  overridden_dispatcher_restored_ = true;
}

}  // namespace ui

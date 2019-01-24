// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_SOURCE_H_
#define UI_EVENTS_EVENT_SOURCE_H_

#include <vector>

#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;
class EventSink;

// EventSource receives events from the native platform (e.g. X11, win32 etc.)
// and sends the events to an EventSink.
class EVENTS_EXPORT EventSource {
 public:
  EventSource();
  virtual ~EventSource();

  virtual EventSink* GetEventSink() = 0;

  // Adds a rewriter to modify events before they are sent to the
  // EventSink. The rewriter must be explicitly removed from the
  // EventSource before the rewriter is destroyed. The EventSource
  // does not take ownership of the rewriter.
  void AddEventRewriter(EventRewriter* rewriter);
  void RemoveEventRewriter(EventRewriter* rewriter);

  // Sends the event through all rewriters and onto the source's EventSink.
  EventDispatchDetails SendEventToSink(const Event* event);

  // Send the event to the sink after rewriting; subclass overrides may queue
  // events before delivery, i.e. for the WindowService.
  virtual EventDispatchDetails DeliverEventToSink(Event* event);

 protected:
  // Sends the event through the rewriters and onto the source's EventSink.
  // If |rewriter| is valid, |event| is only sent to the subsequent rewriters.
  // This is used for asynchronous reposting of events processed by |rewriter|.
  // TODO(kpschoedel): Remove along with old EventRewriter API.
  EventDispatchDetails SendEventToSinkFromRewriter(
      const Event* event,
      const EventRewriter* rewriter);

 private:
  // Implementation of EventRewriter::Continuation. No details need to be
  // visible outside of event_source.cc.
  class EventRewriterContinuation;

  // The role of an EventRewriter::Continuation is to propagate events
  // out from one EventRewriter and (optionally) on to the next. Using
  // the |base::LinkedList| container makes this simple, and also allows
  // the container to hold the continuation directly without risk of
  // invalidating WeakPtrs.
  typedef base::LinkedList<EventRewriterContinuation> EventRewriterList;

  // Returns the EventRewriterContinuation for a given EventRewriter,
  // or nullptr if the rewriter is not registered with this EventSource.
  EventRewriterContinuation* GetContinuation(
      const EventRewriter* rewriter) const;

  EventRewriterList rewriter_list_;

  friend class EventRewriter;  // TODO(kpschoedel): Remove along with old API.
  friend class EventRewriterContinuation;
  friend class EventSourceTestApi;

  DISALLOW_COPY_AND_ASSIGN(EventSource);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_SOURCE_H_

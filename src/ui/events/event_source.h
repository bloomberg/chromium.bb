// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_SOURCE_H_
#define UI_EVENTS_EVENT_SOURCE_H_

#include <list>
#include <memory>
#include <vector>

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
  // Implementation of EventRewriterContinuation. No details need to be
  // visible outside of event_source.cc.
  class EventRewriterContinuationImpl;

  // It's sufficient to have one EventRewriterContinuationImpl for each
  // registered EventRewriter, so a list of them also serves as a list
  // of registered rewriters.
  typedef std::list<std::unique_ptr<EventRewriterContinuationImpl>>
      EventRewriterList;

  // Returns the EventRewriterContinuation for a given EventRewriter,
  // or |rewriter_list_.end()| if the rewriter is not registered.
  EventRewriterList::iterator FindContinuation(const EventRewriter* rewriter);

  EventRewriterList rewriter_list_;

  friend class EventRewriter;  // TODO(kpschoedel): Remove along with old API.
  friend class EventSourceTestApi;

  DISALLOW_COPY_AND_ASSIGN(EventSource);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_SOURCE_H_

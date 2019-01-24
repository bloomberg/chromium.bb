// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_source.h"

#include <memory>

#include "ui/events/event_sink.h"

namespace ui {

namespace {

bool IsLocatedEventWithDifferentLocations(const Event& event) {
  if (!event.IsLocatedEvent())
    return false;
  const LocatedEvent* located_event = event.AsLocatedEvent();
  return located_event->target() &&
         located_event->location_f() != located_event->root_location_f();
}

}  // namespace

class EventSource::EventRewriterContinuation
    : public EventRewriter::Continuation,
      public base::LinkNode<EventRewriterContinuation> {
 public:
  EventRewriterContinuation(EventRewriter* rewriter,
                            EventSource* const source,
                            EventSource::EventRewriterList* const rewriter_list)
      : rewriter_(rewriter),
        source_(source),
        rewriter_list_(rewriter_list),
        weak_ptr_factory_(this) {}
  ~EventRewriterContinuation() override {}

  EventDispatchDetails SendEvent(const Event* event) override {
    if (this == rewriter_list_->tail())
      return SendEventFinally(event);
    return next()->value()->rewriter_->RewriteEvent(
        *event, next()->value()->GetWeakPtr());
  }

  EventDispatchDetails SendEventFinally(const Event* event) override {
    return source_->DeliverEventToSink(const_cast<Event*>(event));
  }

  EventDispatchDetails DiscardEvent() override {
    ui::EventDispatchDetails details;
    details.event_discarded = true;
    return details;
  }

  EventRewriter* rewriter() const { return rewriter_; }

  base::WeakPtr<EventRewriterContinuation> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  EventRewriter* rewriter_;
  EventSource* const source_;
  EventSource::EventRewriterList* const rewriter_list_;

  base::WeakPtrFactory<EventRewriterContinuation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(EventRewriterContinuation);
};

EventSource::EventSource() {}

EventSource::~EventSource() {
  while (!rewriter_list_.empty())
    RemoveEventRewriter(rewriter_list_.head()->value()->rewriter());
}

void EventSource::AddEventRewriter(EventRewriter* rewriter) {
  DCHECK(rewriter);
  DCHECK(!GetContinuation(rewriter));
  rewriter_list_.Append(
      new EventRewriterContinuation(rewriter, this, &rewriter_list_));
}

void EventSource::RemoveEventRewriter(EventRewriter* rewriter) {
  EventRewriterContinuation* continuation = GetContinuation(rewriter);
  if (!continuation) {
    // We need to tolerate attempting to remove an unregistered
    // EventRewriter, because many unit tests currently do so:
    // the rewriter gets added to the current root window source
    // on construction, and removed from the current root window
    // source on destruction, but the root window changes in
    // between.
    LOG(WARNING) << "EventRewriter not registered";
    return;
  }
  continuation->RemoveFromList();
  delete continuation;
}

EventDispatchDetails EventSource::SendEventToSink(const Event* event) {
  return SendEventToSinkFromRewriter(event, nullptr);
}

EventDispatchDetails EventSource::DeliverEventToSink(Event* event) {
  EventSink* sink = GetEventSink();
  CHECK(sink);
  return sink->OnEventFromSource(event);
}

EventDispatchDetails EventSource::SendEventToSinkFromRewriter(
    const Event* event,
    const EventRewriter* rewriter) {
  std::unique_ptr<ui::Event> event_for_rewriting_ptr;
  const Event* event_for_rewriting = event;
  if (!rewriter_list_.empty() && IsLocatedEventWithDifferentLocations(*event)) {
    // EventRewriters don't expect an event with differing location and
    // root-location (because they don't honor the target). Provide such an
    // event.
    event_for_rewriting_ptr = ui::Event::Clone(*event);
    event_for_rewriting_ptr->AsLocatedEvent()->set_location_f(
        event_for_rewriting_ptr->AsLocatedEvent()->root_location_f());
    event_for_rewriting = event_for_rewriting_ptr.get();
  }
  EventRewriterContinuation* continuation = nullptr;
  if (rewriter) {
    // If a rewriter reposted |event|, only send it to subsequent rewriters.
    continuation = GetContinuation(rewriter);
    CHECK(continuation);
    continuation = continuation->next()->value();
  } else {
    // Otherwise, start with the first rewriter.
    // (If the list is empty, head() == end().)
    continuation = rewriter_list_.head()->value();
  }
  if (continuation == rewriter_list_.end() || continuation == nullptr)
    return DeliverEventToSink(const_cast<Event*>(event));
  return continuation->rewriter()->RewriteEvent(*event_for_rewriting,
                                                continuation->GetWeakPtr());
}

EventSource::EventRewriterContinuation* EventSource::GetContinuation(
    const EventRewriter* rewriter) const {
  for (auto* it = rewriter_list_.head(); it != rewriter_list_.end();
       it = it->next()) {
    if (it->value()->rewriter() == rewriter) {
      return it->value();
    }
  }
  return nullptr;
}

}  // namespace ui

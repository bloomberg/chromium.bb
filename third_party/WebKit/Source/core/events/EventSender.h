/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventSender_h
#define EventSender_h

#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

template <typename T>
class EventSender final : public GarbageCollectedFinalized<EventSender<T>> {
  WTF_MAKE_NONCOPYABLE(EventSender);

 public:
  static EventSender* Create(const AtomicString& event_type) {
    return new EventSender(event_type);
  }

  const AtomicString& EventType() const { return event_type_; }
  void DispatchEventSoon(T*);
  void CancelEvent(T*);
  void DispatchPendingEvents();

#if DCHECK_IS_ON()
  bool HasPendingEvents(T* sender) const {
    return dispatch_soon_list_.Find(sender) != kNotFound ||
           dispatching_list_.Find(sender) != kNotFound;
  }
#endif

  DEFINE_INLINE_TRACE() {
    visitor->Trace(dispatch_soon_list_);
    visitor->Trace(dispatching_list_);
  }

 private:
  explicit EventSender(const AtomicString& event_type);

  void TimerFired(TimerBase*) { DispatchPendingEvents(); }

  AtomicString event_type_;
  Timer<EventSender<T>> timer_;
  HeapVector<Member<T>> dispatch_soon_list_;
  HeapVector<Member<T>> dispatching_list_;
};

template <typename T>
EventSender<T>::EventSender(const AtomicString& event_type)
    : event_type_(event_type), timer_(this, &EventSender::TimerFired) {}

template <typename T>
void EventSender<T>::DispatchEventSoon(T* sender) {
  dispatch_soon_list_.push_back(sender);
  if (!timer_.IsActive())
    timer_.StartOneShot(0, BLINK_FROM_HERE);
}

template <typename T>
void EventSender<T>::CancelEvent(T* sender) {
  // Remove instances of this sender from both lists.
  // Use loops because we allow multiple instances to get into the lists.
  for (auto& sender_in_list : dispatch_soon_list_) {
    if (sender_in_list == sender)
      sender_in_list = nullptr;
  }
  for (auto& sender_in_list : dispatching_list_) {
    if (sender_in_list == sender)
      sender_in_list = nullptr;
  }
}

template <typename T>
void EventSender<T>::DispatchPendingEvents() {
  // Need to avoid re-entering this function; if new dispatches are
  // scheduled before the parent finishes processing the list, they
  // will set a timer and eventually be processed.
  if (!dispatching_list_.IsEmpty())
    return;

  timer_.Stop();

  dispatching_list_.Swap(dispatch_soon_list_);
  for (auto& sender_in_list : dispatching_list_) {
    if (T* sender = sender_in_list) {
      sender_in_list = nullptr;
      sender->DispatchPendingEvent(this);
    }
  }
  dispatching_list_.Clear();
}

}  // namespace blink

#endif  // EventSender_h

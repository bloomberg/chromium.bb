/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GenericEventQueue_h
#define GenericEventQueue_h

#include "core/CoreExport.h"
#include "core/dom/events/EventQueue.h"
#include "core/dom/events/EventTarget.h"
#include "platform/Timer.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class CORE_EXPORT GenericEventQueue final : public EventQueue {
 public:
  static GenericEventQueue* Create(EventTarget*);
  ~GenericEventQueue() override;

  // EventQueue
  DECLARE_VIRTUAL_TRACE();
  bool EnqueueEvent(const WebTraceLocation&, Event*) override;
  bool CancelEvent(Event*) override;
  void Close() override;

  void CancelAllEvents();
  bool HasPendingEvents() const;

 private:
  explicit GenericEventQueue(EventTarget*);
  void TimerFired(TimerBase*);

  Member<EventTarget> owner_;
  HeapVector<Member<Event>> pending_events_;
  Timer<GenericEventQueue> timer_;

  bool is_closed_;
};

}  // namespace blink

#endif

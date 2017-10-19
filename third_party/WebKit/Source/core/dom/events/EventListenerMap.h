/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 *
 */

#ifndef EventListenerMap_h
#define EventListenerMap_h

#include "core/CoreExport.h"
#include "core/dom/events/AddEventListenerOptionsResolved.h"
#include "core/dom/events/EventListenerOptions.h"
#include "core/events/RegisteredEventListener.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class EventTarget;

using EventListenerVector = HeapVector<RegisteredEventListener, 1>;

class CORE_EXPORT EventListenerMap {
  WTF_MAKE_NONCOPYABLE(EventListenerMap);
  DISALLOW_NEW();

 public:
  EventListenerMap();

  bool IsEmpty() const { return entries_.IsEmpty(); }
  bool Contains(const AtomicString& event_type) const;
  bool ContainsCapturing(const AtomicString& event_type) const;

  void Clear();
  bool Add(const AtomicString& event_type,
           EventListener*,
           const AddEventListenerOptionsResolved&,
           RegisteredEventListener* registered_listener);
  bool Remove(const AtomicString& event_type,
              const EventListener*,
              const EventListenerOptions&,
              size_t* index_of_removed_listener,
              RegisteredEventListener* registered_listener);
  EventListenerVector* Find(const AtomicString& event_type);
  Vector<AtomicString> EventTypes() const;

  void CopyEventListenersNotCreatedFromMarkupToTarget(EventTarget*);

  void Trace(blink::Visitor*);
  DECLARE_TRACE_WRAPPERS();

 private:
  friend class EventListenerIterator;

  void CheckNoActiveIterators();

  // We use HeapVector instead of HeapHashMap because
  //  - HeapVector is much more space efficient than HeapHashMap.
  //  - An EventTarget rarely has event listeners for many event types, and
  //    HeapVector is faster in such cases.
  HeapVector<std::pair<AtomicString, Member<EventListenerVector>>, 2> entries_;

#if DCHECK_IS_ON()
  int active_iterator_count_ = 0;
#endif
};

#if !DCHECK_IS_ON()
inline void EventListenerMap::CheckNoActiveIterators() {}
#endif

}  // namespace blink

#endif  // EventListenerMap_h

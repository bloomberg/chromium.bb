/*
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IdTargetObserverRegistry_h
#define IdTargetObserverRegistry_h

#include "base/macros.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class IdTargetObserver;

class IdTargetObserverRegistry final
    : public GarbageCollected<IdTargetObserverRegistry> {
  friend class IdTargetObserver;

 public:
  static IdTargetObserverRegistry* Create();
  void Trace(blink::Visitor*);
  void NotifyObservers(const AtomicString& id);
  bool HasObservers(const AtomicString& id) const;

 private:
  IdTargetObserverRegistry() : notifying_observers_in_set_(nullptr) {}
  void AddObserver(const AtomicString& id, IdTargetObserver*);
  void RemoveObserver(const AtomicString& id, IdTargetObserver*);
  void NotifyObserversInternal(const AtomicString& id);

  typedef HeapHashSet<Member<IdTargetObserver>> ObserverSet;
  typedef HeapHashMap<StringImpl*, Member<ObserverSet>> IdToObserverSetMap;
  IdToObserverSetMap registry_;
  Member<ObserverSet> notifying_observers_in_set_;
  DISALLOW_COPY_AND_ASSIGN(IdTargetObserverRegistry);
};

inline void IdTargetObserverRegistry::NotifyObservers(const AtomicString& id) {
  DCHECK(!notifying_observers_in_set_);
  if (id.IsEmpty() || registry_.IsEmpty())
    return;
  IdTargetObserverRegistry::NotifyObserversInternal(id);
}

}  // namespace blink

#endif  // IdTargetObserverRegistry_h

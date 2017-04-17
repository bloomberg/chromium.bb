/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScopedEventQueue_h
#define ScopedEventQueue_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class EventDispatchMediator;

class CORE_EXPORT ScopedEventQueue {
  WTF_MAKE_NONCOPYABLE(ScopedEventQueue);
  USING_FAST_MALLOC(ScopedEventQueue);

 public:
  ~ScopedEventQueue();

  void EnqueueEventDispatchMediator(EventDispatchMediator*);
  void DispatchAllEvents();
  static ScopedEventQueue* Instance();

  void IncrementScopingLevel();
  void DecrementScopingLevel();
  bool ShouldQueueEvents() const { return scoping_level_ > 0; }

 private:
  ScopedEventQueue();
  static void Initialize();
  void DispatchEvent(EventDispatchMediator*) const;

  PersistentHeapVector<Member<EventDispatchMediator>>
      queued_event_dispatch_mediators_;
  unsigned scoping_level_;

  static ScopedEventQueue* instance_;
};

class EventQueueScope {
  WTF_MAKE_NONCOPYABLE(EventQueueScope);
  STACK_ALLOCATED();

 public:
  EventQueueScope() { ScopedEventQueue::Instance()->IncrementScopingLevel(); }
  ~EventQueueScope() { ScopedEventQueue::Instance()->DecrementScopingLevel(); }
};

}  // namespace blink

#endif  // ScopedEventQueue_h

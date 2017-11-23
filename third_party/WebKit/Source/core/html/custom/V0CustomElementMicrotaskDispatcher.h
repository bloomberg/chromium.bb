// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementMicrotaskDispatcher_h
#define V0CustomElementMicrotaskDispatcher_h

#include "base/macros.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class V0CustomElementCallbackQueue;

class V0CustomElementMicrotaskDispatcher final
    : public GarbageCollected<V0CustomElementMicrotaskDispatcher> {
 public:
  static V0CustomElementMicrotaskDispatcher& Instance();

  void Enqueue(V0CustomElementCallbackQueue*);

  bool ElementQueueIsEmpty() { return elements_.IsEmpty(); }

  void Trace(blink::Visitor*);

 private:
  V0CustomElementMicrotaskDispatcher();

  void EnsureMicrotaskScheduledForElementQueue();
  void EnsureMicrotaskScheduled();

  static void Dispatch();
  void DoDispatch();

  bool has_scheduled_microtask_;
  enum { kQuiescent, kResolving, kDispatchingCallbacks } phase_;

  HeapVector<Member<V0CustomElementCallbackQueue>> elements_;

  DISALLOW_COPY_AND_ASSIGN(V0CustomElementMicrotaskDispatcher);
};

}  // namespace blink

#endif  // V0CustomElementMicrotaskDispatcher_h

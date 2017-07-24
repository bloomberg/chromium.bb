// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserverDelegate_h
#define IntersectionObserverDelegate_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class IntersectionObserver;
class IntersectionObserverEntry;

class IntersectionObserverDelegate
    : public GarbageCollectedFinalized<IntersectionObserverDelegate>,
      public TraceWrapperBase {
 public:
  virtual ~IntersectionObserverDelegate() {}
  virtual void Deliver(const HeapVector<Member<IntersectionObserverEntry>>&,
                       IntersectionObserver&) = 0;
  virtual ExecutionContext* GetExecutionContext() const = 0;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {}
};

}  // namespace blink

#endif  // IntersectionObserverDelegate_h

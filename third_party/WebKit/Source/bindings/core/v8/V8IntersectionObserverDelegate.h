// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IntersectionObserverDelegate_h
#define V8IntersectionObserverDelegate_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/intersection_observer/IntersectionObserverDelegate.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

class V8IntersectionObserverCallback;

class V8IntersectionObserverDelegate final
    : public IntersectionObserverDelegate,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(V8IntersectionObserverDelegate);

 public:
  CORE_EXPORT V8IntersectionObserverDelegate(V8IntersectionObserverCallback*,
                                             ScriptState*);
  ~V8IntersectionObserverDelegate() override;

  ExecutionContext* GetExecutionContext() const override;

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>&,
               IntersectionObserver&) override;

 private:
  TraceWrapperMember<V8IntersectionObserverCallback> callback_;
};

}  // namespace blink
#endif  // V8IntersectionObserverDelegate_h

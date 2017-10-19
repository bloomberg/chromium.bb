// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IntersectionObserverDelegate_h
#define V8IntersectionObserverDelegate_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/intersection_observer/IntersectionObserverDelegate.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

class V8IntersectionObserverCallback;

class V8IntersectionObserverDelegate final
    : public IntersectionObserverDelegate {
 public:
  CORE_EXPORT V8IntersectionObserverDelegate(V8IntersectionObserverCallback*,
                                             ScriptState*);
  ~V8IntersectionObserverDelegate() override;

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>&,
               IntersectionObserver&) override;

  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContext::From(script_state_.get());
  }

 private:
  TraceWrapperMember<V8IntersectionObserverCallback> callback_;
  // TODO(bashi): Use ContextClient rather than holding ScriptState.
  RefPtr<ScriptState> script_state_;
};

}  // namespace blink
#endif  // V8IntersectionObserverDelegate_h

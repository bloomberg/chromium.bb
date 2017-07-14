// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IntersectionObserverCallback_h
#define V8IntersectionObserverCallback_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/intersection_observer/IntersectionObserverCallback.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScopedPersistent.h"

namespace blink {

class V8IntersectionObserverCallback final
    : public IntersectionObserverCallback {
 public:
  CORE_EXPORT V8IntersectionObserverCallback(v8::Local<v8::Function>,
                                             v8::Local<v8::Object>,
                                             ScriptState*);
  ~V8IntersectionObserverCallback() override;

  DECLARE_VIRTUAL_TRACE();

  void HandleEvent(const HeapVector<Member<IntersectionObserverEntry>>&,
                   IntersectionObserver&) override;

  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContext::From(script_state_.Get());
  }

 private:
  ScopedPersistent<v8::Function> callback_;
  RefPtr<ScriptState> script_state_;
};

}  // namespace blink
#endif  // V8IntersectionObserverCallback_h

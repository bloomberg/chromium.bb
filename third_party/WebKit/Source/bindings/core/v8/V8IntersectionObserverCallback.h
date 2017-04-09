// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IntersectionObserverCallback_h
#define V8IntersectionObserverCallback_h

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScopedPersistent.h"
#include "core/CoreExport.h"
#include "core/dom/IntersectionObserverCallback.h"

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
    return script_state_->GetExecutionContext();
  }

 private:
  ScopedPersistent<v8::Function> callback_;
  RefPtr<ScriptState> script_state_;
};

}  // namespace blink
#endif  // V8IntersectionObserverCallback_h

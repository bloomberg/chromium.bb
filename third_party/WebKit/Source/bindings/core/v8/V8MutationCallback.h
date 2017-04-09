/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8MutationCallback_h
#define V8MutationCallback_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/TraceWrapperV8Reference.h"
#include "core/dom/MutationCallback.h"
#include "platform/wtf/RefPtr.h"
#include "v8/include/v8.h"

namespace blink {

class ExecutionContext;

class V8MutationCallback final : public MutationCallback {
 public:
  static V8MutationCallback* Create(v8::Local<v8::Function> callback,
                                    v8::Local<v8::Object> owner,
                                    ScriptState* script_state) {
    return new V8MutationCallback(callback, owner, script_state);
  }
  ~V8MutationCallback() override;

  void Call(const HeapVector<Member<MutationRecord>>&,
            MutationObserver*) override;

  ExecutionContext* GetExecutionContext() const override {
    return script_state_->GetExecutionContext();
  }

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  V8MutationCallback(v8::Local<v8::Function>,
                     v8::Local<v8::Object>,
                     ScriptState*);

  TraceWrapperV8Reference<v8::Function> callback_;
  RefPtr<ScriptState> script_state_;
};

}  // namespace blink

#endif  // V8MutationCallback_h

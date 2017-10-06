// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackFunctionBase_h
#define CallbackFunctionBase_h

#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"

namespace blink {

// |CallbackFunctionBase| is a base class for callback function classes (e.g
// |V8FrameRequestCallback|) and has a reference to a V8 callback function.
class PLATFORM_EXPORT CallbackFunctionBase
    : public GarbageCollectedFinalized<CallbackFunctionBase>,
      public TraceWrapperBase {
 public:
  virtual ~CallbackFunctionBase() = default;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  v8::Local<v8::Function> v8Value(v8::Isolate* isolate) {
    return callback_.NewLocal(isolate);
  }

  v8::Isolate* GetIsolate() const { return script_state_->GetIsolate(); }

 protected:
  CallbackFunctionBase(ScriptState*, v8::Local<v8::Function>);
  RefPtr<ScriptState> script_state_;
  TraceWrapperV8Reference<v8::Function> callback_;
};

}  // namespace blink

#endif  // CallbackFunctionBase_h

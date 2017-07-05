/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/bindings/V8ObjectConstructor.h"

#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

v8::MaybeLocal<v8::Object> V8ObjectConstructor::NewInstance(
    v8::Isolate* isolate,
    v8::Local<v8::Function> function,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(!function.IsEmpty());
  TRACE_EVENT0("v8", "v8.newInstance");
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);
  ConstructorMode constructor_mode(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::MaybeLocal<v8::Object> result =
      function->NewInstance(isolate->GetCurrentContext(), argc, argv);
  CHECK(!isolate->IsDead());
  return result;
}

void V8ObjectConstructor::IsValidConstructorMode(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (ConstructorMode::Current(info.GetIsolate()) ==
      ConstructorMode::kCreateNewObject) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), "Illegal constructor");
    return;
  }
  V8SetReturnValue(info, info.Holder());
}

}  // namespace blink

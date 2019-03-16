// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_SCRIPT_FUNCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_SCRIPT_FUNCTION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

// A variant of ScriptFunction that avoids the conversion to and from a
// ScriptValue.
class CORE_EXPORT StreamScriptFunction : public ScriptFunction {
 public:
  explicit StreamScriptFunction(ScriptState* script_state);

  virtual void CallWithLocal(v8::Local<v8::Value>) = 0;

  // Exposed as public so that we don't need a boilerplate Create() function for
  // every subclass.
  using ScriptFunction::BindToV8Function;

 private:
  void CallRaw(const v8::FunctionCallbackInfo<v8::Value>&) final;
};

// A convenient wrapper for promise->Then() for when both paths are
// StreamScriptFunctions. It avoids having to call BindToV8Function()
// explicitly. If |on_rejected| is null then behaves like single-argument
// Then(). If |on_fulfilled| is null then it calls Catch().
v8::Local<v8::Promise> StreamThenPromise(
    v8::Local<v8::Context>,
    v8::Local<v8::Promise>,
    StreamScriptFunction* on_fulfilled,
    StreamScriptFunction* on_rejected = nullptr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_STREAM_SCRIPT_FUNCTION_H_

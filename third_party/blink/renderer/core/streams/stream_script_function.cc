// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/stream_script_function.h"

#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

StreamScriptFunction::StreamScriptFunction(ScriptState* script_state)
    : ScriptFunction(script_state) {}

void StreamScriptFunction::CallRaw(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 1);
  CallWithLocal(args[0]);
}

void StreamThenPromise(v8::Local<v8::Context> context,
                       v8::Local<v8::Promise> promise,
                       StreamScriptFunction* on_fulfilled,
                       StreamScriptFunction* on_rejected) {
  DCHECK(on_fulfilled);
  DCHECK(on_rejected);
  auto result = promise->Then(context, on_fulfilled->BindToV8Function(),
                              on_rejected->BindToV8Function());
  if (result.IsEmpty()) {
    DVLOG(3)
        << "assuming that failure of promise->Then() is caused by shutdown and"
           "ignoring it";
  }
}

}  // namespace blink

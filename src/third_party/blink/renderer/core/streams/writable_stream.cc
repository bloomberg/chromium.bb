// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"
#include "third_party/blink/renderer/core/streams/writable_stream_native.h"
#include "third_party/blink/renderer/core/streams/writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

WritableStream* WritableStream::Create(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return Create(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

WritableStream* WritableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_sink,
                                       ExceptionState& exception_state) {
  return Create(
      script_state, underlying_sink,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

WritableStream* WritableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_sink,
                                       ScriptValue strategy,
                                       ExceptionState& exception_state) {
  WritableStream* stream = nullptr;
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    stream = MakeGarbageCollected<WritableStreamNative>(
        script_state, underlying_sink, strategy, exception_state);
  } else {
    auto* stream_wrapper = MakeGarbageCollected<WritableStreamWrapper>();
    stream = stream_wrapper;
    stream_wrapper->Init(script_state, underlying_sink, strategy,
                         exception_state);
  }
  if (exception_state.HadException())
    return nullptr;

  return stream;
}

// static
WritableStream* WritableStream::Deserialize(ScriptState* script_state,
                                            MessagePort* port,
                                            ExceptionState& exception_state) {
  // TODO(ricea): Switch on Blink feature.
  return WritableStreamWrapper::Deserialize(script_state, port,
                                            exception_state);
}

}  // namespace blink

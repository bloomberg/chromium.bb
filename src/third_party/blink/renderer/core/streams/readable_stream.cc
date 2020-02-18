// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream.h"

#include "third_party/blink/renderer/core/streams/readable_stream_native.h"
#include "third_party/blink/renderer/core/streams/readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return Create(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_source,
                                       ExceptionState& exception_state) {
  return Create(
      script_state, underlying_source,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_source,
                                       ScriptValue strategy,
                                       ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    return ReadableStreamNative::Create(script_state, underlying_source,
                                        strategy, exception_state);
  }

  return ReadableStreamWrapper::Create(script_state, underlying_source,
                                       strategy, exception_state);
}

ReadableStream* ReadableStream::CreateWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark) {
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    return ReadableStreamNative::CreateWithCountQueueingStrategy(
        script_state, underlying_source, high_water_mark);
  }

  // TODO(ricea): Select implementation based on StreamsNative feature here.
  return ReadableStreamWrapper::CreateWithCountQueueingStrategy(
      script_state, underlying_source, high_water_mark);
}

// static
ReadableStream* ReadableStream::Deserialize(ScriptState* script_state,
                                            MessagePort* port,
                                            ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::StreamsNativeEnabled()) {
    return ReadableStreamNative::Deserialize(script_state, port,
                                             exception_state);
  }

  return ReadableStreamWrapper::Deserialize(script_state, port,
                                            exception_state);
}

}  // namespace blink

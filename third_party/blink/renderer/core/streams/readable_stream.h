// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class UnderlyingSourceBase;

// This is an implementation of the corresponding IDL interface.
// Use TraceWrapperMember to hold a reference to an instance of this class.
class CORE_EXPORT ReadableStream : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ReadableStream(ScriptState*, v8::Local<v8::Object> object);

  static ReadableStream* Create(ScriptState*, ExceptionState&);
  static ReadableStream* Create(ScriptState*,
                                ScriptValue underlying_source,
                                ExceptionState&);
  static ReadableStream* Create(ScriptState*,
                                ScriptValue underlying_source,
                                ScriptValue strategy,
                                ExceptionState&);

  // This function doesn't take ExceptionState because the caller cannot have
  // one. Returns null when an error happens.
  static ReadableStream* CreateWithCountQueueingStrategy(
      ScriptState*,
      UnderlyingSourceBase* underlying_source,
      size_t high_water_mark);

  ~ReadableStream() override = default;

  void Trace(Visitor* visitor) override;

  // IDL defined functions
  bool locked(ScriptState*, ExceptionState&) const;
  ScriptPromise cancel(ScriptState*, ExceptionState&);
  ScriptPromise cancel(ScriptState*, ScriptValue reason, ExceptionState&);
  ScriptValue getReader(ScriptState*, ExceptionState&);
  ScriptValue getReader(ScriptState*, ScriptValue options, ExceptionState&);
  ScriptValue pipeThrough(ScriptState*,
                          ScriptValue transform_stream,
                          ExceptionState&);
  ScriptValue pipeThrough(ScriptState*,
                          ScriptValue transform_stream,
                          ScriptValue options,
                          ExceptionState&);
  ScriptPromise pipeTo(ScriptState*, ScriptValue destination, ExceptionState&);
  ScriptPromise pipeTo(ScriptState*,
                       ScriptValue destination,
                       ScriptValue options,
                       ExceptionState&);
  ScriptValue tee(ScriptState*, ExceptionState&);

  void Tee(ScriptState*,
           ReadableStream** branch1,
           ReadableStream** branch2,
           ExceptionState&);

  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const;
  base::Optional<bool> IsDisturbed(ScriptState*, ExceptionState&) const;
  base::Optional<bool> IsReadable(ScriptState*, ExceptionState&) const;
  base::Optional<bool> IsClosed(ScriptState*, ExceptionState&) const;
  base::Optional<bool> IsErrored(ScriptState*, ExceptionState&) const;

  // Makes this stream locked and disturbed.
  void LockAndDisturb(ScriptState*, ExceptionState&);

  ScriptValue AsScriptValue(ScriptState* script_state) const;

  // In some cases we are known to fail to trace the stream correctly. In such
  // cases |object_| will be silently gone. This function is for detecting the
  // issue. Use this function at places where an actual crash happens. Do not
  // use this function to write "just in case" code.
  bool IsInternalStreamMissing() const { return object_.IsEmpty(); }

 private:
  TraceWrapperV8Reference<v8::Object> object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_H_

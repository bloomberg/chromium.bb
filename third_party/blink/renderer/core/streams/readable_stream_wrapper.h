// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_WRAPPER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "v8/include/v8.h"

namespace blink {

// This is an implementation of the corresponding IDL interface.
class CORE_EXPORT ReadableStreamWrapper : public ReadableStream {
 public:
  // Call one of Init functions before using the instance.
  ReadableStreamWrapper() = default;
  ~ReadableStreamWrapper() override = default;

  // If an error happens, |exception_state.HadException()| will be true, and
  // |this| will not be usable after that.
  void Init(ScriptState*,
            ScriptValue underlying_source,
            ScriptValue strategy,
            ExceptionState& exception_state);
  void InitWithInternalStream(ScriptState*,
                              v8::Local<v8::Object> object,
                              ExceptionState& exception_state);

  // Create* functions call Init* internally and returns null when an error
  // happens.
  static ReadableStreamWrapper* Create(ScriptState*,
                                       ScriptValue underlying_source,
                                       ScriptValue strategy,
                                       ExceptionState&);
  static ReadableStreamWrapper* CreateFromInternalStream(
      ScriptState*,
      v8::Local<v8::Object> object,
      ExceptionState&);
  static ReadableStreamWrapper* CreateFromInternalStream(ScriptState*,
                                                         ScriptValue object,
                                                         ExceptionState&);
  // This function doesn't take ExceptionState because the caller cannot have
  // one. Returns null when an error happens.
  static ReadableStreamWrapper* CreateWithCountQueueingStrategy(
      ScriptState*,
      UnderlyingSourceBase* underlying_source,
      size_t high_water_mark);

  void Trace(Visitor* visitor) override;

  // IDL defined functions
  bool locked(ScriptState*, ExceptionState&) const override;
  ScriptPromise cancel(ScriptState*, ExceptionState&) override;
  ScriptPromise cancel(ScriptState*,
                       ScriptValue reason,
                       ExceptionState&) override;
  ScriptValue getReader(ScriptState*, ExceptionState&) override;
  ScriptValue getReader(ScriptState*,
                        ScriptValue options,
                        ExceptionState&) override;
  ScriptValue pipeThrough(ScriptState*,
                          ScriptValue transform_stream,
                          ExceptionState&) override;
  ScriptValue pipeThrough(ScriptState*,
                          ScriptValue transform_stream,
                          ScriptValue options,
                          ExceptionState&) override;
  ScriptPromise pipeTo(ScriptState*,
                       ScriptValue destination,
                       ExceptionState&) override;
  ScriptPromise pipeTo(ScriptState*,
                       ScriptValue destination,
                       ScriptValue options,
                       ExceptionState&) override;
  ScriptValue tee(ScriptState*, ExceptionState&) override;

  void Tee(ScriptState*,
           ReadableStream** branch1,
           ReadableStream** branch2,
           ExceptionState&) override;

  ReadHandle* GetReadHandle(ScriptState*, ExceptionState&) override;

  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const override;
  base::Optional<bool> IsDisturbed(ScriptState*,
                                   ExceptionState&) const override;
  base::Optional<bool> IsReadable(ScriptState*, ExceptionState&) const override;
  base::Optional<bool> IsClosed(ScriptState*, ExceptionState&) const override;
  base::Optional<bool> IsErrored(ScriptState*, ExceptionState&) const override;

  // Makes this stream locked and disturbed.
  void LockAndDisturb(ScriptState*, ExceptionState&) override;

  // Serialize this stream to |port|. The stream will be locked by this
  // operation.
  void Serialize(ScriptState*, MessagePort* port, ExceptionState&) override;

  // Given a |port| which is entangled with a MessagePort that was previously
  // passed to Serialize(), returns a new ReadableStreamWrapper which behaves
  // like it was the original.
  static ReadableStreamWrapper* Deserialize(ScriptState*,
                                            MessagePort* port,
                                            ExceptionState&);

  ScriptValue GetInternalStream(ScriptState* script_state) const;

  // In some cases we are known to fail to trace the stream correctly. In such
  // cases |object_| will be silently gone. This function is for detecting the
  // issue. Use this function at places where an actual crash happens. Do not
  // use this function to write "just in case" code.
  bool IsBroken() const override { return object_.IsEmpty(); }

 private:
  class ReadHandleImpl;

  TraceWrapperV8Reference<v8::Object> object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_WRAPPER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_WRITER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class ScriptValue;
class WritableStream;
class WritableStreamNative;

class WritableStreamDefaultWriter : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WritableStreamDefaultWriter* Create(ScriptState*,
                                             WritableStream* stream,
                                             ExceptionState&);

  WritableStreamDefaultWriter(ScriptState*,
                              WritableStreamNative* stream,
                              ExceptionState&);
  ~WritableStreamDefaultWriter() override;

  ScriptPromise closed(ScriptState*) const;
  ScriptValue desiredSize(ScriptState*, ExceptionState&) const;
  ScriptPromise ready(ScriptState*) const;

  ScriptPromise abort(ScriptState*);
  ScriptPromise abort(ScriptState*, ScriptValue reason);

  ScriptPromise close(ScriptState*);

  void releaseLock(ScriptState*);

  ScriptPromise write(ScriptState*);
  ScriptPromise write(ScriptState*, ScriptValue chunk);

 private:
  static void ThrowUnimplemented(ExceptionState& exception_state);
  static ScriptPromise RejectUnimplemented(ScriptState* script_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_WRITER_H_

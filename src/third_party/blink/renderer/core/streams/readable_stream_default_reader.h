// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_READER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class ScriptValue;
class ReadableStream;
class ReadableStreamNative;

class ReadableStreamDefaultReader : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ReadableStreamDefaultReader* Create(ScriptState*,
                                             ReadableStream* stream,
                                             ExceptionState&);

  ReadableStreamDefaultReader(ScriptState*,
                              ReadableStreamNative* stream,
                              ExceptionState&);
  ~ReadableStreamDefaultReader() override;

  ScriptPromise closed(ScriptState*) const;

  ScriptPromise cancel(ScriptState*);
  ScriptPromise cancel(ScriptState*, ScriptValue reason);

  ScriptPromise read(ScriptState*);
  ScriptPromise read(ScriptState*, ScriptValue chunk);

  void releaseLock(ScriptState*);

 private:
  static void ThrowUnimplemented(ExceptionState&);
  static ScriptPromise RejectUnimplemented(ScriptState*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_READER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_NATIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_NATIVE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"

namespace blink {

class ExceptionState;
class ScriptState;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamStartAlgorithm;

class CORE_EXPORT WritableStreamNative : public WritableStream {
 public:
  static WritableStreamNative* Create(ScriptState*,
                                      StreamStartAlgorithm* start_algorithm,
                                      StreamAlgorithm* write_algorithm,
                                      StreamAlgorithm* close_algorithm,
                                      StreamAlgorithm* abort_algorithm,
                                      double high_water_mark,
                                      StrategySizeAlgorithm* size_algorithm,
                                      ExceptionState&);

  WritableStreamNative();
  WritableStreamNative(ScriptState*,
                       ScriptValue raw_underlying_sink,
                       ScriptValue raw_strategy,
                       ExceptionState&);
  ~WritableStreamNative() override;

  // IDL defined functions
  bool locked(ScriptState*, ExceptionState&) const override;
  ScriptPromise abort(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;
  ScriptValue getWriter(ScriptState*, ExceptionState&) override;

  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const override;

  void Serialize(ScriptState*, MessagePort*, ExceptionState&) override {
    // TODO(ricea): Implement this.
  }

 private:
  static void ThrowUnimplemented(ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_NATIVE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_CONTROLLER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class QueueWithSizes;
class ScriptState;
class ScriptValue;
class StrategySizeAlgorithm;
class StreamAlgorithm;
class StreamStartAlgorithm;
class Visitor;
class WritableStreamDefaultWriter;
class WritableStreamNative;

class WritableStreamDefaultController final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The JavaScript-exposed constructor throws automatically as no constructor
  // is specified in the IDL. This constructor is used internally during
  // creation of a WritableStreamNative object.
  WritableStreamDefaultController();

  // https://streams.spec.whatwg.org/#ws-default-controller-error
  void error(ScriptState*);
  void error(ScriptState*, ScriptValue e);

  void Trace(Visitor*) override;

 private:
  // https://streams.spec.whatwg.org/#ws-default-controller-private-abort
  v8::Local<v8::Promise> AbortSteps(ScriptState*, v8::Local<v8::Value> reason);

  // https://streams.spec.whatwg.org/#ws-default-controller-private-error
  void ErrorSteps();

  // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller
  static void SetUp(ScriptState*,
                    WritableStreamNative*,
                    WritableStreamDefaultController*,
                    StreamStartAlgorithm* start_algorithm,
                    StreamAlgorithm* write_algorithm,
                    StreamAlgorithm* close_algorithm,
                    StreamAlgorithm* abort_algorithm,
                    double high_water_mark,
                    StrategySizeAlgorithm* size_algorithm,
                    ExceptionState&);

  // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
  static void SetUpFromUnderlyingSink(ScriptState*,
                                      WritableStreamNative*,
                                      v8::Local<v8::Object> underlying_sink,
                                      double high_water_mark,
                                      StrategySizeAlgorithm* size_algorithm,
                                      ExceptionState&);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-clear-algorithms
  static void ClearAlgorithms(WritableStreamDefaultController*);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-close
  static void Close(ScriptState*, WritableStreamDefaultController*);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-chunk-size
  static double GetChunkSize(ScriptState*,
                             WritableStreamDefaultController*,
                             v8::Local<v8::Value> chunk);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-desired-size
  static double GetDesiredSize(WritableStreamDefaultController*);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-write
  static void Write(ScriptState*,
                    WritableStreamDefaultController*,
                    v8::Local<v8::Value> chunk,
                    double chunk_size);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-advance-queue-if-needed
  static void AdvanceQueueIfNeeded(ScriptState*,
                                   WritableStreamDefaultController*);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-error-if-needed
  static void ErrorIfNeeded(ScriptState*,
                            WritableStreamDefaultController*,
                            v8::Local<v8::Value> error);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-close
  static void ProcessClose(ScriptState*, WritableStreamDefaultController*);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
  static void ProcessWrite(ScriptState*,
                           WritableStreamDefaultController*,
                           v8::Local<v8::Value> chunk);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-backpressure
  static bool GetBackpressure(WritableStreamDefaultController*);

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-error
  static void Error(ScriptState*,
                    WritableStreamDefaultController*,
                    v8::Local<v8::Value> error);

  friend class WritableStreamDefaultWriter;
  friend class WritableStreamNative;

  // Most member variables correspond 1:1 with the internal slots in the
  // standard. See
  // https://streams.spec.whatwg.org/#ws-default-controller-internal-slots.
  TraceWrapperMember<StreamAlgorithm> abort_algorithm_;
  TraceWrapperMember<StreamAlgorithm> close_algorithm_;
  TraceWrapperMember<WritableStreamNative> controlled_writable_stream_;

  // |queue_| covers both the [[queue]] and [[queueTotalSize]] internal slots.
  // Instead of chunks in the queue being wrapped in an object, they are
  // stored-as-is, and the `"close"` marker in the queue is represented by an
  // empty queue together with the |close_queued_| flag being set.
  TraceWrapperMember<QueueWithSizes> queue_;
  bool close_queued_ = false;
  bool started_ = false;
  double strategy_high_water_mark_ = 0.0;
  TraceWrapperMember<StrategySizeAlgorithm> strategy_size_algorithm_;
  TraceWrapperMember<StreamAlgorithm> write_algorithm_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_CONTROLLER_H_

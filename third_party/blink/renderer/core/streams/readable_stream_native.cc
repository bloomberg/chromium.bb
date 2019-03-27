// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_native.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/stream_script_function.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/streams/writable_stream_native.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class ReadableStreamNative::ReadHandleImpl final
    : public ReadableStream::ReadHandle {
 public:
  explicit ReadHandleImpl(ReadableStreamDefaultReader* reader)
      : reader_(reader) {}
  ~ReadHandleImpl() override = default;

  ScriptPromise Read(ScriptState* script_state) override {
    return ReadableStreamDefaultReader::Read(script_state, reader_)
        ->GetScriptPromise(script_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(reader_);
    ReadHandle::Trace(visitor);
  }

 private:
  const TraceWrapperMember<ReadableStreamDefaultReader> reader_;
};

ReadableStreamNative* ReadableStreamNative::Create(
    ScriptState* script_state,
    ScriptValue underlying_source,
    ScriptValue strategy,
    ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStreamNative>(
      script_state, underlying_source, strategy, false, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  return stream;
}

ReadableStreamNative* ReadableStreamNative::CreateWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark) {
  auto* isolate = script_state->GetIsolate();

  // It's safer to use a workalike rather than a real CountQueuingStrategy
  // object. We use the default "size" function as it is implemented in C++ and
  // so much faster than calling into JavaScript. Since the create object has a
  // null prototype, there is no danger of us finding some other "size" function
  // via the prototype chain.
  v8::Local<v8::Name> high_water_mark_string =
      V8AtomicString(isolate, "highWaterMark");
  v8::Local<v8::Value> high_water_mark_value =
      v8::Number::New(isolate, high_water_mark);

  auto strategy_object =
      v8::Object::New(isolate, v8::Null(isolate), &high_water_mark_string,
                      &high_water_mark_value, 1);

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "ReadableStream");

  v8::Local<v8::Value> underlying_source_v8 =
      ToV8(underlying_source, script_state);

  auto* stream = MakeGarbageCollected<ReadableStreamNative>(
      script_state, ScriptValue(script_state, underlying_source_v8),
      ScriptValue(script_state, strategy_object), true, exception_state);

  if (exception_state.HadException()) {
    exception_state.ClearException();
    DLOG(WARNING)
        << "Ignoring an exception in CreateWithCountQueuingStrategy().";
  }

  return stream;
}

ReadableStreamNative* ReadableStreamNative::Create(
    ScriptState* script_state,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* pull_algorithm,
    StreamAlgorithm* cancel_algorithm,
    double high_water_mark,
    StrategySizeAlgorithm* size_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#create-readable-stream
  // All arguments are compulsory in this implementation, so the first two steps
  // are skipped:
  // 1. If highWaterMark was not passed, set it to 1.
  // 2. If sizeAlgorithm was not passed, set it to an algorithm that returns 1.

  // 3. Assert: ! IsNonNegativeNumber(highWaterMark) is true.
  DCHECK_GE(high_water_mark, 0);

  // 4. Let stream be ObjectCreate(the original value of ReadableStream's
  //    prototype property).
  auto* stream = MakeGarbageCollected<ReadableStreamNative>();

  // 5. Perform ! InitializeReadableStream(stream).
  Initialize(stream);

  // 6. Let controller be ObjectCreate(the original value of
  //    ReadableStreamDefaultController's prototype property).
  auto* controller = MakeGarbageCollected<ReadableStreamDefaultController>();

  // 7. Perform ? SetUpReadableStreamDefaultController(stream, controller,
  //    startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //    sizeAlgorithm).
  ReadableStreamDefaultController::SetUp(
      script_state, stream, controller, start_algorithm, pull_algorithm,
      cancel_algorithm, high_water_mark, size_algorithm, false,
      exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 8. Return stream.
  return stream;
}

ReadableStreamNative::ReadableStreamNative() = default;

// TODO(ricea): Remove |enable_blink_lock_notifications| once
// blink::ReadableStreamOperations has been updated to use CreateReadableStream.
ReadableStreamNative::ReadableStreamNative(ScriptState* script_state,
                                           ScriptValue raw_underlying_source,
                                           ScriptValue raw_strategy,
                                           bool enable_blink_lock_notifications,
                                           ExceptionState& exception_state) {
  if (!enable_blink_lock_notifications) {
    // TODO(ricea): Move this to IDL once blink::ReadableStreamOperations is
    // no longer using the public constructor.
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kReadableStreamConstructor);
  }

  // https://streams.spec.whatwg.org/#rs-constructor
  //  1. Perform ! InitializeReadableStream(this).
  Initialize(this);

  // The next part of this constructor corresponds to the object conversions
  // that are implicit in the definition in the standard.
  DCHECK(!raw_underlying_source.IsEmpty());
  DCHECK(!raw_strategy.IsEmpty());

  auto context = script_state->GetContext();
  auto* isolate = script_state->GetIsolate();

  // TODO(ricea): Share some of this code with WritableStreamNative.

  auto underlying_source_value = raw_underlying_source.V8Value();
  if (underlying_source_value->IsUndefined()) {
    underlying_source_value = v8::Object::New(isolate);
  }
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Object> underlying_source;
  if (!underlying_source_value->ToObject(context).ToLocal(&underlying_source)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }

  auto strategy_value = raw_strategy.V8Value();
  if (strategy_value->IsUndefined()) {
    strategy_value = v8::Object::New(isolate);
  }
  v8::Local<v8::Object> strategy;
  v8::MaybeLocal<v8::Object> strategy_maybe = strategy_value->ToObject(context);
  if (!strategy_maybe.ToLocal(&strategy)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }

  // 2. Let size be ? GetV(strategy, "size").
  v8::Local<v8::Value> size;
  if (!strategy->Get(context, V8AtomicString(isolate, "size")).ToLocal(&size)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }

  // 3. Let highWaterMark be ? GetV(strategy, "highWaterMark").
  v8::Local<v8::Value> high_water_mark_value;
  if (!strategy->Get(context, V8AtomicString(isolate, "highWaterMark"))
           .ToLocal(&high_water_mark_value)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }

  // 4. Let type be ? GetV(underlyingSource, "type").
  v8::Local<v8::Value> type;
  if (!underlying_source->Get(context, V8AtomicString(isolate, "type"))
           .ToLocal(&type)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }

  if (!type->IsUndefined()) {
    // 5. Let typeString be ? ToString(type).
    v8::Local<v8::String> type_string;
    if (!type->ToString(context).ToLocal(&type_string)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return;
    }

    // 6. If typeString is "bytes",
    if (type_string == V8AtomicString(isolate, "bytes")) {
      // TODO(ricea): Implement bytes type.
      exception_state.ThrowRangeError("bytes type is not yet implemented");
      return;
    }

    // 8. Otherwise, throw a RangeError exception.
    exception_state.ThrowRangeError("Invalid type is specified");
    return;
  }

  // 7. Otherwise, if type is undefined,
  //   a. Let sizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(size).
  auto* size_algorithm =
      MakeSizeAlgorithmFromSizeFunction(script_state, size, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  DCHECK(size_algorithm);

  // 2. If highWaterMark is undefined, let highWaterMark be 1.
  double high_water_mark = 1;
  if (!high_water_mark_value->IsUndefined()) {
    // The conversion to Number which happens inside
    // ValidateAndNormalizeHighWaterMark in the standard is performed beforehand
    // here.
    v8::Local<v8::Number> high_water_mark_as_number;
    if (!high_water_mark_value->ToNumber(context).ToLocal(
            &high_water_mark_as_number)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return;
    }
    high_water_mark = high_water_mark_as_number->Value();
  }

  //  3. Set highWaterMark to ? ValidateAndNormalizeHighWaterMark(
  //     highWaterMark).
  high_water_mark =
      ValidateAndNormalizeHighWaterMark(high_water_mark, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 4. Perform ? SetUpReadableStreamDefaultControllerFromUnderlyingSource
  //  (this, underlyingSource, highWaterMark, sizeAlgorithm).
  ReadableStreamDefaultController::SetUpFromUnderlyingSource(
      script_state, this, underlying_source, high_water_mark, size_algorithm,
      enable_blink_lock_notifications, exception_state);
}

ReadableStreamNative::~ReadableStreamNative() = default;

bool ReadableStreamNative::locked(ScriptState* script_state,
                                  ExceptionState& exception_state) const {
  // https://streams.spec.whatwg.org/#rs-locked
  // 2. Return ! IsReadableStreamLocked(this).
  return IsLocked(this);
}

ScriptPromise ReadableStreamNative::cancel(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  return cancel(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ScriptPromise ReadableStreamNative::cancel(ScriptState* script_state,
                                           ScriptValue reason,
                                           ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-cancel
  // 2. If ! IsReadableStreamLocked(this) is true, return a promise rejected
  //    with a TypeError exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot cancel a locked stream");
    return ScriptPromise();
  }

  // 3. Return ! ReadableStreamCancel(this, reason).
  v8::Local<v8::Promise> result = Cancel(script_state, this, reason.V8Value());
  return ScriptPromise(script_state, result);
}

ScriptValue ReadableStreamNative::getReader(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  // 2. If mode is undefined, return ? AcquireReadableStreamDefaultReader(this,
  //    true).
  auto* reader = ReadableStreamNative::AcquireDefaultReader(
      script_state, this, true, exception_state);
  if (!reader) {
    return ScriptValue();
  }

  return ScriptValue(script_state, ToV8(reader, script_state));
}

ScriptValue ReadableStreamNative::getReader(ScriptState* script_state,
                                            ScriptValue options,
                                            ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  // Since we don't support byob readers, the only thing
  // GetReaderValidateOptions() needs to do is throw an exception if
  // |options.mode| is invalid.
  GetReaderValidateOptions(script_state, options, exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  return getReader(script_state, exception_state);
}

ScriptValue ReadableStreamNative::pipeThrough(ScriptState* script_state,
                                              ScriptValue transform_stream,
                                              ExceptionState& exception_state) {
  return pipeThrough(
      script_state, transform_stream,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

// https://streams.spec.whatwg.org/#rs-pipe-through
ScriptValue ReadableStreamNative::pipeThrough(ScriptState* script_state,
                                              ScriptValue transform_stream,
                                              ScriptValue options,
                                              ExceptionState& exception_state) {
  exception_state.ThrowTypeError("pipeThrough not yet implemented");
  return ScriptValue();
}

ScriptPromise ReadableStreamNative::pipeTo(ScriptState* script_state,
                                           ScriptValue destination,
                                           ExceptionState& exception_state) {
  return pipeTo(
      script_state, destination,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())),
      exception_state);
}

ScriptPromise ReadableStreamNative::pipeTo(ScriptState* script_state,
                                           ScriptValue destination_value,
                                           ScriptValue options,
                                           ExceptionState& exception_state) {
  exception_state.ThrowTypeError("pipeTo not yet implemented");
  return ScriptPromise();
}

ScriptValue ReadableStreamNative::tee(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  return CallTeeAndReturnBranchArray(script_state, this, exception_state);
}

//
// Readable stream abstract operations
//
ReadableStreamDefaultReader* ReadableStreamNative::AcquireDefaultReader(
    ScriptState* script_state,
    ReadableStreamNative* stream,
    bool for_author_code,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#acquire-readable-stream-reader
  // for_author_code is compulsory in this implementation
  // 1. If forAuthorCode was not passed, set it to false.

  // 2. Let reader be ? Construct(ReadableStreamDefaultReader, « stream »).
  auto* reader = MakeGarbageCollected<ReadableStreamDefaultReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 3. Set reader.[[forAuthorCode]] to forAuthorCode.
  reader->for_author_code_ = for_author_code;

  // 4. Return reader.
  return reader;
}

void ReadableStreamNative::Initialize(ReadableStreamNative* stream) {
  // Fields are initialised by the constructor, so we only check that they were
  // initialised correctly.
  // https://streams.spec.whatwg.org/#initialize-readable-stream
  // 1. Set stream.[[state]] to "readable".
  DCHECK_EQ(stream->state_, kReadable);
  // 2. Set stream.[[reader]] and stream.[[storedError]] to undefined.
  DCHECK(!stream->reader_);
  DCHECK(stream->stored_error_.IsEmpty());
  // 3. Set stream.[[disturbed]] to false.
  DCHECK(!stream->is_disturbed_);
}

// TODO(domenic): cloneForBranch2 argument from spec not supported yet
void ReadableStreamNative::Tee(ScriptState* script_state,
                               ReadableStream** branch1,
                               ReadableStream** branch2,
                               ExceptionState& exception_state) {
  exception_state.ThrowTypeError("tee not yet implemented");
}

ReadableStream::ReadHandle* ReadableStreamNative::GetReadHandle(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* reader = ReadableStreamNative::AcquireDefaultReader(
      script_state, this, false, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return MakeGarbageCollected<ReadHandleImpl>(reader);
}

void ReadableStreamNative::LockAndDisturb(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  ScriptState::Scope scope(script_state);

  if (reader_) {
    return;
  }

  ReadableStreamDefaultReader* reader =
      AcquireDefaultReader(script_state, this, false, exception_state);
  if (!reader) {
    return;
  }

  is_disturbed_ = true;
}

void ReadableStreamNative::Serialize(ScriptState* script_state,
                                     MessagePort* port,
                                     ExceptionState& exception_state) {
  // TODO(ricea): Implement this.
}

v8::Local<v8::Value> ReadableStreamNative::GetStoredError(
    v8::Isolate* isolate) const {
  return stored_error_.NewLocal(isolate);
}

void ReadableStreamNative::Trace(Visitor* visitor) {
  visitor->Trace(readable_stream_controller_);
  visitor->Trace(reader_);
  visitor->Trace(stored_error_);
  ReadableStream::Trace(visitor);
}

//
// Abstract Operations Used By Controllers
//

StreamPromiseResolver* ReadableStreamNative::AddReadRequest(
    ScriptState* script_state,
    ReadableStreamNative* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-add-read-request
  // 1. Assert: ! IsReadableStreamDefaultReader(stream.[[reader]]) is true.
  DCHECK(stream->reader_);

  // 2. Assert: stream.[[state]] is "readable".
  DCHECK_EQ(stream->state_, kReadable);

  // 3. Let promise be a new promise.
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);

  // This implementation stores promises directly in |read_requests_| rather
  // than wrapping them in a Record.
  // 4. Let readRequest be Record {[[promise]]: promise}.
  // 5. Append readRequest as the last element of stream.[[reader]].
  //  [[readRequests]].
  stream->reader_->read_requests_.push_back(promise);

  // 6. Return promise.
  return promise;
}

v8::Local<v8::Promise> ReadableStreamNative::Cancel(
    ScriptState* script_state,
    ReadableStreamNative* stream,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#readable-stream-cancel
  // 1. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  // 2. If stream.[[state]] is "closed", return a promise resolved with
  //    undefined.
  const auto state = stream->state_;
  if (state == kClosed) {
    return PromiseResolveWithUndefined(script_state);
  }

  // 3. If stream.[[state]] is "errored", return a promise rejected with stream.
  //    [[storedError]].
  if (state == kErrored) {
    return PromiseReject(script_state,
                         stream->GetStoredError(script_state->GetIsolate()));
  }

  // 4. Perform ! ReadableStreamClose(stream).
  Close(script_state, stream);

  // 5. Let sourceCancelPromise be ! stream.[[readableStreamController]].
  //    [[CancelSteps]](reason).
  v8::Local<v8::Promise> source_cancel_promise =
      stream->readable_stream_controller_->CancelSteps(script_state, reason);

  class ReturnUndefinedFunction : public StreamScriptFunction {
   public:
    explicit ReturnUndefinedFunction(ScriptState* script_state)
        : StreamScriptFunction(script_state) {}

    // The method does nothing; the default value of undefined is returned to
    // JavaScript.
    void CallWithLocal(v8::Local<v8::Value>) override {}
  };

  // 6. Return the result of transforming sourceCancelPromise with a
  //    fulfillment handler that returns undefined.
  return StreamThenPromise(
      script_state->GetContext(), source_cancel_promise,
      MakeGarbageCollected<ReturnUndefinedFunction>(script_state));
}

void ReadableStreamNative::Close(ScriptState* script_state,
                                 ReadableStreamNative* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-close
  // 1. Assert: stream.[[state]] is "readable".
  DCHECK_EQ(stream->state_, kReadable);

  // 2. Set stream.[[state]] to "closed".
  stream->state_ = kClosed;

  // 3. Let reader be stream.[[reader]].
  ReadableStreamDefaultReader* reader = stream->reader_;

  // 4. If reader is undefined, return.
  if (!reader) {
    return;
  }

  // TODO(ricea): Support BYOB readers.
  // 5. If ! IsReadableStreamDefaultReader(reader) is true,
  //   a. Repeat for each readRequest that is an element of reader.
  //      [[readRequests]],
  for (StreamPromiseResolver* promise : reader->read_requests_) {
    //   i. Resolve readRequest.[[promise]] with !
    //      ReadableStreamCreateReadResult(undefined, true, reader.
    //      [[forAuthorCode]]).
    promise->Resolve(script_state,
                     CreateReadResult(script_state,
                                      v8::Undefined(script_state->GetIsolate()),
                                      true, reader->for_author_code_));
  }

  //   b. Set reader.[[readRequests]] to an empty List.
  reader->read_requests_.clear();

  // 6. Resolve reader.[[closedPromise]] with undefined.
  reader->closed_promise_->ResolveWithUndefined(script_state);
}

v8::Local<v8::Value> ReadableStreamNative::CreateReadResult(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    bool done,
    bool for_author_code) {
  // https://streams.spec.whatwg.org/#readable-stream-create-read-result
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  auto value_string = V8AtomicString(isolate, "value");
  auto done_string = V8AtomicString(isolate, "done");
  auto done_value = v8::Boolean::New(isolate, done);
  // 1. Let prototype be null.
  // 2. If forAuthorCode is true, set prototype to %ObjectPrototype%.
  // This implementation doesn't use a |prototype| variable, instead using
  // different code paths depending on the value of |for_author_code|.
  if (for_author_code) {
    // 4. Let obj be ObjectCreate(prototype).
    auto obj = v8::Object::New(isolate);

    // 5. Perform CreateDataProperty(obj, "value", value).
    obj->CreateDataProperty(context, value_string, value).Check();

    // 6. Perform CreateDataProperty(obj, "done", done).
    obj->CreateDataProperty(context, done_string, done_value).Check();

    // 7. Return obj.
    return obj;
  }

  // When |for_author_code| is false, we can perform all the steps in a single
  // call to V8.

  // 4. Let obj be ObjectCreate(prototype).
  // 5. Perform CreateDataProperty(obj, "value", value).
  // 6. Perform CreateDataProperty(obj, "done", done).
  // 7. Return obj.
  // TODO(ricea): Is it possible to use this optimised API in both cases?
  v8::Local<v8::Name> names[2] = {value_string, done_string};
  v8::Local<v8::Value> values[2] = {value, done_value};

  static_assert(base::size(names) == base::size(values),
                "names and values arrays must be the same size");
  return v8::Object::New(isolate, v8::Null(isolate), names, values,
                         base::size(names));
}

void ReadableStreamNative::Error(ScriptState* script_state,
                                 ReadableStreamNative* stream,
                                 v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#readable-stream-error
  // 2. Assert: stream.[[state]] is "readable".
  DCHECK_EQ(stream->state_, kReadable);
  auto* isolate = script_state->GetIsolate();

  // 3. Set stream.[[state]] to "errored".
  stream->state_ = kErrored;

  // 4. Set stream.[[storedError]] to e.
  stream->stored_error_.Set(isolate, e);

  // 5. Let reader be stream.[[reader]].
  ReadableStreamDefaultReader* reader = stream->reader_;

  // 6. If reader is undefined, return.
  if (!reader) {
    return;
  }

  // 7. If ! IsReadableStreamDefaultReader(reader) is true,
  // TODO(ricea): Support BYOB readers.
  //   a. Repeat for each readRequest that is an element of reader.
  //      [[readRequests]],
  for (StreamPromiseResolver* promise : reader->read_requests_) {
    //   i. Reject readRequest.[[promise]] with e.
    promise->Reject(script_state, e);
  }

  //   b. Set reader.[[readRequests]] to a new empty List.
  reader->read_requests_.clear();

  // 9. Reject reader.[[closedPromise]] with e.
  reader->closed_promise_->Reject(script_state, e);

  // 10. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  reader->closed_promise_->MarkAsHandled(isolate);
}

void ReadableStreamNative::FulfillReadRequest(ScriptState* script_state,
                                              ReadableStreamNative* stream,
                                              v8::Local<v8::Value> chunk,
                                              bool done) {
  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-request
  // 1. Let reader be stream.[[reader]].
  ReadableStreamDefaultReader* reader = stream->reader_;

  // 2. Let readRequest be the first element of reader.[[readRequests]].
  StreamPromiseResolver* read_request = reader->read_requests_.front();

  // 3. Remove readIntoRequest from reader.[[readIntoRequests]], shifting all
  //    other elements downward (so that the second becomes the first, and so
  //    on).
  reader->read_requests_.pop_front();

  // 4. Resolve readIntoRequest.[[promise]] with !
  //    ReadableStreamCreateReadResult(chunk, done, reader.[[forAuthorCode]]).
  read_request->Resolve(
      script_state, ReadableStreamNative::CreateReadResult(
                        script_state, chunk, done, reader->for_author_code_));
}

int ReadableStreamNative::GetNumReadRequests(
    const ReadableStreamNative* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
  // 1. Return the number of elements in stream.[[reader]].[[readRequests]].
  return stream->reader_->read_requests_.size();
}

//
//  Readable Stream Reader Generic Abstract Operations
//

v8::Local<v8::Promise> ReadableStreamNative::ReaderGenericCancel(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
  // 1. Let stream be reader.[[ownerReadableStream]].
  ReadableStreamNative* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Return ! ReadableStreamCancel(stream, reason).
  return ReadableStreamNative::Cancel(script_state, stream, reason);
}

void ReadableStreamNative::ReaderGenericInitialize(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader,
    ReadableStreamNative* stream) {
  auto* isolate = script_state->GetIsolate();
  // TODO(yhirano): Remove this when we don't need hasPendingActivity in
  // blink::UnderlyingSourceBase.
  ReadableStreamDefaultController* controller =
      stream->readable_stream_controller_;
  if (controller->enable_blink_lock_notifications_) {
    // The stream is created with an external controller (i.e. made in
    // Blink).
    v8::Local<v8::Object> lock_notify_target =
        controller->lock_notify_target_.NewLocal(isolate);
    CallNullaryMethod(script_state, lock_notify_target, "notifyLockAcquired");
  }

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-initialize
  // 1. Set reader.[[forAuthorCode]] to true.
  DCHECK(reader->for_author_code_);

  // 2. Set reader.[[ownerReadableStream]] to stream.
  reader->owner_readable_stream_ = stream;

  // 3. Set stream.[[reader]] to reader.
  stream->reader_ = reader;

  switch (stream->state_) {
    // 4. If stream.[[state]] is "readable",
    case kReadable:
      // a. Set reader.[[closedPromise]] to a new promise.
      reader->closed_promise_ =
          MakeGarbageCollected<StreamPromiseResolver>(script_state);
      break;

    // 5. Otherwise, if stream.[[state]] is "closed",
    case kClosed:
      // a. Set reader.[[closedPromise]] to a promise resolved with undefined.
      reader->closed_promise_ =
          StreamPromiseResolver::CreateResolvedWithUndefined(script_state);
      break;

    // 6. Otherwise,
    case kErrored:
      // a. Assert: stream.[[state]] is "errored".
      DCHECK_EQ(stream->state_, kErrored);

      // b. Set reader.[[closedPromise]] to a promise rejected with stream.
      //    [[storedError]].
      reader->closed_promise_ = StreamPromiseResolver::CreateRejected(
          script_state, stream->GetStoredError(isolate));

      // c. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
      reader->closed_promise_->MarkAsHandled(isolate);
      break;
  }
}

void ReadableStreamNative::ReaderGenericRelease(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader) {
  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
  // 1. Assert: reader.[[ownerReadableStream]] is not undefined.
  DCHECK(reader->owner_readable_stream_);

  // 2. Assert: reader.[[ownerReadableStream]].[[reader]] is reader.
  DCHECK_EQ(reader->owner_readable_stream_->reader_, reader);

  auto* isolate = script_state->GetIsolate();
  // TODO(yhirano): Remove this when we don"t need hasPendingActivity in
  // blink::UnderlyingSourceBase.
  ReadableStreamDefaultController* controller =
      reader->owner_readable_stream_->readable_stream_controller_;
  if (controller->enable_blink_lock_notifications_) {
    // The stream is created with an external controller (i.e. made in
    // Blink).
    auto lock_notify_target = controller->lock_notify_target_.NewLocal(isolate);
    CallNullaryMethod(script_state, lock_notify_target, "notifyLockReleased");
  }

  // 3. If reader.[[ownerReadableStream]].[[state]] is "readable", reject
  //    reader.[[closedPromise]] with a TypeError exception.
  if (reader->owner_readable_stream_->state_ == kReadable) {
    reader->closed_promise_->Reject(
        script_state,
        v8::Exception::TypeError(V8String(
            isolate,
            "This readable stream reader has been released and cannot be used "
            "to monitor the stream's state")));
  } else {
    // 4. Otherwise, set reader.[[closedPromise]] to a promise rejected with a
    //    TypeError exception.
    reader->closed_promise_ = StreamPromiseResolver::CreateRejected(
        script_state, v8::Exception::TypeError(V8String(
                          isolate,
                          "This readable stream reader has been released and "
                          "cannot be used to monitor the stream's state")));
  }

  // 5. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  reader->closed_promise_->MarkAsHandled(isolate);

  // 6. Set reader.[[ownerReadableStream]].[[reader]] to undefined.
  reader->owner_readable_stream_->reader_ = nullptr;

  // 7. Set reader.[[ownerReadableStream]] to undefined.
  reader->owner_readable_stream_ = nullptr;
}

//
// TODO(ricea): Functions for transferable streams.
//

void ReadableStreamNative::CallNullaryMethod(ScriptState* script_state,
                                             v8::Local<v8::Object> object,
                                             const char* method_name) {
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> method;
  if (!object->Get(context, V8AtomicString(isolate, method_name))
           .ToLocal(&method)) {
    DLOG(WARNING) << "Ignored failed lookup of '" << method_name
                  << "' in CallNullaryMethod";
    return;
  }

  if (!method->IsFunction()) {
    DLOG(WARNING) << "Didn't call '" << method_name
                  << "' in CallNullaryMethod because it was the wrong type";
    return;
  }

  v8::MaybeLocal<v8::Value> result =
      method.As<v8::Function>()->Call(context, object, 0, nullptr);
  if (result.IsEmpty()) {
    DLOG(WARNING) << "Ignored failure of '" << method_name
                  << "' in CallNullaryMethod";
  }
}

}  // namespace blink

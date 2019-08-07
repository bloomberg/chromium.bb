// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_native.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

ReadableStreamDefaultReader* ReadableStreamDefaultReader::Create(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::StreamsNativeEnabled());
  auto* stream_native = static_cast<ReadableStreamNative*>(stream);
  auto* reader = MakeGarbageCollected<ReadableStreamDefaultReader>(
      script_state, stream_native, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return reader;
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(
    ScriptState* script_state,
    ReadableStreamNative* stream,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-constructor
  // 2. If ! IsReadableStreamLocked(stream) is true, throw a TypeError
  //    exception.
  if (ReadableStreamNative::IsLocked(stream)) {
    exception_state.ThrowTypeError(
        "ReadableStreamReader constructor can only accept readable streams "
        "that are not yet locked to a reader");
    return;
  }

  // 3. Perform ! ReadableStreamReaderGenericInitialize(this, stream).
  ReadableStreamNative::ReaderGenericInitialize(script_state, this, stream);

  // 4. Set this.[[readRequests]] to a new empty List.
  DCHECK_EQ(read_requests_.size(), 0u);
}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader() = default;

ScriptPromise ReadableStreamDefaultReader::closed(
    ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#default-reader-closed
  //  2. Return this.[[closedPromise]].
  return closed_promise_->GetScriptPromise(script_state);
}

ScriptPromise ReadableStreamDefaultReader::cancel(ScriptState* script_state) {
  return cancel(
      script_state,
      ScriptValue(script_state, v8::Undefined(script_state->GetIsolate())));
}

ScriptPromise ReadableStreamDefaultReader::cancel(ScriptState* script_state,
                                                  ScriptValue reason) {
  // https://streams.spec.whatwg.org/#default-reader-cancel
  // 2. If this.[[ownerReadableStream]] is undefined, return a promise rejected
  //    with a TypeError exception.
  if (!owner_readable_stream_) {
    return ScriptPromise::Reject(
        script_state,
        v8::Exception::TypeError(V8String(
            script_state->GetIsolate(),
            "This readable stream reader has been released and cannot be used "
            "to cancel its previous owner stream")));
  }

  // 3. Return ! ReadableStreamReaderGenericCancel(this, reason).
  v8::Local<v8::Promise> result = ReadableStreamNative::ReaderGenericCancel(
      script_state, this, reason.V8Value());
  return ScriptPromise(script_state, result);
}

ScriptPromise ReadableStreamDefaultReader::read(ScriptState* script_state) {
  // https://streams.spec.whatwg.org/#default-reader-read
  // 2. If this.[[ownerReadableStream]] is undefined, return a promise rejected
  //  with a TypeError exception.
  if (!owner_readable_stream_) {
    return ScriptPromise::Reject(
        script_state,
        v8::Exception::TypeError(V8String(
            script_state->GetIsolate(),
            "This readable stream reader has been released and cannot be used "
            "to read from its previous owner stream")));
  }

  // 3. Return ! ReadableStreamDefaultReaderRead(this).
  return ReadableStreamDefaultReader::Read(script_state, this)
      ->GetScriptPromise(script_state);
}

void ReadableStreamDefaultReader::releaseLock(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-release-lock
  // 2. If this.[[ownerReadableStream]] is undefined, return.
  if (!owner_readable_stream_) {
    return;
  }

  // 3. If this.[[readRequests]] is not empty, throw a TypeError exception.
  if (read_requests_.size() > 0) {
    exception_state.ThrowTypeError(
        "Cannot release a readable stream reader when it still has outstanding "
        "read() calls that have not yet settled");
    return;
  }

  // 4. Perform ! ReadableStreamReaderGenericRelease(this).
  ReadableStreamNative::ReaderGenericRelease(script_state, this);
}

StreamPromiseResolver* ReadableStreamDefaultReader::Read(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader) {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
  // 1. Let stream be reader.[[ownerReadableStream]].
  ReadableStreamNative* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  switch (stream->state_) {
    // 4. If stream.[[state]] is "closed", return a promise resolved with !
    //    ReadableStreamCreateReadResult(undefined, true,
    //    reader.[[forAuthorCode]]).
    case ReadableStreamNative::kClosed:
      return StreamPromiseResolver::CreateResolved(
          script_state, ReadableStreamNative::CreateReadResult(
                            script_state, v8::Undefined(isolate), true,
                            reader->for_author_code_));

    // 5. If stream.[[state]] is "errored", return a promise rejected with
    //    stream.[[storedError]].
    case ReadableStreamNative::kErrored:
      return StreamPromiseResolver::CreateRejected(
          script_state, stream->GetStoredError(isolate));

    case ReadableStreamNative::kReadable:
      // 6. Assert: stream.[[state]] is "readable".
      DCHECK_EQ(stream->state_, ReadableStreamNative::kReadable);

      // 7. Return ! stream.[[readableStreamController]].[[PullSteps]]().
      return stream->GetController()->PullSteps(script_state);
  }
}

void ReadableStreamDefaultReader::Trace(Visitor* visitor) {
  visitor->Trace(closed_promise_);
  visitor->Trace(owner_readable_stream_);
  visitor->Trace(read_requests_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

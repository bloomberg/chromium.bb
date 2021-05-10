// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"

#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source_cancel_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source_pull_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source_start_callback.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

template <typename DOMType>
DOMArrayBufferView* CreateAsArrayBufferView(DOMArrayBuffer* buffer,
                                            size_t byte_offset,
                                            size_t length) {
  return DOMType::Create(buffer, byte_offset, length);
}

}  // namespace

ReadableByteStreamController::QueueEntry::QueueEntry(DOMArrayBuffer* buffer,
                                                     size_t byte_offset,
                                                     size_t byte_length)
    : buffer(buffer), byte_offset(byte_offset), byte_length(byte_length) {}

void ReadableByteStreamController::QueueEntry::Trace(Visitor* visitor) const {
  visitor->Trace(buffer);
}

ReadableByteStreamController::PullIntoDescriptor::PullIntoDescriptor(
    DOMArrayBuffer* buffer,
    size_t byte_offset,
    size_t byte_length,
    size_t bytes_filled,
    size_t element_size,
    ViewConstructorType view_constructor,
    ReaderType reader_type)
    : buffer(buffer),
      byte_offset(byte_offset),
      byte_length(byte_length),
      bytes_filled(bytes_filled),
      element_size(element_size),
      view_constructor(view_constructor),
      reader_type(reader_type) {}

void ReadableByteStreamController::PullIntoDescriptor::Trace(
    Visitor* visitor) const {
  visitor->Trace(buffer);
}

// This constructor is used internally; it is not reachable from Javascript.
ReadableByteStreamController::ReadableByteStreamController()
    : queue_total_size_(queue_.size()) {}

ReadableStreamBYOBRequest* ReadableByteStreamController::byobRequest() {
  // https://streams.spec.whatwg.org/#rbs-controller-byob-request
  // 1. If this.[[byobRequest]] is null and this.[[pendingPullIntos]] is not
  // empty,
  if (!byob_request_ && !pending_pull_intos_.IsEmpty()) {
    //   a. Let firstDescriptor be this.[[pendingPullIntos]][0].
    const PullIntoDescriptor* first_descriptor = pending_pull_intos_[0];
    //   b. Let view be ! Construct(%Uint8Array%, « firstDescriptor’s buffer,
    //   firstDescriptor’s byte offset + firstDescriptor’s bytes filled,
    //   firstDescriptor’s byte length − firstDescriptor’s bytes filled »).
    DOMUint8Array* const view = DOMUint8Array::Create(
        first_descriptor->buffer,
        first_descriptor->byte_offset + first_descriptor->bytes_filled,
        first_descriptor->byte_length - first_descriptor->bytes_filled);
    //   c. Let byobRequest be a new ReadableStreamBYOBRequest.
    //   d. Set byobRequest.[[controller]] to this.
    //   e. Set byobRequest.[[view]] to view.
    //   f. Set this.[[byobRequest]] to byobRequest.
    byob_request_ = MakeGarbageCollected<ReadableStreamBYOBRequest>(
        this, NotShared<DOMUint8Array>(view));
  }
  // 2. Return this.[[byobRequest]].
  return byob_request_;
}

base::Optional<double> ReadableByteStreamController::desiredSize() {
  // https://streams.spec.whatwg.org/#rbs-controller-desired-size
  // 1. Return ! ReadableByteStreamControllerGetDesiredSize(this).
  return GetDesiredSize(this);
}

base::Optional<double> ReadableByteStreamController::GetDesiredSize(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-get-desired-size
  // 1. Let state be controller.[[stream]].[[state]].
  switch (controller->controlled_readable_stream_->state_) {
      // 2. If state is "errored", return null.
    case ReadableStream::kErrored:
      return base::nullopt;

      // 3. If state is "closed", return 0.
    case ReadableStream::kClosed:
      return 0.0;

    case ReadableStream::kReadable:
      // 4. Return controller.[[strategyHWM]]] - controller.[[queueTotalSize]].
      return controller->strategy_high_water_mark_ -
             controller->queue_total_size_;
  }
}

void ReadableByteStreamController::close(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rbs-controller-close
  // 1. If this.[[closeRequested]] is true, throw a TypeError exception.
  if (close_requested_) {
    exception_state.ThrowTypeError(
        "Cannot close a readable stream that has already been requested "
        "to be closed");
    return;
  }

  // 2. If this.[[stream]].[[state]] is not "readable", throw a TypeError
  // exception.
  if (controlled_readable_stream_->state_ != ReadableStream::kReadable) {
    exception_state.ThrowTypeError(
        "Cannot close a readable stream that is not readable");
    return;
  }

  // 3. Perform ? ReadableByteStreamControllerClose(this).
  Close(script_state, this, exception_state);
}

void ReadableByteStreamController::enqueue(ScriptState* script_state,
                                           NotShared<DOMArrayBufferView> chunk,
                                           ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rbs-controller-enqueue
  // 1. If chunk.[[ByteLength]] is 0, throw a TypeError exception.
  if (chunk->byteLength() == 0) {
    exception_state.ThrowTypeError("chunk is empty");
    return;
  }

  // 2. If chunk.[[ViewedArrayBuffer]].[[ArrayBufferByteLength]] is 0, throw a
  // TypeError exception.
  if (chunk->buffer()->ByteLength() == 0) {
    exception_state.ThrowTypeError("chunk's buffer is empty");
    return;
  }

  // 3. If this.[[closeRequested]] is true, throw a TypeError exception.
  if (close_requested_) {
    exception_state.ThrowTypeError("close requested already");
    return;
  }

  // 4. If this.[[stream]].[[state]] is not "readable", throw a TypeError
  // exception.
  if (controlled_readable_stream_->state_ != ReadableStream::kReadable) {
    exception_state.ThrowTypeError("stream is not readable");
    return;
  }

  // 5. Return ! ReadableByteStreamControllerEnqueue(this, chunk).
  Enqueue(script_state, this, chunk, exception_state);
}

void ReadableByteStreamController::error(ScriptState* script_state) {
  error(script_state, ScriptValue(script_state->GetIsolate(),
                                  v8::Undefined(script_state->GetIsolate())));
}

void ReadableByteStreamController::error(ScriptState* script_state,
                                         const ScriptValue& e) {
  // https://streams.spec.whatwg.org/#rbs-controller-error
  // 1. Perform ! ReadableByteStreamControllerError(this, e).
  Error(script_state, this, e.V8Value());
}

void ReadableByteStreamController::Close(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-close
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;

  // 2. If controller.[[closeRequested]] is true or stream.[[state]] is not
  // "readable", return.
  if (controller->close_requested_ ||
      stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. If controller.[[queueTotalSize]] > 0,
  if (controller->queue_total_size_ > 0) {
    //   a. Set controller.[[closeRequested]] to true.
    controller->close_requested_ = true;
    //   b. Return.
    return;
  }

  // 4. If controller.[[pendingPullIntos]] is not empty,
  if (!controller->pending_pull_intos_.IsEmpty()) {
    //   a. Let firstPendingPullInto be controller.[[pendingPullIntos]][0].
    const PullIntoDescriptor* first_pending_pull_into =
        controller->pending_pull_intos_[0];
    //   b. If firstPendingPullInto’s bytes filled > 0,
    if (first_pending_pull_into->bytes_filled > 0) {
      //     i. Let e be a new TypeError exception.
      exception_state.ThrowTypeError("Cannot close while responding");
      v8::Local<v8::Value> e = exception_state.GetException();
      //     ii. Perform ! ReadableByteStreamControllerError(controller, e).
      Error(script_state, controller, e);
      //     iii. Throw e.
      return;
    }
  }

  // 5. Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  // 6. Perform ! ReadableStreamClose(stream).
  ReadableStream::Close(script_state, stream);
}

void ReadableByteStreamController::Error(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-error
  // 1. Let stream by controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;

  // 2. If stream.[[state]] is not "readable", return.
  if (stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. Perform ! ReadableByteStreamControllerClearPendingPullIntos(controller).
  ClearPendingPullIntos(controller);

  // 4. Perform ! ResetQueue(controller).
  ResetQueue(controller);

  // 5. Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  // 6. Perform ! ReadableStreamError(stream, e).
  ReadableStream::Error(script_state, stream, e);
}

void ReadableByteStreamController::Enqueue(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    NotShared<DOMArrayBufferView> chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue
  // 1. Let stream be controller.[[stream]].i
  ReadableStream* const stream = controller->controlled_readable_stream_;

  // 2. If controller.[[closeRequested]] is true or stream.[[state]] is not
  // "readable", return.
  if (controller->close_requested_ ||
      stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. Let buffer be chunk.[[ViewedArrayBuffer]].
  DOMArrayBuffer* const buffer = chunk->buffer();

  // 4. Let byteOffset be chunk.[[ByteOffset]].
  const size_t byte_offset = chunk->byteOffset();

  // 5. Let byteLength be chunk.[[ByteLength]].
  const size_t byte_length = chunk->byteLength();

  // 6. Let transferredBuffer be ! TransferArrayBuffer(buffer).
  DOMArrayBuffer* const transferred_buffer =
      TransferArrayBuffer(script_state, buffer, exception_state);

  // 7. If ! ReadableStreamHasDefaultReader(stream) is true
  if (ReadableStream::HasDefaultReader(stream)) {
    //   a. If ! ReadableStreamGetNumReadRequests(stream) is 0,
    if (ReadableStream::GetNumReadRequests(stream) == 0) {
      //     i. Perform !
      //     ReadableByteStreamControllerEnqueueChunkToQueue(controller,
      //     transferredBuffer, byteOffset, byteLength).
      EnqueueChunkToQueue(controller, transferred_buffer, byte_offset,
                          byte_length);
    } else {
      // b. Otherwise,
      //     i. Assert: controller.[[queue]] is empty.
      DCHECK(controller->queue_.IsEmpty());
      //     ii. Let transferredView be ! Construct(%Uint8Array%, «
      //     transferredBuffer, byteOffset, byteLength »).
      v8::Local<v8::Value> const transferred_view = v8::Uint8Array::New(
          ToV8(transferred_buffer, script_state).As<v8::ArrayBuffer>(),
          byte_offset, byte_length);
      //     iii. Perform ! ReadableStreamFulfillReadRequest(stream,
      //     transferredView, false).
      ReadableStream::FulfillReadRequest(script_state, stream, transferred_view,
                                         false);
    }
  }

  // 8. Otherwise, if ! ReadableStreamHasBYOBReader(stream) is true,
  else if (ReadableStream::HasBYOBReader(stream)) {
    //   a. Perform !
    //   ReadableByteStreamControllerEnqueueChunkToQueue(controller,
    //   transferredBuffer, byteOffset, byteLength).
    EnqueueChunkToQueue(controller, transferred_buffer, byte_offset,
                        byte_length);
    //   b. Perform !
    //   ReadableByteStreamControllerProcessPullIntoDescriptorsUsing
    //   Queue(controller).
    ProcessPullIntoDescriptorsUsingQueue(script_state, controller);
  } else {
    // 9. Otherwise,
    //   a. Assert: ! IsReadableStreamLocked(stream) is false.
    DCHECK(!ReadableStream::IsLocked(stream));
    //   b. Perform !
    //   ReadableByteStreamControllerEnqueueChunkToQueue(controller,
    //   transferredBuffer, byteOffset, byteLength).
    EnqueueChunkToQueue(controller, transferred_buffer, byte_offset,
                        byte_length);
  }

  // 10. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableByteStreamController::EnqueueChunkToQueue(
    ReadableByteStreamController* controller,
    DOMArrayBuffer* buffer,
    size_t byte_offset,
    size_t byte_length) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue-chunk-to-queue
  // 1. Append a new readable byte stream queue entry with buffer buffer, byte
  // offset byteOffset, and byte length byteLength to controller.[[queue]].
  QueueEntry* const entry =
      MakeGarbageCollected<QueueEntry>(buffer, byte_offset, byte_length);
  controller->queue_.push_back(entry);
  // 2. Set controller.[[queueTotalSize]] to controller.[[queueTotalSize]] +
  // byteLength.
  controller->queue_total_size_ += byte_length;
}

void ReadableByteStreamController::ProcessPullIntoDescriptorsUsingQueue(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-process-pull-into-descriptors-using-queue
  // 1. Assert: controller.[[closeRequested]] is false.
  DCHECK(!controller->close_requested_);
  // 2. While controller.[[pendingPullIntos]] is not empty,
  while (!controller->pending_pull_intos_.IsEmpty()) {
    //   a. If controller.[[queueTotalSize]] is 0, return.
    if (controller->queue_total_size_ == 0) {
      return;
    }
    //   b. Let pullIntoDescriptor be controller.[[pendingPullIntos]][0].
    PullIntoDescriptor* const pull_into_descriptor =
        controller->pending_pull_intos_[0];
    //   c. If ! ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(
    //   controller, pullIntoDescriptor) is true,
    if (FillPullIntoDescriptorFromQueue(controller, pull_into_descriptor)) {
      //     i. Perform !
      //     ReadableByteStreamControllerShiftPendingPullInto(controller).
      ShiftPendingPullInto(controller);
      //     ii. Perform ! ReadableByteStreamControllerCommitPullIntoDescriptor(
      //     controller.[[stream]], pullIntoDescriptor).
      CommitPullIntoDescriptor(script_state,
                               controller->controlled_readable_stream_,
                               pull_into_descriptor);
    }
  }
}

void ReadableByteStreamController::CallPullIfNeeded(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
  // 1. Let shouldPull be !
  // ReadableByteStreamControllerShouldCallPull(controller).
  const bool should_pull = ShouldCallPull(controller);
  // 2. If shouldPull is false, return.
  if (!should_pull) {
    return;
  }
  // 3. If controller.[[pulling]] is true,
  if (controller->pulling_) {
    //   a. Set controller.[[pullAgain]] to true.
    controller->pull_again_ = true;
    //   b. Return.
    return;
  }
  // 4. Assert: controller.[[pullAgain]] is false.
  DCHECK(!controller->pull_again_);
  // 5. Set controller.[[pulling]] to true.
  controller->pulling_ = true;
  // 6. Let pullPromise be the result of performing
  // controller.[[pullAlgorithm]].
  auto pull_promise =
      controller->pull_algorithm_->Run(script_state, 0, nullptr);

  class ResolveFunction final : public PromiseHandler {
   public:
    ResolveFunction(ScriptState* script_state,
                    ReadableByteStreamController* controller)
        : PromiseHandler(script_state), controller_(controller) {}

    void CallWithLocal(v8::Local<v8::Value>) override {
      // 7. Upon fulfillment of pullPromise,
      //   a. Set controller.[[pulling]] to false.
      controller_->pulling_ = false;
      //   b. If controller.[[pullAgain]] is true,
      if (controller_->pull_again_) {
        //     i. Set controller.[[pullAgain]] to false.
        controller_->pull_again_ = false;
        //     ii. Perform !
        //     ReadableByteStreamControllerCallPullIfNeeded(controller).
        CallPullIfNeeded(GetScriptState(), controller_);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  class RejectFunction final : public PromiseHandler {
   public:
    RejectFunction(ScriptState* script_state,
                   ReadableByteStreamController* controller)
        : PromiseHandler(script_state), controller_(controller) {}

    void CallWithLocal(v8::Local<v8::Value> e) override {
      // 8. Upon rejection of pullPromise with reason e,
      //   a. Perform ! ReadableByteStreamControllerError(controller, e).
      Error(GetScriptState(), controller_, e);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  StreamThenPromise(
      script_state->GetContext(), pull_promise,
      MakeGarbageCollected<ResolveFunction>(script_state, controller),
      MakeGarbageCollected<RejectFunction>(script_state, controller));
}

ReadableByteStreamController::PullIntoDescriptor*
ReadableByteStreamController::ShiftPendingPullInto(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-shift-pending-pull-into
  // 1. Let descriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* const descriptor = controller->pending_pull_intos_[0];
  // 2. Remove descriptor from controller.[[pendingPullIntos]].
  controller->pending_pull_intos_.pop_front();
  // 3. Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
  InvalidateBYOBRequest(controller);
  // 4. Return descriptor.
  return descriptor;
}

bool ReadableByteStreamController::ShouldCallPull(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-should-call-pull
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;
  // 2. If stream.[[state]] is not "readable", return false.
  if (stream->state_ != ReadableStream::kReadable) {
    return false;
  }
  // 3. If controller.[[closeRequested]] is true, return false.
  if (controller->close_requested_) {
    return false;
  }
  // 4. If controller.[[started]] is false, return false.
  if (!controller->started_) {
    return false;
  }
  // 5. If ! ReadableStreamHasDefaultReader(stream) is true and !
  // ReadableStreamGetNumReadRequests(stream) > 0, return true.
  if (ReadableStream::HasDefaultReader(stream) &&
      ReadableStream::GetNumReadRequests(stream) > 0) {
    return true;
  }
  // 6. If ! ReadableStreamHasBYOBReader(stream) is true and !
  // ReadableStreamGetNumReadIntoRequests(stream) > 0, return true.
  if (ReadableStream::HasBYOBReader(stream) &&
      ReadableStream::GetNumReadIntoRequests(stream) > 0) {
    return true;
  }
  // 7. Let desiredSize be !
  // ReadableByteStreamControllerGetDesiredSize(controller).
  const base::Optional<double> desired_size = GetDesiredSize(controller);
  // 8. Assert: desiredSize is not null.
  DCHECK(desired_size);
  // 9. If desiredSize > 0, return true.
  if (*desired_size > 0) {
    return true;
  }
  // 10. Return false.
  return false;
}

void ReadableByteStreamController::CommitPullIntoDescriptor(
    ScriptState* script_state,
    ReadableStream* stream,
    PullIntoDescriptor* pull_into_descriptor) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-commit-pull-into-descriptor
  // 1. Assert: stream.[[state]] is not "errored".
  DCHECK_NE(stream->state_, ReadableStream::kErrored);
  // 2. Let done be false.
  bool done = false;
  // 3. If stream.[[state]] is "closed",
  if (stream->state_ == ReadableStream::kClosed) {
    //   a. Assert: pullIntoDescriptor’s bytes filled is 0.
    DCHECK_EQ(pull_into_descriptor->bytes_filled, 0u);
    //   b. Set done to true.
    done = true;
  }
  // 4. Let filledView be !
  // ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
  auto* filled_view = ConvertPullIntoDescriptor(pull_into_descriptor);
  // 5. If pullIntoDescriptor’s reader type is "default",
  if (pull_into_descriptor->reader_type == ReaderType::kDefault) {
    //   a. Perform ! ReadableStreamFulfillReadRequest(stream, filledView,
    //   done).
    ReadableStream::FulfillReadRequest(script_state, stream,
                                       ToV8(filled_view, script_state), done);
  } else {
    // 6. Otherwise,
    //   a. Assert: pullIntoDescriptor’s reader type is "byob".
    DCHECK_EQ(pull_into_descriptor->reader_type, ReaderType::kBYOB);
    //   b. Perform ! ReadableStreamFulfillReadIntoRequest(stream, filledView,
    //   done).
    ReadableStream::FulfillReadIntoRequest(script_state, stream, filled_view,
                                           done);
  }
}

DOMArrayBufferView* ReadableByteStreamController::ConvertPullIntoDescriptor(
    PullIntoDescriptor* pull_into_descriptor) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-convert-pull-into-descriptor
  // 1. Let bytesFilled be pullIntoDescriptor’s bytes filled.
  const size_t bytes_filled = pull_into_descriptor->bytes_filled;
  // 2. Let elementSize be pullIntoDescriptor’s element size.
  const size_t element_size = pull_into_descriptor->element_size;
  // 3. Assert: bytesFilled ≤ pullIntoDescriptor’s byte length.
  DCHECK_LE(bytes_filled, pull_into_descriptor->byte_length);
  // 4. Assert: bytesFilled mod elementSize is 0.
  DCHECK_EQ(bytes_filled % element_size, 0u);
  // 5. Return ! Construct(pullIntoDescriptor’s view constructor, «
  // pullIntoDescriptor’s buffer, pullIntoDescriptor’s byte offset, bytesFilled
  // ÷ elementSize »).
  return pull_into_descriptor->view_constructor(
      pull_into_descriptor->buffer, pull_into_descriptor->byte_offset,
      (bytes_filled / element_size));
}

void ReadableByteStreamController::ClearPendingPullIntos(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-pending-pull-intos
  // 1. Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
  InvalidateBYOBRequest(controller);
  // 2. Set controller.[[pendingPullIntos]] to a new empty list.
  controller->pending_pull_intos_.clear();
}

void ReadableByteStreamController::ClearAlgorithms(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-algorithms
  // 1. Set controller.[[pullAlgorithm]] to undefined.
  controller->pull_algorithm_ = nullptr;

  // 2. Set controller.[[cancelAlgorithm]] to undefined.
  controller->cancel_algorithm_ = nullptr;
}

void ReadableByteStreamController::InvalidateBYOBRequest(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-invalidate-byob-request
  // 1. If controller.[[byobRequest]] is null, return.
  if (!controller->byob_request_) {
    return;
  }
  // 2. Set controller.[[byobRequest]].[[controller]] to undefined.
  controller->byob_request_->controller_ = nullptr;
  // 3. Set controller.[[byobRequest]].[[view]] to null.
  controller->byob_request_->view_ = NotShared<DOMArrayBufferView>(nullptr);
  // 4. Set controller.[[byobRequest]] to null.
  controller->byob_request_ = nullptr;
}

void ReadableByteStreamController::SetUp(
    ScriptState* script_state,
    ReadableStream* stream,
    ReadableByteStreamController* controller,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* pull_algorithm,
    StreamAlgorithm* cancel_algorithm,
    double high_water_mark,
    size_t auto_allocate_chunk_size,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller
  // 1. Assert: stream.[[controller]] is undefined.
  DCHECK(!stream->readable_stream_controller_);
  // 2. If autoAllocateChunkSize is not undefined,
  if (auto_allocate_chunk_size) {
    //   a. Assert: ! IsInteger(autoAllocateChunkSize) is true.
    //   b. Assert: autoAllocateChunkSize is positive.
    //   Due to autoAllocateChunkSize having the [EnforceRange] attribute, it
    //   can never be negative.
    DCHECK_GT(auto_allocate_chunk_size, 0u);
  }
  // 3. Set controller.[[stream]] to stream.
  controller->controlled_readable_stream_ = stream;
  // 4. Set controller.[[pullAgain]] and controller.[[pulling]] to false.
  DCHECK(!controller->pull_again_);
  DCHECK(!controller->pulling_);
  // 5. Set controller.[[byobRequest]] to null.
  DCHECK(!controller->byob_request_);
  // 6. Perform ! ResetQueue(controller).
  ResetQueue(controller);
  // 7. Set controller.[[closeRequested]] and controller.[[started]] to false.
  DCHECK(!controller->close_requested_);
  DCHECK(!controller->started_);
  // 8. Set controller.[[strategyHWM]] to highWaterMark.
  controller->strategy_high_water_mark_ = high_water_mark;
  // 9. Set controller.[[pullAlgorithm]] to pullAlgorithm.
  controller->pull_algorithm_ = pull_algorithm;
  // 10. Set controller.[[cancelAlgorithm]] to cancelAlgorithm.
  controller->cancel_algorithm_ = cancel_algorithm;
  // 11. Set controller.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
  controller->auto_allocate_chunk_size_ = auto_allocate_chunk_size;
  // 12. Set controller.[[pendingPullIntos]] to a new empty list.
  DCHECK(controller->pending_pull_intos_.IsEmpty());
  // 13. Set stream.[[controller]] to controller.
  stream->readable_stream_controller_ = controller;
  // 14. Let startResult be the result of performing startAlgorithm.
  // 15. Let startPromise be a promise resolved with startResult.
  // The conversion of startResult to a promise happens inside start_algorithm
  // in this implementation.
  v8::Local<v8::Promise> start_promise;
  if (!start_algorithm->Run(script_state, exception_state)
           .ToLocal(&start_promise)) {
    if (!exception_state.HadException()) {
      exception_state.ThrowException(
          static_cast<int>(DOMExceptionCode::kInvalidStateError),
          "start algorithm failed with no exception thrown");
    }
    return;
  }
  DCHECK(!exception_state.HadException());

  class ResolveFunction final : public PromiseHandler {
   public:
    ResolveFunction(ScriptState* script_state,
                    ReadableByteStreamController* controller)
        : PromiseHandler(script_state), controller_(controller) {}

    void CallWithLocal(v8::Local<v8::Value>) override {
      // 16. Upon fulfillment of startPromise,
      //   a. Set controller.[[started]] to true.
      controller_->started_ = true;
      //   b. Assert: controller.[[pulling]] is false.
      DCHECK(!controller_->pulling_);
      //   c. Assert: controller.[[pullAgain]] is false.
      DCHECK(!controller_->pull_again_);
      //   d. Perform !
      //   ReadableByteStreamControllerCallPullIfNeeded(controller).
      CallPullIfNeeded(GetScriptState(), controller_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  class RejectFunction final : public PromiseHandler {
   public:
    RejectFunction(ScriptState* script_state,
                   ReadableByteStreamController* controller)
        : PromiseHandler(script_state), controller_(controller) {}

    void CallWithLocal(v8::Local<v8::Value> r) override {
      // 17. Upon rejection of startPromise with reason r,
      //   a. Perform ! ReadableByteStreamControllerError(controller, r).
      Error(GetScriptState(), controller_, r);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  StreamThenPromise(
      script_state->GetContext(), start_promise,
      MakeGarbageCollected<ResolveFunction>(script_state, controller),
      MakeGarbageCollected<RejectFunction>(script_state, controller));
}

void ReadableByteStreamController::SetUpFromUnderlyingSource(
    ScriptState* script_state,
    ReadableStream* stream,
    v8::Local<v8::Object> underlying_source,
    UnderlyingSource* underlying_source_dict,
    double high_water_mark,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller-from-underlying-source
  // 1. Let controller be a new ReadableByteStreamController.
  ReadableByteStreamController* controller =
      MakeGarbageCollected<ReadableByteStreamController>();
  // 2. Let startAlgorithm be an algorithm that returns undefined.
  StreamStartAlgorithm* start_algorithm = CreateTrivialStartAlgorithm();
  // 3. Let pullAlgorithm be an algorithm that returns a promise resolved with
  // undefined.
  StreamAlgorithm* pull_algorithm = CreateTrivialStreamAlgorithm();
  // 4. Let cancelAlgorithm be an algorithm that returns a promise resolved with
  // undefined.
  StreamAlgorithm* cancel_algorithm = CreateTrivialStreamAlgorithm();

  const auto controller_value = ToV8(controller, script_state);
  // 5. If underlyingSourceDict["start"] exists, then set startAlgorithm to an
  // algorithm which returns the result of invoking
  // underlyingSourceDict["start"] with argument list « controller » and
  // callback this value underlyingSource.
  if (underlying_source_dict->hasStart()) {
    start_algorithm = CreateByteStreamStartAlgorithm(
        script_state, underlying_source,
        ToV8(underlying_source_dict->start(), script_state), controller_value);
  }
  // 6. If underlyingSourceDict["pull"] exists, then set pullAlgorithm to an
  // algorithm which returns the result of invoking underlyingSourceDict["pull"]
  // with argument list « controller » and callback this value underlyingSource.
  if (underlying_source_dict->hasPull()) {
    pull_algorithm = CreateAlgorithmFromResolvedMethod(
        script_state, underlying_source,
        ToV8(underlying_source_dict->pull(), script_state), controller_value);
  }
  // 7. If underlyingSourceDict["cancel"] exists, then set cancelAlgorithm to an
  // algorithm which takes an argument reason and returns the result of invoking
  // underlyingSourceDict["cancel"] with argument list « reason » and callback
  // this value underlyingSource.
  if (underlying_source_dict->hasCancel()) {
    cancel_algorithm = CreateAlgorithmFromResolvedMethod(
        script_state, underlying_source,
        ToV8(underlying_source_dict->cancel(), script_state), controller_value);
  }
  // 8. Let autoAllocateChunkSize be
  // underlyingSourceDict["autoAllocateChunkSize"], if it exists, or undefined
  // otherwise.
  size_t auto_allocate_chunk_size =
      underlying_source_dict->hasAutoAllocateChunkSize()
          ? static_cast<size_t>(underlying_source_dict->autoAllocateChunkSize())
          : 0u;
  // 9. If autoAllocateChunkSize is 0, then throw a TypeError exception.
  if (underlying_source_dict->hasAutoAllocateChunkSize() &&
      auto_allocate_chunk_size == 0) {
    exception_state.ThrowTypeError("autoAllocateChunkSize cannot be 0");
    return;
  }
  // 10. Perform ? SetUpReadableByteStreamController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  // autoAllocateChunkSize).
  SetUp(script_state, stream, controller, start_algorithm, pull_algorithm,
        cancel_algorithm, high_water_mark, auto_allocate_chunk_size,
        exception_state);
}

void ReadableByteStreamController::FillHeadPullIntoDescriptor(
    ReadableByteStreamController* controller,
    size_t size,
    PullIntoDescriptor* pull_into_descriptor) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-head-pull-into-descriptor
  // 1. Assert: either controller.[[pendingPullIntos]] is empty, or
  // controller.[[pendingPullIntos]][0] is pullIntoDescriptor.
  DCHECK(controller->pending_pull_intos_.IsEmpty() ||
         controller->pending_pull_intos_[0] == pull_into_descriptor);
  // 2. Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
  InvalidateBYOBRequest(controller);
  // 3. Set pullIntoDescriptor’s bytes filled to bytes filled + size.
  pull_into_descriptor->bytes_filled =
      base::CheckAdd(pull_into_descriptor->bytes_filled, size).ValueOrDie();
}

bool ReadableByteStreamController::FillPullIntoDescriptorFromQueue(
    ReadableByteStreamController* controller,
    PullIntoDescriptor* pull_into_descriptor) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-pull-into-descriptor-from-queue
  // 1. Let elementSize be pullIntoDescriptor.[[elementSize]].
  const size_t element_size = pull_into_descriptor->element_size;
  // 2. Let currentAlignedBytes be pullIntoDescriptor's bytes filled −
  // (pullIntoDescriptor's bytes filled mod elementSize).
  const size_t current_aligned_bytes =
      pull_into_descriptor->bytes_filled -
      (pull_into_descriptor->bytes_filled % element_size);
  // 3. Let maxBytesToCopy be min(controller.[[queueTotalSize]],
  // pullIntoDescriptor’s byte length − pullIntoDescriptor’s bytes filled).
  // The subtraction will not underflow because bytes length will always be more
  // than or equal to bytes filled.
  const size_t max_bytes_to_copy = std::min(
      static_cast<size_t>(controller->queue_total_size_),
      pull_into_descriptor->byte_length - pull_into_descriptor->bytes_filled);
  // 4. Let maxBytesFilled be pullIntoDescriptor’s bytes filled +
  // maxBytesToCopy.
  // This addition will not overflow because maxBytesToCopy can be at most
  // queue_total_size_. Both bytes_filled and queue_total_size_ refer to
  // actually allocated memory, so together they cannot exceed size_t.
  const size_t max_bytes_filled =
      pull_into_descriptor->bytes_filled + max_bytes_to_copy;
  // 5. Let maxAlignedBytes be maxBytesFilled − (maxBytesFilled mod
  // elementSize).
  // This subtraction will not underflow because the modulus operator is
  // guaranteed to return a value less than or equal to the first argument.
  const size_t max_aligned_bytes =
      max_bytes_filled - (max_bytes_filled % element_size);
  // 6. Let totalBytesToCopyRemaining be maxBytesToCopy.
  size_t total_bytes_to_copy_remaining = max_bytes_to_copy;
  // 7. Let ready be false;
  bool ready = false;
  // 8. If maxAlignedBytes > currentAlignedBytes,
  if (max_aligned_bytes > current_aligned_bytes) {
    // a. Set totalBytesToCopyRemaining to maxAlignedBytes −
    // pullIntoDescriptor’s bytes filled.
    total_bytes_to_copy_remaining =
        base::CheckSub(max_aligned_bytes, pull_into_descriptor->bytes_filled)
            .ValueOrDie();
    // b. Set ready to true.
    ready = true;
  }
  // 9. Let queue be controller.[[queue]].
  HeapDeque<Member<QueueEntry>>& queue = controller->queue_;
  // 10. While totalBytesToCopyRemaining > 0,
  while (total_bytes_to_copy_remaining > 0) {
    // a. Let headOfQueue be queue[0].
    QueueEntry* head_of_queue = queue[0];
    // b. Let bytesToCopy be min(totalBytesToCopyRemaining,
    // headOfQueue’s byte length).
    size_t bytes_to_copy =
        std::min(total_bytes_to_copy_remaining, head_of_queue->byte_length);
    // c. Let destStart be pullIntoDescriptor’s byte offset +
    // pullIntoDescriptor’s bytes filled.
    // This addition will not overflow because byte offset and bytes filled
    // refer to actually allocated memory, so together they cannot exceed
    // size_t.
    size_t dest_start =
        pull_into_descriptor->byte_offset + pull_into_descriptor->bytes_filled;
    // d. Perform ! CopyDataBlockBytes(pullIntoDescriptor’s
    // buffer.[[ArrayBufferData]], destStart, headOfQueue’s
    // buffer.[[ArrayBufferData]], headOfQueue’s byte offset, bytesToCopy).
    memcpy(
        static_cast<char*>(pull_into_descriptor->buffer->Data()) + dest_start,
        static_cast<char*>(head_of_queue->buffer->Data()) +
            head_of_queue->byte_offset,
        bytes_to_copy);
    // e. If headOfQueue’s byte length is bytesToCopy,
    if (head_of_queue->byte_length == bytes_to_copy) {
      //   i. Remove queue[0].
      queue.pop_front();
    } else {
      // f. Otherwise,
      //   i. Set headOfQueue’s byte offset to headOfQueue’s byte offset +
      //   bytesToCopy.
      head_of_queue->byte_offset =
          base::CheckAdd(head_of_queue->byte_offset, bytes_to_copy)
              .ValueOrDie();
      //   ii. Set headOfQueue’s byte length to headOfQueue’s byte
      //   length − bytesToCopy.
      head_of_queue->byte_length =
          base::CheckSub(head_of_queue->byte_length, bytes_to_copy)
              .ValueOrDie();
    }
    // g. Set controller.[[queueTotalSize]] to controller.[[queueTotalSize]] −
    // bytesToCopy.
    controller->queue_total_size_ =
        base::CheckSub(controller->queue_total_size_, bytes_to_copy)
            .ValueOrDie();
    // h. Perform !
    // ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
    // bytesToCopy, pullIntoDescriptor).
    FillHeadPullIntoDescriptor(controller, bytes_to_copy, pull_into_descriptor);
    // i. Set totalBytesToCopyRemaining to totalBytesToCopyRemaining −
    // bytesToCopy.
    // This subtraction will not underflow because bytes_to_copy will always be
    // greater than or equal to total_bytes_to_copy_remaining.
    total_bytes_to_copy_remaining -= bytes_to_copy;
  }
  // 11. If ready is false,
  if (!ready) {
    // a. Assert: controller.[[queueTotalSize]] is 0.
    DCHECK_EQ(controller->queue_total_size_, 0u);
    // b. Assert: pullIntoDescriptor’s bytes filled > 0.
    DCHECK_GT(pull_into_descriptor->bytes_filled, 0.0);
    // c. Assert: pullIntoDescriptor’s bytes filled < pullIntoDescriptor’s
    // element size.
    DCHECK_LT(pull_into_descriptor->bytes_filled,
              pull_into_descriptor->element_size);
  }
  // 12. Return ready.
  return ready;
}

void ReadableByteStreamController::PullInto(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    NotShared<DOMArrayBufferView> view,
    ReadableStreamBYOBReader::ReadIntoRequest* read_into_request,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-pull-into
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;
  // 2. Let elementSize be 1.
  size_t element_size = 1;
  // 3. Let ctor be %DataView%.
  auto* ctor = &CreateAsArrayBufferView<DOMDataView>;
  // 4. If view has a [[TypedArrayName]] internal slot (i.e., it is not a
  // DataView),
  if (view->GetType() != DOMArrayBufferView::kTypeDataView) {
    //   a. Set elementSize to be the element size specified in the typed array
    //   constructors table for view.[[TypedArrayName]].
    element_size = view->TypeSize();
    //   b. Set ctor to the constructor specified in the typed array
    //   constructors table for view.[[TypedArrayName]].
    switch (view->GetType()) {
      case DOMArrayBufferView::kTypeInt8:
        ctor = &CreateAsArrayBufferView<DOMInt8Array>;
        break;
      case DOMArrayBufferView::kTypeUint8:
        ctor = &CreateAsArrayBufferView<DOMUint8Array>;
        break;
      case DOMArrayBufferView::kTypeUint8Clamped:
        ctor = &CreateAsArrayBufferView<DOMUint8ClampedArray>;
        break;
      case DOMArrayBufferView::kTypeInt16:
        ctor = &CreateAsArrayBufferView<DOMInt16Array>;
        break;
      case DOMArrayBufferView::kTypeUint16:
        ctor = &CreateAsArrayBufferView<DOMUint16Array>;
        break;
      case DOMArrayBufferView::kTypeInt32:
        ctor = &CreateAsArrayBufferView<DOMInt32Array>;
        break;
      case DOMArrayBufferView::kTypeUint32:
        ctor = &CreateAsArrayBufferView<DOMUint32Array>;
        break;
      case DOMArrayBufferView::kTypeFloat32:
        ctor = &CreateAsArrayBufferView<DOMFloat32Array>;
        break;
      case DOMArrayBufferView::kTypeFloat64:
        ctor = &CreateAsArrayBufferView<DOMFloat64Array>;
        break;
      case DOMArrayBufferView::kTypeBigInt64:
        ctor = &CreateAsArrayBufferView<DOMBigInt64Array>;
        break;
      case DOMArrayBufferView::kTypeBigUint64:
        ctor = &CreateAsArrayBufferView<DOMBigUint64Array>;
        break;
      case DOMArrayBufferView::kTypeDataView:
        NOTREACHED();
    }
  }
  // 5. Let byteOffset be view.[[ByteOffset]].
  const size_t byte_offset = view->byteOffset();
  // 6. Let byteLength be view.[[ByteLength]].
  const size_t byte_length = view->byteLength();
  // 7. Let buffer be ! TransferArrayBuffer(view.[[ViewedArrayBuffer]]).
  DOMArrayBuffer* buffer =
      TransferArrayBuffer(script_state, view->buffer(), exception_state);
  // 8. Let pullIntoDescriptor be a new pull-into descriptor with buffer buffer,
  // byte offset byteOffset, byte length byteLength, bytes filled 0, element
  // size elementSize, view construcot ctor, and reader type "byob".
  PullIntoDescriptor* pull_into_descriptor =
      MakeGarbageCollected<PullIntoDescriptor>(buffer, byte_offset, byte_length,
                                               0, element_size, ctor,
                                               ReaderType::kBYOB);
  // 9. If controller.[[pendingPullIntos]] is not empty,
  if (!controller->pending_pull_intos_.IsEmpty()) {
    //   a. Append pullIntoDescriptor to controller.[[pendingPullIntos]].
    controller->pending_pull_intos_.push_back(pull_into_descriptor);
    //   b. Perform ! ReadableStreamAddReadIntoRequest(stream, readIntoRequest).
    ReadableStream::AddReadIntoRequest(script_state, stream, read_into_request);
    //   c. Return.
    return;
  }
  // 10. If stream.[[state]] is "closed",
  if (stream->state_ == ReadableStream::kClosed) {
    //   a. Let emptyView be ! Construct(ctor, « pullIntoDescriptor’s buffer,
    //   pullIntoDescriptor’s byte offset, 0 »).
    DOMArrayBufferView* emptyView = ctor(pull_into_descriptor->buffer,
                                         pull_into_descriptor->byte_offset, 0);
    //   b. Perform readIntoRequest’s close steps, given emptyView.
    read_into_request->CloseSteps(script_state, emptyView);
    //   c. Return.
    return;
  }
  // 11. If controller.[[queueTotalSize]] > 0,
  if (controller->queue_total_size_ > 0) {
    //   a. If !
    //   ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(controller,
    //   pullIntoDescriptor) is true,
    if (FillPullIntoDescriptorFromQueue(controller, pull_into_descriptor)) {
      //     i. Let filledView be !
      //     ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
      DOMArrayBufferView* filled_view =
          ConvertPullIntoDescriptor(pull_into_descriptor);
      //     ii. Perform !
      //     ReadableByteStreamControllerHandleQueueDrain(controller).
      HandleQueueDrain(script_state, controller);
      //     iii. Perform readIntoRequest’s chunk steps, given filledView.
      read_into_request->ChunkSteps(script_state, filled_view);
      //     iv. Return.
      return;
    }
    //   b. If controller.[[closeRequested]] is true,
    if (controller->close_requested_) {
      //     i. Let e be a TypeError exception.
      v8::Local<v8::Value> e = V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), "close requested");
      //     ii. Perform ! ReadableByteStreamControllerError(controller, e).
      controller->Error(script_state, controller, e);
      //     iii. Perform readIntoRequest’s error steps, given e.
      read_into_request->ErrorSteps(script_state, e);
      //     iv. Return.
      return;
    }
  }
  // 12. Append pullIntoDescriptor to controller.[[pendingPullIntos]].
  controller->pending_pull_intos_.push_back(pull_into_descriptor);
  // 13. Perform ! ReadableStreamAddReadIntoRequest(stream, readIntoRequest).
  ReadableStream::AddReadIntoRequest(script_state, stream, read_into_request);
  // 14. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableByteStreamController::HandleQueueDrain(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-handle-queue-drain
  // 1. Assert: controller.[[stream]].[[state]] is "readable".
  DCHECK_EQ(controller->controlled_readable_stream_->state_,
            ReadableStream::kReadable);
  // 2. If controller.[[queueTotalSize]] is 0 and controller.[[closeRequested]]
  // is true,
  if (!controller->queue_total_size_ && controller->close_requested_) {
    //   a. Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
    ClearAlgorithms(controller);
    //   b. Perform ! ReadableStreamClose(controller.[[stream]]).
    ReadableStream::Close(script_state,
                          controller->controlled_readable_stream_);
  } else {
    // 3. Otherwise,
    //   a. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
    CallPullIfNeeded(script_state, controller);
  }
}

void ReadableByteStreamController::ResetQueue(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#reset-queue
  // 1. Assert: container has [[queue]] and [[queueTotalSize]] internal slots.
  // 2. Set container.[[queue]] to a new empty list.
  controller->queue_.clear();
  // 3. Set container.[[queueTotalSize]] to 0.
  controller->queue_total_size_ = 0;
}

void ReadableByteStreamController::Respond(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    size_t bytes_written,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond
  // 1. Assert: controller.[[pendingPullIntos]] is not empty.
  DCHECK(!controller->pending_pull_intos_.IsEmpty());
  // 2. Perform ? ReadableByteStreamControllerRespondInternal(controller,
  // bytesWritten).
  RespondInternal(script_state, controller, bytes_written, exception_state);
}

void ReadableByteStreamController::RespondInClosedState(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    PullIntoDescriptor* first_descriptor,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-closed-state
  // 1. Set firstDescriptor’s buffer to ! TransferArrayBuffer(firstDescriptor’s
  // buffer).
  first_descriptor->buffer = TransferArrayBuffer(
      script_state, first_descriptor->buffer, exception_state);
  // 2. Assert: firstDescriptor’s bytes filled is 0.
  DCHECK_EQ(first_descriptor->bytes_filled, 0u);
  // 3. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;
  // 4. If ! ReadableStreamHasBYOBReader(stream) is true,
  if (ReadableStream::HasBYOBReader(stream)) {
    //   a. While ! ReadableStreamGetNumReadIntoRequests(stream) > 0,
    while (ReadableStream::GetNumReadIntoRequests(stream) > 0) {
      //     i. Let pullIntoDescriptor be !
      //     ReadableByteStreamControllerShiftPendingPullInto(controller).
      PullIntoDescriptor* pull_into_descriptor =
          ShiftPendingPullInto(controller);
      //     ii. Perform !
      //     ReadableByteStreamControllerCommitPullIntoDescriptor(stream,
      //     pullIntoDescriptor).
      CommitPullIntoDescriptor(script_state, stream, pull_into_descriptor);
    }
  }
}

void ReadableByteStreamController::RespondInReadableState(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    size_t bytes_written,
    PullIntoDescriptor* pull_into_descriptor,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-readable-state
  // 1. If pullIntoDescriptor’s bytes filled + bytesWritten >
  // pullIntoDescriptor’s byte length, throw a RangeError exception.
  if (base::ClampAdd(pull_into_descriptor->bytes_filled, bytes_written) >
      pull_into_descriptor->byte_length) {
    exception_state.ThrowRangeError(
        "Cannot respond because buffer will overflow if written to");
    return;
  }
  // 2. Perform !
  // ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
  // bytesWritten, pullIntoDescriptor).
  FillHeadPullIntoDescriptor(controller, bytes_written, pull_into_descriptor);
  // 3. If pullIntoDescriptor’s bytes filled < pullIntoDescriptor’s element
  // size, return.
  if (pull_into_descriptor->bytes_filled < pull_into_descriptor->element_size) {
    return;
  }
  // 4. Perform ! ReadableByteStreamControllerShiftPendingPullInto(controller).
  ShiftPendingPullInto(controller);
  // 5. Let remainderSize be pullIntoDescriptor’s bytes filled mod
  // pullIntoDescriptor’s element size.
  const size_t remainder_size =
      pull_into_descriptor->bytes_filled % pull_into_descriptor->element_size;
  // 6. If remainderSize > 0,
  if (remainder_size > 0) {
    //   a. Let end be pullIntoDescriptor’s byte offset + pullIntoDescriptor’s
    //   bytes filled.
    //   This addition will not overflow because byte offset and bytes filled
    //   refer to actually allocated memory, so together they cannot exceed
    //   size_t.
    size_t end =
        pull_into_descriptor->byte_offset + pull_into_descriptor->bytes_filled;
    //   b. Let remainder be ? CloneArrayBuffer(pullIntoDescriptor’s
    //   buffer, end − remainderSize, remainderSize, %ArrayBuffer%).
    DOMArrayBuffer* const remainder = DOMArrayBuffer::Create(
        static_cast<char*>(pull_into_descriptor->buffer->Data()) + end -
            remainder_size,
        remainder_size);
    //   c. Perform !
    //   ReadableByteStreamControllerEnqueueChunkToQueue(controller, remainder,
    //   0, remainder.[[ByteLength]]).
    EnqueueChunkToQueue(controller, remainder, 0, remainder->ByteLength());
  }
  // 7. Set pullIntoDescriptor’s buffer to !
  // TransferArrayBuffer(pullIntoDescriptor’s buffer).
  pull_into_descriptor->buffer = TransferArrayBuffer(
      script_state, pull_into_descriptor->buffer, exception_state);
  // 8. Set pullIntoDescriptor’s bytes filled to pullIntoDescriptor’s bytes
  // filled − remainderSize.
  pull_into_descriptor->bytes_filled =
      pull_into_descriptor->bytes_filled - remainder_size;
  // 9. Perform !
  // ReadableByteStreamControllerCommitPullIntoDescriptor(controller.[[stream]],
  // pullIntoDescriptor).
  CommitPullIntoDescriptor(script_state,
                           controller->controlled_readable_stream_,
                           pull_into_descriptor);
  // 10. Perform !
  // ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
  ProcessPullIntoDescriptorsUsingQueue(script_state, controller);
}

void ReadableByteStreamController::RespondInternal(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    size_t bytes_written,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-internal
  // 1. Let firstDescriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* const first_descriptor =
      controller->pending_pull_intos_[0];
  // 2. Let state be controller.[[stream]].[[state]].
  const ReadableStream::State state =
      controller->controlled_readable_stream_->state_;
  // 3. If state is "closed",
  if (state == ReadableStream::kClosed) {
    //   a. If bytesWritten is not 0, throw a TypeError exception.
    if (bytes_written != 0) {
      exception_state.ThrowTypeError("bytes written is not 0");
      return;
    }
    //   b. Perform !
    //   ReadableByteStreamControllerRespondInClosedState(controller,
    //   firstDescriptor).
    RespondInClosedState(script_state, controller, first_descriptor,
                         exception_state);
  } else {
    // 4. Otherwise,
    //   a. Assert: state is "readable".
    DCHECK_EQ(state, ReadableStream::kReadable);
    //   b. Perform ?
    //   ReadableByteStreamControllerRespondInReadableState(controller,
    //   bytesWritten, firstDescriptor).
    RespondInReadableState(script_state, controller, bytes_written,
                           first_descriptor, exception_state);
  }
  // 5. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableByteStreamController::RespondWithNewView(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    NotShared<DOMArrayBufferView> view,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-with-new-view
  // 1. Assert: controller.[[pendingPullIntos]] is not empty.
  DCHECK(!controller->pending_pull_intos_.IsEmpty());
  // 2. Let firstDescriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* first_descriptor = controller->pending_pull_intos_[0];
  // 3. If firstDescriptor’s byte offset + firstDescriptor’ bytes filled is not
  // view.[[ByteOffset]], throw a RangeError exception.
  // We don't expect this addition to overflow as the bytes are expected to be
  // equal.
  if (first_descriptor->byte_offset + first_descriptor->bytes_filled !=
      view->byteOffset()) {
    exception_state.ThrowRangeError(
        "First descriptor's byte offset and bytes filled is not the same as "
        "the view's byte offset");
    return;
  }
  // 4. If firstDescriptor’s byte length is not view.[[ByteLength]], throw a
  // RangeError exception.
  if (first_descriptor->byte_length != view->byteLength()) {
    exception_state.ThrowRangeError("byte lengths are not equal");
    return;
  }
  // 5. Set firstDescriptor’s buffer to view.[[ViewedArrayBuffer]].
  first_descriptor->buffer = view->buffer();
  // 6. Perform ? ReadableByteStreamControllerRespondInternal(controller,
  // view.[[ByteLength]]).
  RespondInternal(script_state, controller, view->byteLength(),
                  exception_state);
}

DOMArrayBuffer* ReadableByteStreamController::TransferArrayBuffer(
    ScriptState* script_state,
    DOMArrayBuffer* buffer,
    ExceptionState& exception_state) {
  ArrayBufferContents contents;
  if (buffer->IsDetachable(script_state->GetIsolate()) &&
      buffer->Transfer(script_state->GetIsolate(), contents)) {
    return DOMArrayBuffer::Create(std::move(contents));
  }
  exception_state.ThrowTypeError("not able to transfer array buffer");
  return nullptr;
}

void ReadableByteStreamController::Trace(Visitor* visitor) const {
  visitor->Trace(byob_request_);
  visitor->Trace(cancel_algorithm_);
  visitor->Trace(controlled_readable_stream_);
  visitor->Trace(pending_pull_intos_);
  visitor->Trace(pull_algorithm_);
  visitor->Trace(queue_);
  ScriptWrappable::Trace(visitor);
}

//
// Readable byte stream controller internal methods
//

v8::Local<v8::Promise> ReadableByteStreamController::CancelSteps(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#rbs-controller-private-cancel
  // 1. If this.[[pendingPullIntos]] is not empty,
  if (!pending_pull_intos_.IsEmpty()) {
    //   a. Let firstDescriptor be this.[[pendingPullIntos]][0].
    PullIntoDescriptor* first_descriptor = pending_pull_intos_[0];
    //   b. Set firstDescriptor’s bytes filled to 0.
    first_descriptor->bytes_filled = 0;
  }
  // 2. Perform ! ResetQueue(this).
  ResetQueue(this);
  // 3. Let result be the result of performing this.[[cancelAlgorithm]], passing
  // in reason.
  auto result = cancel_algorithm_->Run(script_state, 1, &reason);
  // 4. Perform ! ReadableByteStreamControllerClearAlgorithms(this).
  ClearAlgorithms(this);
  // 5. Return result.
  return result;
}

StreamPromiseResolver* ReadableByteStreamController::PullSteps(
    ScriptState* script_state) {
  // https://whatpr.org/streams/1029.html#rbs-controller-private-pull
  // TODO: This function follows an old version of the spec referenced above, so
  // it needs to be updated to the new version on
  // https://streams.spec.whatwg.org when the ReadableStreamDefaultReader
  // implementation is updated.
  // 1. Let stream be this.[[controlledReadableByteStream]].
  ReadableStream* const stream = controlled_readable_stream_;
  // 2. Assert: ! ReadableStreamHasDefaultReader(stream) is true.
  DCHECK(ReadableStream::HasDefaultReader(stream));
  // 3. If this.[[queueTotalSize]] > 0,
  if (queue_total_size_ > 0) {
    //   a. Assert: ! ReadableStreamGetNumReadRequests(stream) is 0.
    DCHECK_EQ(ReadableStream::GetNumReadRequests(stream), 0);
    //   b. Let entry be the first element of this.[[queue]].
    QueueEntry* entry = queue_[0];
    //   c. Remove entry from this.[[queue]], shifting all other elements
    //   downward (so that the second becomes the first, and so on).
    queue_.pop_front();
    //   d. Set this.[[queueTotalSize]] to this.[[queueTotalSize]] −
    //   entry.[[byteLength]].
    queue_total_size_ -= entry->byte_length;
    //   e. Perform ! ReadableByteStreamControllerHandleQueueDrain(this).
    HandleQueueDrain(script_state, this);
    //   f. Let view be ! Construct(%Uint8Array%, « entry.[[buffer]],
    //   entry.[[byteOffset]], entry.[[byteLength]] »).
    DOMUint8Array* view = DOMUint8Array::Create(
        entry->buffer, entry->byte_offset, entry->byte_length);
    //   g. Return a promise resolved with !
    //   ReadableStreamCreateReadResult(view, false,
    //   stream.[[reader]].[[forAuthorCode]]).
    ReadableStreamGenericReader* reader = stream->reader_;
    return StreamPromiseResolver::CreateResolved(
        script_state,
        ReadableStream::CreateReadResult(
            script_state, ToV8(view, script_state), false,
            To<ReadableStreamDefaultReader>(reader)->for_author_code_));
  }
  // 4. Let autoAllocateChunkSize be this.[[autoAllocateChunkSize]].
  const size_t auto_allocate_chunk_size = auto_allocate_chunk_size_;
  // 5. If autoAllocateChunkSize is not undefined,
  if (auto_allocate_chunk_size) {
    //   a. Let buffer be Construct(%ArrayBuffer%, « autoAllocateChunkSize »).
    auto* buffer = DOMArrayBuffer::Create(auto_allocate_chunk_size, 1);
    //   b. If buffer is an abrupt completion, return a promise rejected with
    //   buffer.[[Value]].
    //   This is not needed as DOMArrayBuffer::Create() is designed to
    //   crash if it cannot allocate the memory.

    //   c. Let pullIntoDescriptor be Record {[[buffer]]: buffer.[[Value]],
    //   [[byteOffset]]: 0, [[byteLength]]: autoAllocateChunkSize,
    //   [[bytesFilled]]: 0, [[elementSize]]: 1, [[ctor]]: %Uint8Array%,
    //   [[readerType]]: "default"}.
    auto* ctor = &CreateAsArrayBufferView<DOMUint8Array>;
    PullIntoDescriptor* pull_into_descriptor =
        MakeGarbageCollected<PullIntoDescriptor>(buffer, 0,
                                                 auto_allocate_chunk_size, 0, 1,
                                                 ctor, ReaderType::kDefault);
    //   d. Append pullIntoDescriptor as the last element of
    //   this.[[pendingPullIntos]].
    pending_pull_intos_.push_back(pull_into_descriptor);
  }
  // 6. Let promise be ! ReadableStreamAddReadRequest(stream).
  StreamPromiseResolver* promise =
      ReadableStream::AddReadRequest(script_state, stream);
  // 7. Perform ! ReadableByteStreamControllerCallPullIfNeeded(this).
  CallPullIfNeeded(script_state, this);
  // 8. Return promise.
  return promise;
}

}  // namespace blink

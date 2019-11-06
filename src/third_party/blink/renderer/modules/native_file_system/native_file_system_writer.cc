// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_writer.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

NativeFileSystemWriter::NativeFileSystemWriter(NativeFileSystemFileHandle* file)
    : file_(file) {
  DCHECK(file_);
}

ScriptPromise NativeFileSystemWriter::write(
    ScriptState* script_state,
    uint64_t position,
    const ArrayBufferOrArrayBufferViewOrBlobOrUSVString& data,
    ExceptionState& exception_state) {
  DCHECK(!data.IsNull());

  auto blob_data = std::make_unique<BlobData>();
  Blob* blob = nullptr;
  if (data.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = data.GetAsArrayBuffer();
    blob_data->AppendBytes(array_buffer->Data(), array_buffer->ByteLength());
  } else if (data.IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view = data.GetAsArrayBufferView().View();
    blob_data->AppendBytes(array_buffer_view->BaseAddress(),
                           array_buffer_view->byteLength());
  } else if (data.IsBlob()) {
    blob = data.GetAsBlob();
  } else if (data.IsUSVString()) {
    // Let the developer be explicit about line endings.
    blob_data->AppendText(data.GetAsUSVString(),
                          /*normalize_line_endings_to_native=*/false);
  }

  if (!blob) {
    uint64_t size = blob_data->length();
    blob = Blob::Create(BlobDataHandle::Create(std::move(blob_data), size));
  }

  return WriteBlob(script_state, position, blob);
}

ScriptPromise NativeFileSystemWriter::WriteBlob(ScriptState* script_state,
                                                uint64_t position,
                                                Blob* blob) {
  if (!file_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  file_->MojoHandle()->Write(
      position, blob->AsMojoBlob(),
      WTF::Bind(&NativeFileSystemWriter::WriteComplete, WrapPersistent(this)));
  return result;
}

class NativeFileSystemWriter::StreamWriterClient
    : public GarbageCollectedFinalized<StreamWriterClient>,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(StreamWriterClient);

 public:
  explicit StreamWriterClient(NativeFileSystemWriter* writer)
      : writer_(writer) {}

  void DidFetchDataStartedDataPipe(
      mojo::ScopedDataPipeConsumerHandle data_pipe) override {
    data_pipe_ = std::move(data_pipe);
  }

  mojo::ScopedDataPipeConsumerHandle TakeDataPipe() {
    DCHECK(data_pipe_);
    return std::move(data_pipe_);
  }

  void DidFetchDataLoadedDataPipe() override {
    // WriteComplete could have been called with an error before we reach this
    // point, in that case just return.
    if (did_complete_)
      return;
    DCHECK(!did_finish_writing_to_pipe_);
    DCHECK(writer_->pending_operation_);
    did_finish_writing_to_pipe_ = true;
  }

  void DidFetchDataLoadFailed() override {
    // WriteComplete could have been called with an error before we reach this
    // point, in that case just return.
    if (did_complete_)
      return;
    DCHECK(writer_->pending_operation_);
    did_complete_ = true;
    writer_->pending_operation_->Reject(
        file_error::CreateDOMException(base::File::FILE_ERROR_FAILED));
    Reset();
  }

  void Abort() override {
    // WriteComplete could have been called with an error before we reach this
    // point, in that case just return.
    if (did_complete_)
      return;
    DCHECK(writer_->pending_operation_);
    did_complete_ = true;
    writer_->pending_operation_->Reject(
        file_error::CreateDOMException(base::File::FILE_ERROR_ABORT));
    Reset();
  }

  void WriteComplete(mojom::blink::NativeFileSystemErrorPtr result,
                     uint64_t bytes_written) {
    // Early return if we already completed (with an error) before.
    if (did_complete_)
      return;
    DCHECK(writer_->pending_operation_);
    did_complete_ = true;
    if (result->error_code != base::File::FILE_OK) {
      writer_->pending_operation_->Reject(
          file_error::CreateDOMException(result->error_code));
    } else {
      DCHECK(did_finish_writing_to_pipe_);
      writer_->pending_operation_->Resolve();
    }
    Reset();
  }

  void Trace(Visitor* visitor) override {
    Client::Trace(visitor);
    visitor->Trace(writer_);
  }

 private:
  void Reset() {
    writer_->pending_operation_ = nullptr;
    writer_->stream_loader_ = nullptr;
  }

  Member<NativeFileSystemWriter> writer_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  bool did_finish_writing_to_pipe_ = false;
  bool did_complete_ = false;
};

ScriptPromise NativeFileSystemWriter::WriteStream(
    ScriptState* script_state,
    uint64_t position,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  if (!file_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError));
  }
  DCHECK(!stream_loader_);

  auto* consumer = MakeGarbageCollected<ReadableStreamBytesConsumer>(
      script_state, stream, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  stream_loader_ = FetchDataLoader::CreateLoaderAsDataPipe(
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kInternalDefault));
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  auto* client = MakeGarbageCollected<StreamWriterClient>(this);
  stream_loader_->Start(consumer, client);
  file_->MojoHandle()->WriteStream(
      position, client->TakeDataPipe(),
      WTF::Bind(&StreamWriterClient::WriteComplete, WrapPersistent(client)));
  return result;
}

ScriptPromise NativeFileSystemWriter::truncate(ScriptState* script_state,
                                               uint64_t size) {
  if (!file_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  file_->MojoHandle()->Truncate(
      size, WTF::Bind(&NativeFileSystemWriter::TruncateComplete,
                      WrapPersistent(this)));
  return result;
}

ScriptPromise NativeFileSystemWriter::close(ScriptState* script_state) {
  if (!file_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError));
  }
  file_ = nullptr;
  return ScriptPromise::CastUndefined(script_state);
}

void NativeFileSystemWriter::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(file_);
  visitor->Trace(pending_operation_);
  visitor->Trace(stream_loader_);
}

void NativeFileSystemWriter::WriteComplete(
    mojom::blink::NativeFileSystemErrorPtr result,
    uint64_t bytes_written) {
  DCHECK(pending_operation_);
  if (result->error_code == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    pending_operation_->Reject(
        file_error::CreateDOMException(result->error_code));
  }
  pending_operation_ = nullptr;
}

void NativeFileSystemWriter::TruncateComplete(
    mojom::blink::NativeFileSystemErrorPtr result) {
  DCHECK(pending_operation_);
  if (result->error_code == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    pending_operation_->Reject(
        file_error::CreateDOMException(result->error_code));
  }
  pending_operation_ = nullptr;
}

}  // namespace blink

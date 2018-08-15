// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_writer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemWriter::FileSystemWriter(mojom::blink::FileWriterPtr writer)
    : writer_(std::move(writer)) {
  DCHECK(writer_);
}

ScriptPromise FileSystemWriter::write(ScriptState* script_state,
                                      uint64_t position,
                                      Blob* blob) {
  if (!writer_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_->Write(
      position, blob->AsMojoBlob(),
      WTF::Bind(&FileSystemWriter::WriteComplete, WrapPersistent(this)));
  return result;
}

ScriptPromise FileSystemWriter::truncate(ScriptState* script_state,
                                         uint64_t size) {
  if (!writer_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_->Truncate(size, WTF::Bind(&FileSystemWriter::TruncateComplete,
                                    WrapPersistent(this)));
  return result;
}

ScriptPromise FileSystemWriter::close(ScriptState* script_state) {
  if (!writer_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  writer_ = nullptr;
  return ScriptPromise::CastUndefined(script_state);
}

void FileSystemWriter::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(pending_operation_);
}

void FileSystemWriter::WriteComplete(base::File::Error result,
                                     uint64_t bytes_written) {
  DCHECK(pending_operation_);
  if (result == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    // TODO(mek): Take actual error code into account.
    pending_operation_->Reject(
        DOMException::Create(DOMExceptionCode::kAbortError));
  }
}

void FileSystemWriter::TruncateComplete(base::File::Error result) {
  DCHECK(pending_operation_);
  if (result == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    // TODO(mek): Take actual error code into account.
    pending_operation_->Reject(
        DOMException::Create(DOMExceptionCode::kAbortError));
  }
}

}  // namespace blink

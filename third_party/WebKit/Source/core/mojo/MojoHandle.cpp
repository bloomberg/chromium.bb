// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/MojoHandle.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/ExecutionContext.h"
#include "core/mojo/MojoCreateSharedBufferResult.h"
#include "core/mojo/MojoDiscardDataOptions.h"
#include "core/mojo/MojoDuplicateBufferHandleOptions.h"
#include "core/mojo/MojoMapBufferResult.h"
#include "core/mojo/MojoReadDataOptions.h"
#include "core/mojo/MojoReadDataResult.h"
#include "core/mojo/MojoReadMessageFlags.h"
#include "core/mojo/MojoReadMessageResult.h"
#include "core/mojo/MojoWatcher.h"
#include "core/mojo/MojoWriteDataOptions.h"
#include "core/mojo/MojoWriteDataResult.h"

// Mojo messages typically do not contain many handles. In fact most
// messages do not contain any handle. An inline capacity of 4 should avoid
// heap allocation in vast majority of cases.
static const size_t kHandleVectorInlineCapacity = 4;

namespace blink {

MojoHandle* MojoHandle::Create(mojo::ScopedHandle handle) {
  return new MojoHandle(std::move(handle));
}

MojoHandle::MojoHandle(mojo::ScopedHandle handle)
    : handle_(std::move(handle)) {}

void MojoHandle::close() {
  handle_.reset();
}

MojoWatcher* MojoHandle::watch(ScriptState* script_state,
                               const MojoHandleSignals& signals,
                               MojoWatchCallback* callback) {
  return MojoWatcher::Create(handle_.get(), signals, callback,
                             ExecutionContext::From(script_state));
}

MojoResult MojoHandle::writeMessage(
    ArrayBufferOrArrayBufferView& buffer,
    const HeapVector<Member<MojoHandle>>& handles) {
  // MojoWriteMessage takes ownership of the handles, so release them here.
  Vector<::MojoHandle, kHandleVectorInlineCapacity> raw_handles(handles.size());
  std::transform(
      handles.begin(), handles.end(), raw_handles.begin(),
      [](MojoHandle* handle) { return handle->handle_.release().value(); });

  const void* bytes = nullptr;
  uint32_t num_bytes = 0;
  if (buffer.isArrayBuffer()) {
    DOMArrayBuffer* array = buffer.getAsArrayBuffer();
    bytes = array->Data();
    num_bytes = array->ByteLength();
  } else {
    DOMArrayBufferView* view = buffer.getAsArrayBufferView().View();
    bytes = view->BaseAddress();
    num_bytes = view->byteLength();
  }

  return MojoWriteMessage(handle_->value(), bytes, num_bytes,
                          raw_handles.Data(), raw_handles.size(),
                          MOJO_WRITE_MESSAGE_FLAG_NONE);
}

void MojoHandle::readMessage(const MojoReadMessageFlags& flags_dict,
                             MojoReadMessageResult& result_dict) {
  ::MojoReadMessageFlags flags = MOJO_READ_MESSAGE_FLAG_NONE;
  if (flags_dict.mayDiscard())
    flags |= MOJO_READ_MESSAGE_FLAG_MAY_DISCARD;

  uint32_t num_bytes = 0, num_handles = 0;
  MojoResult result = MojoReadMessage(handle_->value(), nullptr, &num_bytes,
                                      nullptr, &num_handles, flags);
  if (result != MOJO_RESULT_RESOURCE_EXHAUSTED) {
    result_dict.setResult(result);
    return;
  }

  DOMArrayBuffer* buffer =
      DOMArrayBuffer::CreateUninitializedOrNull(num_bytes, 1);
  CHECK(buffer);
  Vector<::MojoHandle, kHandleVectorInlineCapacity> raw_handles(num_handles);
  result = MojoReadMessage(handle_->value(), buffer->Data(), &num_bytes,
                           raw_handles.Data(), &num_handles, flags);

  HeapVector<Member<MojoHandle>> handles(num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
    handles[i] = MojoHandle::Create(
        mojo::MakeScopedHandle(mojo::Handle(raw_handles[i])));
  }

  result_dict.setResult(result);
  result_dict.setBuffer(buffer);
  result_dict.setHandles(handles);
}

void MojoHandle::writeData(const ArrayBufferOrArrayBufferView& buffer,
                           const MojoWriteDataOptions& options_dict,
                           MojoWriteDataResult& result_dict) {
  MojoWriteDataFlags flags = MOJO_WRITE_DATA_FLAG_NONE;
  if (options_dict.allOrNone())
    flags |= MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;

  const void* elements = nullptr;
  uint32_t num_bytes = 0;
  if (buffer.isArrayBuffer()) {
    DOMArrayBuffer* array = buffer.getAsArrayBuffer();
    elements = array->Data();
    num_bytes = array->ByteLength();
  } else {
    DOMArrayBufferView* view = buffer.getAsArrayBufferView().View();
    elements = view->BaseAddress();
    num_bytes = view->byteLength();
  }

  MojoResult result =
      MojoWriteData(handle_->value(), elements, &num_bytes, flags);
  result_dict.setResult(result);
  result_dict.setNumBytes(result == MOJO_RESULT_OK ? num_bytes : 0);
}

void MojoHandle::queryData(MojoReadDataResult& result_dict) {
  uint32_t num_bytes = 0;
  MojoResult result = MojoReadData(handle_->value(), nullptr, &num_bytes,
                                   MOJO_READ_DATA_FLAG_QUERY);
  result_dict.setResult(result);
  result_dict.setNumBytes(num_bytes);
}

void MojoHandle::discardData(unsigned num_bytes,
                             const MojoDiscardDataOptions& options_dict,
                             MojoReadDataResult& result_dict) {
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_DISCARD;
  if (options_dict.allOrNone())
    flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;

  MojoResult result =
      MojoReadData(handle_->value(), nullptr, &num_bytes, flags);
  result_dict.setResult(result);
  result_dict.setNumBytes(result == MOJO_RESULT_OK ? num_bytes : 0);
}

void MojoHandle::readData(ArrayBufferOrArrayBufferView& buffer,
                          const MojoReadDataOptions& options_dict,
                          MojoReadDataResult& result_dict) {
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_NONE;
  if (options_dict.allOrNone())
    flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  if (options_dict.peek())
    flags |= MOJO_READ_DATA_FLAG_PEEK;

  void* elements = nullptr;
  unsigned num_bytes = 0;
  if (buffer.isArrayBuffer()) {
    DOMArrayBuffer* array = buffer.getAsArrayBuffer();
    elements = array->Data();
    num_bytes = array->ByteLength();
  } else {
    DOMArrayBufferView* view = buffer.getAsArrayBufferView().View();
    elements = view->BaseAddress();
    num_bytes = view->byteLength();
  }

  MojoResult result =
      MojoReadData(handle_->value(), elements, &num_bytes, flags);
  result_dict.setResult(result);
  result_dict.setNumBytes(result == MOJO_RESULT_OK ? num_bytes : 0);
}

void MojoHandle::mapBuffer(unsigned offset,
                           unsigned num_bytes,
                           MojoMapBufferResult& result_dict) {
  void* data = nullptr;
  MojoResult result = MojoMapBuffer(handle_->value(), offset, num_bytes, &data,
                                    MOJO_MAP_BUFFER_FLAG_NONE);
  result_dict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    WTF::ArrayBufferContents::DataHandle data_handle(data, [](void* buffer) {
      MojoResult result = MojoUnmapBuffer(buffer);
      DCHECK_EQ(result, MOJO_RESULT_OK);
    });
    WTF::ArrayBufferContents contents(std::move(data_handle), num_bytes,
                                      WTF::ArrayBufferContents::kNotShared);
    result_dict.setBuffer(DOMArrayBuffer::Create(contents));
  }
}

void MojoHandle::duplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions& options_dict,
    MojoCreateSharedBufferResult& result_dict) {
  ::MojoDuplicateBufferHandleOptions options = {
      sizeof(options), MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE};
  if (options_dict.readOnly())
    options.flags |= MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY;

  mojo::Handle handle;
  MojoResult result = MojoDuplicateBufferHandle(handle_->value(), &options,
                                                handle.mutable_value());
  result_dict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict.setHandle(MojoHandle::Create(mojo::MakeScopedHandle(handle)));
  }
}

}  // namespace blink

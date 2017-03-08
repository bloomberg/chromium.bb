// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/MojoHandle.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
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

MojoHandle* MojoHandle::create(mojo::ScopedHandle handle) {
  return new MojoHandle(std::move(handle));
}

MojoHandle::MojoHandle(mojo::ScopedHandle handle)
    : m_handle(std::move(handle)) {}

void MojoHandle::close() {
  m_handle.reset();
}

MojoWatcher* MojoHandle::watch(ScriptState* scriptState,
                               const MojoHandleSignals& signals,
                               MojoWatchCallback* callback) {
  return MojoWatcher::create(m_handle.get(), signals, callback,
                             scriptState->getExecutionContext());
}

MojoResult MojoHandle::writeMessage(
    ArrayBufferOrArrayBufferView& buffer,
    const HeapVector<Member<MojoHandle>>& handles) {
  // MojoWriteMessage takes ownership of the handles, so release them here.
  Vector<::MojoHandle, kHandleVectorInlineCapacity> rawHandles(handles.size());
  std::transform(
      handles.begin(), handles.end(), rawHandles.begin(),
      [](MojoHandle* handle) { return handle->m_handle.release().value(); });

  const void* bytes = nullptr;
  uint32_t numBytes = 0;
  if (buffer.isArrayBuffer()) {
    DOMArrayBuffer* array = buffer.getAsArrayBuffer();
    bytes = array->data();
    numBytes = array->byteLength();
  } else {
    DOMArrayBufferView* view = buffer.getAsArrayBufferView();
    bytes = view->baseAddress();
    numBytes = view->byteLength();
  }

  return MojoWriteMessage(m_handle->value(), bytes, numBytes, rawHandles.data(),
                          rawHandles.size(), MOJO_WRITE_MESSAGE_FLAG_NONE);
}

void MojoHandle::readMessage(const MojoReadMessageFlags& flagsDict,
                             MojoReadMessageResult& resultDict) {
  ::MojoReadMessageFlags flags = MOJO_READ_MESSAGE_FLAG_NONE;
  if (flagsDict.mayDiscard())
    flags |= MOJO_READ_MESSAGE_FLAG_MAY_DISCARD;

  uint32_t numBytes = 0, numHandles = 0;
  MojoResult result = MojoReadMessage(m_handle->value(), nullptr, &numBytes,
                                      nullptr, &numHandles, flags);
  if (result != MOJO_RESULT_RESOURCE_EXHAUSTED) {
    resultDict.setResult(result);
    return;
  }

  DOMArrayBuffer* buffer =
      DOMArrayBuffer::createUninitializedOrNull(numBytes, 1);
  CHECK(buffer);
  Vector<::MojoHandle, kHandleVectorInlineCapacity> rawHandles(numHandles);
  result = MojoReadMessage(m_handle->value(), buffer->data(), &numBytes,
                           rawHandles.data(), &numHandles, flags);

  HeapVector<Member<MojoHandle>> handles(numHandles);
  for (size_t i = 0; i < numHandles; ++i) {
    handles[i] =
        MojoHandle::create(mojo::MakeScopedHandle(mojo::Handle(rawHandles[i])));
  }

  resultDict.setResult(result);
  resultDict.setBuffer(buffer);
  resultDict.setHandles(handles);
}

void MojoHandle::writeData(const ArrayBufferOrArrayBufferView& buffer,
                           const MojoWriteDataOptions& optionsDict,
                           MojoWriteDataResult& resultDict) {
  MojoWriteDataFlags flags = MOJO_WRITE_DATA_FLAG_NONE;
  if (optionsDict.allOrNone())
    flags |= MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;

  const void* elements = nullptr;
  uint32_t numBytes = 0;
  if (buffer.isArrayBuffer()) {
    DOMArrayBuffer* array = buffer.getAsArrayBuffer();
    elements = array->data();
    numBytes = array->byteLength();
  } else {
    DOMArrayBufferView* view = buffer.getAsArrayBufferView();
    elements = view->baseAddress();
    numBytes = view->byteLength();
  }

  MojoResult result =
      MojoWriteData(m_handle->value(), elements, &numBytes, flags);
  resultDict.setResult(result);
  resultDict.setNumBytes(result == MOJO_RESULT_OK ? numBytes : 0);
}

void MojoHandle::queryData(MojoReadDataResult& resultDict) {
  uint32_t numBytes = 0;
  MojoResult result = MojoReadData(m_handle->value(), nullptr, &numBytes,
                                   MOJO_READ_DATA_FLAG_QUERY);
  resultDict.setResult(result);
  resultDict.setNumBytes(numBytes);
}

void MojoHandle::discardData(unsigned numBytes,
                             const MojoDiscardDataOptions& optionsDict,
                             MojoReadDataResult& resultDict) {
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_DISCARD;
  if (optionsDict.allOrNone())
    flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;

  MojoResult result =
      MojoReadData(m_handle->value(), nullptr, &numBytes, flags);
  resultDict.setResult(result);
  resultDict.setNumBytes(result == MOJO_RESULT_OK ? numBytes : 0);
}

void MojoHandle::readData(ArrayBufferOrArrayBufferView& buffer,
                          const MojoReadDataOptions& optionsDict,
                          MojoReadDataResult& resultDict) {
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_NONE;
  if (optionsDict.allOrNone())
    flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  if (optionsDict.peek())
    flags |= MOJO_READ_DATA_FLAG_PEEK;

  void* elements = nullptr;
  unsigned numBytes = 0;
  if (buffer.isArrayBuffer()) {
    DOMArrayBuffer* array = buffer.getAsArrayBuffer();
    elements = array->data();
    numBytes = array->byteLength();
  } else {
    DOMArrayBufferView* view = buffer.getAsArrayBufferView();
    elements = view->baseAddress();
    numBytes = view->byteLength();
  }

  MojoResult result =
      MojoReadData(m_handle->value(), elements, &numBytes, flags);
  resultDict.setResult(result);
  resultDict.setNumBytes(result == MOJO_RESULT_OK ? numBytes : 0);
}

void MojoHandle::mapBuffer(unsigned offset,
                           unsigned numBytes,
                           MojoMapBufferResult& resultDict) {
  void* data = nullptr;
  MojoResult result = MojoMapBuffer(m_handle->value(), offset, numBytes, &data,
                                    MOJO_MAP_BUFFER_FLAG_NONE);
  resultDict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    WTF::ArrayBufferContents::DataHandle dataHandle(data, [](void* buffer) {
      MojoResult result = MojoUnmapBuffer(buffer);
      DCHECK_EQ(result, MOJO_RESULT_OK);
    });
    WTF::ArrayBufferContents contents(std::move(dataHandle), numBytes,
                                      WTF::ArrayBufferContents::NotShared);
    resultDict.setBuffer(DOMArrayBuffer::create(contents));
  }
}

void MojoHandle::duplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions& optionsDict,
    MojoCreateSharedBufferResult& resultDict) {
  ::MojoDuplicateBufferHandleOptions options = {
      sizeof(options), MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE};
  if (optionsDict.readOnly())
    options.flags |= MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY;

  mojo::Handle handle;
  MojoResult result = MojoDuplicateBufferHandle(m_handle->value(), &options,
                                                handle.mutable_value());
  resultDict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    resultDict.setHandle(MojoHandle::create(mojo::MakeScopedHandle(handle)));
  }
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushMessageData.h"

#include <memory>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/ArrayBufferOrArrayBufferViewOrUSVString.h"
#include "core/fileapi/Blob.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "platform/bindings/ScriptState.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/TextEncoding.h"
#include "v8/include/v8.h"

namespace blink {

PushMessageData* PushMessageData::Create(const String& message_string) {
  // The standard supports both an empty but valid message and a null message.
  // In case the message is explicitly null, return a null pointer which will
  // be set in the PushEvent.
  if (message_string.IsNull())
    return nullptr;
  return PushMessageData::Create(
      ArrayBufferOrArrayBufferViewOrUSVString::fromUSVString(message_string));
}

PushMessageData* PushMessageData::Create(
    const ArrayBufferOrArrayBufferViewOrUSVString& message_data) {
  if (message_data.isArrayBuffer() || message_data.isArrayBufferView()) {
    DOMArrayBuffer* buffer =
        message_data.isArrayBufferView()
            ? message_data.getAsArrayBufferView().View()->buffer()
            : message_data.getAsArrayBuffer();

    return new PushMessageData(static_cast<const char*>(buffer->Data()),
                               buffer->ByteLength());
  }

  if (message_data.isUSVString()) {
    CString encoded_string = UTF8Encoding().Encode(
        message_data.getAsUSVString(), WTF::kEntitiesForUnencodables);
    return new PushMessageData(encoded_string.data(), encoded_string.length());
  }

  DCHECK(message_data.isNull());
  return nullptr;
}

PushMessageData::PushMessageData(const char* data, unsigned bytes_size) {
  data_.Append(data, bytes_size);
}

PushMessageData::~PushMessageData() {}

DOMArrayBuffer* PushMessageData::arrayBuffer() const {
  return DOMArrayBuffer::Create(data_.data(), data_.size());
}

Blob* PushMessageData::blob() const {
  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->AppendBytes(data_.data(), data_.size());

  // Note that the content type of the Blob object is deliberately not being
  // provided, following the specification.

  const long long byte_length = blob_data->length();
  return Blob::Create(
      BlobDataHandle::Create(std::move(blob_data), byte_length));
}

ScriptValue PushMessageData::json(ScriptState* script_state,
                                  ExceptionState& exception_state) const {
  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> parsed =
      FromJSONString(script_state->GetIsolate(), text(), exception_state);
  if (exception_state.HadException())
    return ScriptValue();

  return ScriptValue(script_state, parsed);
}

String PushMessageData::text() const {
  return UTF8Encoding().Decode(data_.data(), data_.size());
}

DEFINE_TRACE(PushMessageData) {}

}  // namespace blink

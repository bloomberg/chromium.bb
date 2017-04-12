// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMArrayBuffer.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

DOMArrayBuffer* DOMArrayBuffer::CreateUninitializedOrNull(
    unsigned num_elements,
    unsigned element_byte_size) {
  RefPtr<ArrayBuffer> buffer = WTF::ArrayBuffer::CreateUninitializedOrNull(
      num_elements, element_byte_size);
  if (!buffer)
    return nullptr;
  return Create(std::move(buffer));
}

v8::Local<v8::Object> DOMArrayBuffer::Wrap(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();
  v8::Local<v8::Object> wrapper =
      v8::ArrayBuffer::New(isolate, Data(), ByteLength());

  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
}

}  // namespace blink

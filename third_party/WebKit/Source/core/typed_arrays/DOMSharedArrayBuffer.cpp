// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/typed_arrays/DOMSharedArrayBuffer.h"

#include "platform/bindings/DOMDataStore.h"

namespace blink {

v8::Local<v8::Object> DOMSharedArrayBuffer::Wrap(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();
  v8::Local<v8::Object> wrapper =
      v8::SharedArrayBuffer::New(isolate, Buffer()->DataShared(), ByteLength());

  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
}

}  // namespace blink

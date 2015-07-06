// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMSharedArrayBuffer.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8DOMWrapper.h"

namespace blink {

v8::Local<v8::Object> DOMSharedArrayBuffer::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    // It's possible that no one except for the new wrapper owns this object at
    // this moment, so we have to prevent GC to collect this object until the
    // object gets associated with the wrapper.
    RefPtr<DOMSharedArrayBuffer> protect(this);

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    v8::Local<v8::Object> wrapper = v8::SharedArrayBuffer::New(isolate, data(), byteLength());
    // V8::SharedArrayBuffer::New may run an arbitrary script and it may result in
    // creating a new wrapper and associating it with |this|.  If so, the
    // wrapper already created and associated must be used.
    v8::Local<v8::Object> associatedWrapper = DOMDataStore::getWrapper(this, isolate);
    if (UNLIKELY(!associatedWrapper.IsEmpty()))
        return associatedWrapper;

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

v8::Local<v8::Object> DOMSharedArrayBuffer::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo, v8::Local<v8::Object> wrapper)
{
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

} // namespace blink

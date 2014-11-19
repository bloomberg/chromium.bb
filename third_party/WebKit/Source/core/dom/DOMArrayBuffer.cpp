// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMArrayBuffer.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "core/dom/DOMArrayBufferDeallocationObserver.h"

namespace blink {

v8::Handle<v8::Object> DOMArrayBuffer::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // It's possible that no one except for the new wrapper owns this object at
    // this moment, so we have to prevent GC to collect this object until the
    // object gets associated with the wrapper.
    RefPtr<DOMArrayBuffer> protect(this);

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    v8::Handle<v8::Object> wrapper = v8::ArrayBuffer::New(isolate, data(), byteLength());

    // Only when we create a new wrapper, let V8 know that we allocated and
    // associated a new memory block with the wrapper. Note that
    // setDeallocationObserver implicitly calls
    // DOMArrayBufferDeallocationObserver::blinkAllocatedMemory.
    buffer()->setDeallocationObserver(DOMArrayBufferDeallocationObserver::instance());

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

v8::Handle<v8::Object> DOMArrayBuffer::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper)
{
    // This function does not set a deallocation observer to the underlying
    // array buffer.  It's a caller's duty.

    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

} // namespace blink

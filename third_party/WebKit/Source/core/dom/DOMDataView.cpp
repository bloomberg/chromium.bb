// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMDataView.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8DOMWrapper.h"

namespace blink {

PassRefPtr<DOMDataView> DOMDataView::create(PassRefPtr<DOMArrayBuffer> prpBuffer, unsigned byteOffset, unsigned byteLength)
{
    RefPtr<DOMArrayBuffer> buffer = prpBuffer;
    RefPtr<DataView> dataView = DataView::create(buffer->buffer(), byteOffset, byteLength);
    return adoptRef(new DOMDataView(dataView.release(), buffer.release()));
}

v8::Handle<v8::Object> DOMDataView::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // It's possible that no one except for the new wrapper owns this object at
    // this moment, so we have to prevent GC to collect this object until the
    // object gets associated with the wrapper.
    RefPtr<DOMDataView> protect(this);

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    v8::Local<v8::Value> v8Buffer = toV8(buffer(), creationContext, isolate);
    ASSERT(v8Buffer->IsArrayBuffer());

    v8::Handle<v8::Object> wrapper = v8::DataView::New(v8Buffer.As<v8::ArrayBuffer>(), byteOffset(), byteLength());

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

v8::Handle<v8::Object> DOMDataView::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper)
{
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

} // namespace blink

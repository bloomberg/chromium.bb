// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMTypedArray.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8DOMWrapper.h"

namespace blink {

template<typename WTFTypedArray, typename V8TypedArray>
v8::Handle<v8::Object> DOMTypedArray<WTFTypedArray, V8TypedArray>::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // It's possible that no one except for the new wrapper owns this object at
    // this moment, so we have to prevent GC to collect this object until the
    // object gets associated with the wrapper.
    RefPtr<ThisType> protect(this);

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();
    RefPtr<DOMArrayBuffer> buffer = this->buffer();
    v8::Local<v8::Value> v8Buffer = toV8(buffer.get(), creationContext, isolate);
    ASSERT(v8Buffer->IsArrayBuffer());

    v8::Handle<v8::Object> wrapper = V8TypedArray::New(v8Buffer.As<v8::ArrayBuffer>(), byteOffset(), length());

    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

template<typename WTFTypedArray, typename V8TypedArray>
v8::Handle<v8::Object> DOMTypedArray<WTFTypedArray, V8TypedArray>::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo, v8::Handle<v8::Object> wrapper)
{
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

// Instantiation of the non-inline functions of the template classes.
#define INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Type) \
    INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS_IMPL(WTF::Type##Array, v8::Type##Array)
#define INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS_IMPL(WTFTypedArray, V8TypedArray) \
template v8::Handle<v8::Object> DOMTypedArray<WTFTypedArray, V8TypedArray>::wrap(v8::Handle<v8::Object> creationContext, v8::Isolate*); \
template v8::Handle<v8::Object> DOMTypedArray<WTFTypedArray, V8TypedArray>::associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Handle<v8::Object> wrapper)

INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Int8);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Int16);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Int32);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Uint8);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Uint8Clamped);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Uint16);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Uint32);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Float32);
INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS(Float64);

#undef INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS
#undef INSTANTIATE_DOMTYPEDARRAY_MEMBER_FUNCTIONS_IMPL

} // namespace blink

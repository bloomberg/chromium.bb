// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMTypedArray_h
#define DOMTypedArray_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBufferView.h"
#include "wtf/Float32Array.h"
#include "wtf/Float64Array.h"
#include "wtf/Int16Array.h"
#include "wtf/Int32Array.h"
#include "wtf/Int8Array.h"
#include "wtf/Uint16Array.h"
#include "wtf/Uint32Array.h"
#include "wtf/Uint8Array.h"
#include "wtf/Uint8ClampedArray.h"
#include <v8.h>

namespace blink {

template<typename WTFTypedArray, typename V8TypedArray>
class DOMTypedArray final : public DOMArrayBufferView {
    DEFINE_WRAPPERTYPEINFO();
    typedef DOMTypedArray<WTFTypedArray, V8TypedArray> ThisType;
public:
    typedef typename WTFTypedArray::ValueType ValueType;

    static PassRefPtr<ThisType> create(PassRefPtr<WTFTypedArray> bufferView)
    {
        return adoptRef(new ThisType(bufferView));
    }
    static PassRefPtr<ThisType> create(unsigned length)
    {
        return create(WTFTypedArray::create(length));
    }
    static PassRefPtr<ThisType> create(const ValueType* array, unsigned length)
    {
        return create(WTFTypedArray::create(array, length));
    }
    static PassRefPtr<ThisType> create(PassRefPtr<WTF::ArrayBuffer> buffer, unsigned byteOffset, unsigned length)
    {
        return create(WTFTypedArray::create(buffer, byteOffset, length));
    }
    static PassRefPtr<ThisType> create(PassRefPtr<DOMArrayBuffer> prpBuffer, unsigned byteOffset, unsigned length)
    {
        RefPtr<DOMArrayBuffer> buffer = prpBuffer;
        RefPtr<WTFTypedArray> bufferView = WTFTypedArray::create(buffer->buffer(), byteOffset, length);
        return adoptRef(new ThisType(bufferView.release(), buffer.release()));
    }

    static PassRefPtr<ThisType> createOrNull(unsigned length)
    {
        RefPtr<WTFTypedArray> bufferView = WTFTypedArray::createOrNull(length);
        if (!bufferView)
            return nullptr;
        return create(bufferView.release());
    }

    const WTFTypedArray* view() const { return static_cast<const WTFTypedArray*>(DOMArrayBufferView::view()); }
    WTFTypedArray* view() { return static_cast<WTFTypedArray*>(DOMArrayBufferView::view()); }

    ValueType* data() const { return view()->data(); }
    unsigned length() const { return view()->length(); }
    bool setRange(const ValueType* data, size_t dataLength, unsigned offset)
    {
        return view()->setRange(data, dataLength, offset);
    }
    bool zeroRange(unsigned offset, size_t length)
    {
        return view()->zeroRange(offset, length);
    }
    // Invoked by the indexed getter. Does not perform range checks; caller
    // is responsible for doing so and returning undefined as necessary.
    ValueType item(unsigned index) const { return view()->item(index); }

    virtual v8::Handle<v8::Object> wrap(v8::Handle<v8::Object> creationContext, v8::Isolate*) override;
    virtual v8::Handle<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Handle<v8::Object> wrapper) override;

private:
    explicit DOMTypedArray(PassRefPtr<WTFTypedArray> bufferView)
        : DOMArrayBufferView(bufferView) { }
    DOMTypedArray(PassRefPtr<WTFTypedArray> bufferView, PassRefPtr<DOMArrayBuffer> domArrayBuffer)
        : DOMArrayBufferView(bufferView, domArrayBuffer) { }
};

typedef DOMTypedArray<WTF::Int8Array, v8::Int8Array> DOMInt8Array;
typedef DOMTypedArray<WTF::Int16Array, v8::Int16Array> DOMInt16Array;
typedef DOMTypedArray<WTF::Int32Array, v8::Int32Array> DOMInt32Array;
typedef DOMTypedArray<WTF::Uint8Array, v8::Uint8Array> DOMUint8Array;
typedef DOMTypedArray<WTF::Uint8ClampedArray, v8::Uint8ClampedArray> DOMUint8ClampedArray;
typedef DOMTypedArray<WTF::Uint16Array, v8::Uint16Array> DOMUint16Array;
typedef DOMTypedArray<WTF::Uint32Array, v8::Uint32Array> DOMUint32Array;
typedef DOMTypedArray<WTF::Float32Array, v8::Float32Array> DOMFloat32Array;
typedef DOMTypedArray<WTF::Float64Array, v8::Float64Array> DOMFloat64Array;

} // namespace blink

#endif // DOMTypedArray_h

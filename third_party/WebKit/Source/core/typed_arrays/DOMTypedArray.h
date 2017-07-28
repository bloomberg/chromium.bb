// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMTypedArray_h
#define DOMTypedArray_h

#include "core/CoreExport.h"
#include "core/typed_arrays/DOMArrayBufferView.h"
#include "core/typed_arrays/DOMSharedArrayBuffer.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/typed_arrays/Float32Array.h"
#include "platform/wtf/typed_arrays/Float64Array.h"
#include "platform/wtf/typed_arrays/Int16Array.h"
#include "platform/wtf/typed_arrays/Int32Array.h"
#include "platform/wtf/typed_arrays/Int8Array.h"
#include "platform/wtf/typed_arrays/Uint16Array.h"
#include "platform/wtf/typed_arrays/Uint32Array.h"
#include "platform/wtf/typed_arrays/Uint8Array.h"
#include "platform/wtf/typed_arrays/Uint8ClampedArray.h"
#include "v8/include/v8.h"

namespace blink {

template <typename WTFTypedArray, typename V8TypedArray>
class CORE_TEMPLATE_CLASS_EXPORT DOMTypedArray final
    : public DOMArrayBufferView {
  typedef DOMTypedArray<WTFTypedArray, V8TypedArray> ThisType;
  DECLARE_WRAPPERTYPEINFO();

 public:
  typedef typename WTFTypedArray::ValueType ValueType;

  static ThisType* Create(PassRefPtr<WTFTypedArray> buffer_view) {
    return new ThisType(std::move(buffer_view));
  }
  static ThisType* Create(unsigned length) {
    return Create(WTFTypedArray::Create(length));
  }
  static ThisType* Create(const ValueType* array, unsigned length) {
    return Create(WTFTypedArray::Create(array, length));
  }
  static ThisType* Create(PassRefPtr<WTF::ArrayBuffer> buffer,
                          unsigned byte_offset,
                          unsigned length) {
    return Create(
        WTFTypedArray::Create(std::move(buffer), byte_offset, length));
  }
  static ThisType* Create(DOMArrayBufferBase* buffer,
                          unsigned byte_offset,
                          unsigned length) {
    RefPtr<WTFTypedArray> buffer_view =
        WTFTypedArray::Create(buffer->Buffer(), byte_offset, length);
    return new ThisType(std::move(buffer_view), buffer);
  }

  static ThisType* CreateOrNull(unsigned length) {
    RefPtr<WTF::ArrayBuffer> buffer =
        WTF::ArrayBuffer::CreateOrNull(length, sizeof(ValueType));
    return buffer ? Create(std::move(buffer), 0, length) : nullptr;
  }

  const WTFTypedArray* View() const {
    return static_cast<const WTFTypedArray*>(DOMArrayBufferView::View());
  }
  WTFTypedArray* View() {
    return static_cast<WTFTypedArray*>(DOMArrayBufferView::View());
  }

  ValueType* Data() const { return View()->Data(); }
  ValueType* DataMaybeShared() const { return View()->DataMaybeShared(); }
  unsigned length() const { return View()->length(); }
  // Invoked by the indexed getter. Does not perform range checks; caller
  // is responsible for doing so and returning undefined as necessary.
  ValueType Item(unsigned index) const { return View()->Item(index); }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;

 private:
  explicit DOMTypedArray(PassRefPtr<WTFTypedArray> buffer_view)
      : DOMArrayBufferView(std::move(buffer_view)) {}
  DOMTypedArray(PassRefPtr<WTFTypedArray> buffer_view,
                DOMArrayBufferBase* dom_array_buffer)
      : DOMArrayBufferView(std::move(buffer_view), dom_array_buffer) {}
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Int8Array, v8::Int8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Int16Array, v8::Int16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Int32Array, v8::Int32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Uint8Array, v8::Uint8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Uint8ClampedArray, v8::Uint8ClampedArray>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Uint16Array, v8::Uint16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Uint32Array, v8::Uint32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Float32Array, v8::Float32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<WTF::Float64Array, v8::Float64Array>;

typedef DOMTypedArray<WTF::Int8Array, v8::Int8Array> DOMInt8Array;
typedef DOMTypedArray<WTF::Int16Array, v8::Int16Array> DOMInt16Array;
typedef DOMTypedArray<WTF::Int32Array, v8::Int32Array> DOMInt32Array;
typedef DOMTypedArray<WTF::Uint8Array, v8::Uint8Array> DOMUint8Array;
typedef DOMTypedArray<WTF::Uint8ClampedArray, v8::Uint8ClampedArray>
    DOMUint8ClampedArray;
typedef DOMTypedArray<WTF::Uint16Array, v8::Uint16Array> DOMUint16Array;
typedef DOMTypedArray<WTF::Uint32Array, v8::Uint32Array> DOMUint32Array;
typedef DOMTypedArray<WTF::Float32Array, v8::Float32Array> DOMFloat32Array;
typedef DOMTypedArray<WTF::Float64Array, v8::Float64Array> DOMFloat64Array;

}  // namespace blink

#endif  // DOMTypedArray_h

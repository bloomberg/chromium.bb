// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/typed_arrays/DOMDataView.h"

#include "bindings/core/v8/V8ArrayBuffer.h"
#include "platform/bindings/DOMDataStore.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/typed_arrays/ArrayBufferView.h"

namespace blink {

namespace {

class DataView final : public ArrayBufferView {
 public:
  static scoped_refptr<DataView> Create(ArrayBuffer* buffer,
                                        unsigned byte_offset,
                                        unsigned byte_length) {
    CheckedNumeric<uint32_t> checked_max = byte_offset;
    checked_max += byte_length;
    CHECK_LE(checked_max.ValueOrDie(), buffer->ByteLength());
    return base::AdoptRef(new DataView(buffer, byte_offset, byte_length));
  }

  unsigned ByteLength() const override { return byte_length_; }
  ViewType GetType() const override { return kTypeDataView; }
  unsigned TypeSize() const override { return 1; }

 protected:
  void Neuter() override {
    ArrayBufferView::Neuter();
    byte_length_ = 0;
  }

 private:
  DataView(ArrayBuffer* buffer, unsigned byte_offset, unsigned byte_length)
      : ArrayBufferView(buffer, byte_offset), byte_length_(byte_length) {}

  unsigned byte_length_;
};

}  // anonymous namespace

DOMDataView* DOMDataView::Create(DOMArrayBufferBase* buffer,
                                 unsigned byte_offset,
                                 unsigned byte_length) {
  scoped_refptr<DataView> data_view =
      DataView::Create(buffer->Buffer(), byte_offset, byte_length);
  return new DOMDataView(data_view, buffer);
}

v8::Local<v8::Object> DOMDataView::Wrap(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();
  v8::Local<v8::Value> v8_buffer = ToV8(buffer(), creation_context, isolate);
  if (v8_buffer.IsEmpty())
    return v8::Local<v8::Object>();
  DCHECK(v8_buffer->IsArrayBuffer());

  v8::Local<v8::Object> wrapper = v8::DataView::New(
      v8_buffer.As<v8::ArrayBuffer>(), byteOffset(), byteLength());

  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
}

}  // namespace blink

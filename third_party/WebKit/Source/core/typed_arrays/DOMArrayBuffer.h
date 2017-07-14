// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBuffer_h
#define DOMArrayBuffer_h

#include "core/CoreExport.h"
#include "core/typed_arrays/DOMArrayBufferBase.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"

namespace blink {

class SharedBuffer;

class CORE_EXPORT DOMArrayBuffer final : public DOMArrayBufferBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMArrayBuffer* Create(PassRefPtr<WTF::ArrayBuffer> buffer) {
    return new DOMArrayBuffer(std::move(buffer));
  }
  static DOMArrayBuffer* Create(unsigned num_elements,
                                unsigned element_byte_size) {
    return Create(WTF::ArrayBuffer::Create(num_elements, element_byte_size));
  }
  static DOMArrayBuffer* Create(const void* source, unsigned byte_length) {
    return Create(WTF::ArrayBuffer::Create(source, byte_length));
  }
  static DOMArrayBuffer* Create(WTF::ArrayBufferContents& contents) {
    return Create(WTF::ArrayBuffer::Create(contents));
  }
  static DOMArrayBuffer* Create(PassRefPtr<SharedBuffer>);

  // Only for use by XMLHttpRequest::responseArrayBuffer and
  // Internals::serializeObject.
  static DOMArrayBuffer* CreateUninitializedOrNull(unsigned num_elements,
                                                   unsigned element_byte_size);

  DOMArrayBuffer* Slice(int begin, int end) const {
    return Create(Buffer()->Slice(begin, end));
  }
  DOMArrayBuffer* Slice(int begin) const {
    return Create(Buffer()->Slice(begin));
  }

  bool IsNeuterable(v8::Isolate*);

  // Transfer the ArrayBuffer if it is neuterable, otherwise make a copy and
  // transfer that.
  bool Transfer(v8::Isolate*, WTF::ArrayBufferContents& result);

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;

 private:
  explicit DOMArrayBuffer(PassRefPtr<WTF::ArrayBuffer> buffer)
      : DOMArrayBufferBase(std::move(buffer)) {}
};

}  // namespace blink

#endif  // DOMArrayBuffer_h

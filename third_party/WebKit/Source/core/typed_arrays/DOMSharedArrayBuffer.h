// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMSharedArrayBuffer_h
#define DOMSharedArrayBuffer_h

#include "core/CoreExport.h"
#include "core/typed_arrays/DOMArrayBufferBase.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"

namespace blink {

class CORE_EXPORT DOMSharedArrayBuffer final : public DOMArrayBufferBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMSharedArrayBuffer* Create(PassRefPtr<WTF::ArrayBuffer> buffer) {
    DCHECK(buffer->IsShared());
    return new DOMSharedArrayBuffer(std::move(buffer));
  }
  static DOMSharedArrayBuffer* Create(unsigned num_elements,
                                      unsigned element_byte_size) {
    return Create(
        WTF::ArrayBuffer::CreateShared(num_elements, element_byte_size));
  }
  static DOMSharedArrayBuffer* Create(const void* source,
                                      unsigned byte_length) {
    return Create(WTF::ArrayBuffer::CreateShared(source, byte_length));
  }
  static DOMSharedArrayBuffer* Create(WTF::ArrayBufferContents& contents) {
    DCHECK(contents.IsShared());
    return Create(WTF::ArrayBuffer::Create(contents));
  }

  bool ShareContentsWith(WTF::ArrayBufferContents& result) {
    return Buffer()->ShareContentsWith(result);
  }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;

 private:
  explicit DOMSharedArrayBuffer(PassRefPtr<WTF::ArrayBuffer> buffer)
      : DOMArrayBufferBase(std::move(buffer)) {}
};

}  // namespace blink

#endif  // DOMSharedArrayBuffer_h

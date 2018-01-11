// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBufferBase_h
#define DOMArrayBufferBase_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"

namespace blink {

class CORE_EXPORT DOMArrayBufferBase : public ScriptWrappable {
 public:
  virtual ~DOMArrayBufferBase() = default;

  const WTF::ArrayBuffer* Buffer() const { return buffer_.get(); }
  WTF::ArrayBuffer* Buffer() { return buffer_.get(); }

  const void* Data() const { return Buffer()->Data(); }
  void* Data() { return Buffer()->Data(); }
  unsigned ByteLength() const { return Buffer()->ByteLength(); }
  bool IsNeutered() const { return Buffer()->IsNeutered(); }
  bool IsShared() const { return Buffer()->IsShared(); }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

 protected:
  explicit DOMArrayBufferBase(scoped_refptr<WTF::ArrayBuffer> buffer)
      : buffer_(std::move(buffer)) {
    DCHECK(buffer_);
  }

  scoped_refptr<WTF::ArrayBuffer> buffer_;
};

}  // namespace blink

#endif  // DOMArrayBufferBase_h

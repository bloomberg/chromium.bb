// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMDataView_h
#define DOMDataView_h

#include "core/CoreExport.h"
#include "core/typed_arrays/DOMArrayBufferView.h"

namespace blink {

class CORE_EXPORT DOMDataView final : public DOMArrayBufferView {
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef char ValueType;

  static DOMDataView* Create(DOMArrayBufferBase*,
                             unsigned byte_offset,
                             unsigned byte_length);

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;

 private:
  DOMDataView(scoped_refptr<WTF::ArrayBufferView> data_view,
              DOMArrayBufferBase* dom_array_buffer)
      : DOMArrayBufferView(std::move(data_view), dom_array_buffer) {}
};

}  // namespace blink

#endif  // DOMDataView_h

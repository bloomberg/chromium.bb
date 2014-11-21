// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMDataView_h
#define DOMDataView_h

#include "core/dom/DOMArrayBufferView.h"
#include "core/html/canvas/DataView.h"

namespace blink {

class DOMDataView final : public DOMArrayBufferView {
    DEFINE_WRAPPERTYPEINFO();
public:
    typedef char ValueType;

    static PassRefPtr<DOMDataView> create(PassRefPtr<DOMArrayBuffer>, unsigned byteOffset, unsigned byteLength);

    const DataView* view() const { return static_cast<const DataView*>(DOMArrayBufferView::view()); }
    DataView* view() { return static_cast<DataView*>(DOMArrayBufferView::view()); }

    virtual v8::Handle<v8::Object> wrap(v8::Handle<v8::Object> creationContext, v8::Isolate*) override;
    virtual v8::Handle<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Handle<v8::Object> wrapper) override;

private:
    DOMDataView(PassRefPtr<DataView> dataView, PassRefPtr<DOMArrayBuffer> domArrayBuffer)
        : DOMArrayBufferView(dataView, domArrayBuffer) { }
};

} // namespace blink

#endif // DOMDataView_h

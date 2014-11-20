// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBufferView_h
#define DOMArrayBufferView_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/RefCounted.h"

namespace blink {

class DOMArrayBufferView : public RefCounted<DOMArrayBufferView>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    typedef WTF::ArrayBufferView::ViewType ViewType;
    static const ViewType TypeInt8 = WTF::ArrayBufferView::TypeInt8;
    static const ViewType TypeUint8 = WTF::ArrayBufferView::TypeUint8;
    static const ViewType TypeUint8Clamped = WTF::ArrayBufferView::TypeUint8Clamped;
    static const ViewType TypeInt16 = WTF::ArrayBufferView::TypeInt16;
    static const ViewType TypeUint16 = WTF::ArrayBufferView::TypeUint16;
    static const ViewType TypeInt32 = WTF::ArrayBufferView::TypeInt32;
    static const ViewType TypeUint32 = WTF::ArrayBufferView::TypeUint32;
    static const ViewType TypeFloat32 = WTF::ArrayBufferView::TypeFloat32;
    static const ViewType TypeFloat64 = WTF::ArrayBufferView::TypeFloat64;
    static const ViewType TypeDataView = WTF::ArrayBufferView::TypeDataView;

    virtual ~DOMArrayBufferView() { }

    PassRefPtr<DOMArrayBuffer> buffer() const
    {
        if (!m_domArrayBuffer)
            m_domArrayBuffer = DOMArrayBuffer::create(view()->buffer());
        return m_domArrayBuffer;
    }

    const WTF::ArrayBufferView* view() const { return m_bufferView.get(); }
    WTF::ArrayBufferView* view() { return m_bufferView.get(); }

    ViewType type() const { return view()->type(); }
    const char* typeName() { return view()->typeName(); }
    void* baseAddress() const { return view()->baseAddress(); }
    unsigned byteOffset() const { return view()->byteOffset(); }
    unsigned byteLength() const { return view()->byteLength(); }
    void setNeuterable(bool flag) { return view()->setNeuterable(flag); }

    virtual v8::Handle<v8::Object> wrap(v8::Handle<v8::Object> creationContext, v8::Isolate*) override
    {
        ASSERT_NOT_REACHED();
        return v8::Handle<v8::Object>();
    }
    virtual v8::Handle<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Handle<v8::Object> wrapper) override
    {
        ASSERT_NOT_REACHED();
        return v8::Handle<v8::Object>();
    }

protected:
    explicit DOMArrayBufferView(PassRefPtr<WTF::ArrayBufferView> bufferView)
        : m_bufferView(bufferView)
    {
        ASSERT(m_bufferView);
    }
    DOMArrayBufferView(PassRefPtr<WTF::ArrayBufferView> bufferView, PassRefPtr<DOMArrayBuffer> domArrayBuffer)
        : m_bufferView(bufferView), m_domArrayBuffer(domArrayBuffer)
    {
        ASSERT(m_bufferView);
        ASSERT(m_domArrayBuffer);
        ASSERT(m_domArrayBuffer->buffer() == m_bufferView->buffer());
    }

private:
    RefPtr<WTF::ArrayBufferView> m_bufferView;
    mutable RefPtr<DOMArrayBuffer> m_domArrayBuffer;
};

} // namespace blink

#endif // DOMArrayBufferView_h

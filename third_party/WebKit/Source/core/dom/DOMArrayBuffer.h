// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBuffer_h
#define DOMArrayBuffer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/RefCounted.h"

namespace blink {

class DOMArrayBuffer final : public RefCounted<DOMArrayBuffer>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtr<DOMArrayBuffer> create(PassRefPtr<WTF::ArrayBuffer> buffer)
    {
        if (!buffer)
            return nullptr;
        return adoptRef(new DOMArrayBuffer(buffer));
    }
    static PassRefPtr<DOMArrayBuffer> create(unsigned numElements, unsigned elementByteSize)
    {
        return adoptRef(new DOMArrayBuffer(WTF::ArrayBuffer::create(numElements, elementByteSize)));
    }
    static PassRefPtr<DOMArrayBuffer> create(const void* source, unsigned byteLength)
    {
        return adoptRef(new DOMArrayBuffer(WTF::ArrayBuffer::create(source, byteLength)));
    }
    static PassRefPtr<DOMArrayBuffer> create(WTF::ArrayBufferContents& contents)
    {
        return adoptRef(new DOMArrayBuffer(WTF::ArrayBuffer::create(contents)));
    }

    const WTF::ArrayBuffer* buffer() const { return m_buffer.get(); }
    WTF::ArrayBuffer* buffer() { return m_buffer.get(); }

    const void* data() const { return buffer()->data(); }
    unsigned long byteLength() const { return buffer()->byteLength(); }
    void* data() { return buffer()->data(); }
    bool transfer(WTF::ArrayBufferContents& result) { return buffer()->transfer(result); }
    bool isNeutered() { return buffer()->isNeutered(); }

    virtual v8::Handle<v8::Object> wrap(v8::Handle<v8::Object> creationContext, v8::Isolate*) override;
    virtual v8::Handle<v8::Object> associateWithWrapper(const WrapperTypeInfo*, v8::Handle<v8::Object> wrapper, v8::Isolate*) override;

private:
    explicit DOMArrayBuffer(PassRefPtr<WTF::ArrayBuffer> buffer)
        : m_buffer(buffer)
    {
        ASSERT(m_buffer);
    }

    RefPtr<WTF::ArrayBuffer> m_buffer;
};

} // namespace blink

#endif // DOMArrayBuffer_h

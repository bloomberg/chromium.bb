// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransferableArrayBuffer_h
#define TransferableArrayBuffer_h

#include "bindings/core/v8/Transferable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMArrayBufferBase.h"

namespace blink {

class CORE_EXPORT TransferableArrayBuffer : public Transferable {
public:
    ~TransferableArrayBuffer() override { }
    void append(DOMArrayBufferBase* arrayBuffer)
    {
        m_arrayBufferArray.append(arrayBuffer);
    }
    bool contains(DOMArrayBufferBase* arrayBuffer)
    {
        return m_arrayBufferArray.contains(arrayBuffer);
    }
    static TransferableArrayBuffer* ensure(TransferableArray& transferables)
    {
        if (transferables.size() <= TransferableArrayBufferType) {
            transferables.resize(TransferableArrayBufferType + 1);
            transferables[TransferableArrayBufferType] = new TransferableArrayBuffer();
        }
        return static_cast<TransferableArrayBuffer*>(transferables[TransferableArrayBufferType].get());
    }
    static TransferableArrayBuffer* get(TransferableArray& transferables)
    {
        if (transferables.size() <= TransferableArrayBufferType)
            return nullptr;
        return static_cast<TransferableArrayBuffer*>(transferables[TransferableArrayBufferType].get());
    }
    HeapVector<Member<DOMArrayBufferBase>, 1>& getArray()
    {
        return m_arrayBufferArray;
    }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_arrayBufferArray);
        Transferable::trace(visitor);
    }
private:
    HeapVector<Member<DOMArrayBufferBase>, 1> m_arrayBufferArray;
};

} // namespace blink

#endif // TransferableArrayBuffer_h

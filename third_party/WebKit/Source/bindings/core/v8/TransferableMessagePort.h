// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransferableMessagePort_h
#define TransferableMessagePort_h

#include "bindings/core/v8/Transferable.h"
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"

namespace blink {

class CORE_EXPORT TransferableMessagePort : public Transferable {
public:
    ~TransferableMessagePort() override { }
    void append(MessagePort* messagePort)
    {
        m_messagePortArray.append(messagePort);
    }
    bool contains(MessagePort* messagePort)
    {
        return m_messagePortArray.contains(messagePort);
    }
    static TransferableMessagePort* ensure(TransferableArray& transferables)
    {
        if (transferables.size() <= TransferableMessagePortType) {
            transferables.resize(TransferableMessagePortType + 1);
            transferables[TransferableMessagePortType] = new TransferableMessagePort();
        }
        return static_cast<TransferableMessagePort*>(transferables[TransferableMessagePortType].get());
    }
    static TransferableMessagePort* get(const TransferableArray& transferables)
    {
        if (transferables.size() <= TransferableMessagePortType)
            return nullptr;
        return static_cast<TransferableMessagePort*>(transferables[TransferableMessagePortType].get());
    }
    HeapVector<Member<MessagePort>, 1>& getArray()
    {
        return m_messagePortArray;
    }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_messagePortArray);
        Transferable::trace(visitor);
    }
private:
    HeapVector<Member<MessagePort>, 1> m_messagePortArray;
};

} // namespace blink

#endif // TransferableMessagePort_h

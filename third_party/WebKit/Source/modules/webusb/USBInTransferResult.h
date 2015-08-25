// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBInTransferResult_h
#define USBInTransferResult_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayBuffer.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBTransferInfo.h"
#include "wtf/text/WTFString.h"

namespace blink {

class USBInTransferResult final
    : public GarbageCollectedFinalized<USBInTransferResult>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBInTransferResult* create(const String& status, const WebVector<uint8_t> data)
    {
        return new USBInTransferResult(status, data);
    }

    USBInTransferResult(const String& status, const WebVector<uint8_t> data)
        : m_status(status)
        , m_data(DOMArrayBuffer::create(data.data(), data.size()))
    {
    }

    virtual ~USBInTransferResult() { }

    String status() const { return m_status; }
    PassRefPtr<DOMArrayBuffer> data() const { return m_data; }

    DEFINE_INLINE_TRACE() { }

private:
    const String m_status;
    const RefPtr<DOMArrayBuffer> m_data;
};

} // namespace blink

#endif // USBInTransferResult_h

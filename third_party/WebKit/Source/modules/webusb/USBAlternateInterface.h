// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBAlternateInterface_h
#define USBAlternateInterface_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/webusb/WebUSBDeviceInfo.h"

namespace blink {

class ExceptionState;
class USBEndpoint;
class USBInterface;

class USBAlternateInterface
    : public GarbageCollected<USBAlternateInterface>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBAlternateInterface* create(const USBInterface*, size_t alternateIndex);
    static USBAlternateInterface* create(const USBInterface*, size_t alternateSetting, ExceptionState&);

    USBAlternateInterface(const USBInterface*, size_t alternateIndex);

    const WebUSBDeviceInfo::AlternateInterface& info() const;

    uint8_t alternateSetting() const;
    uint8_t interfaceClass() const;
    uint8_t interfaceSubclass() const;
    uint8_t interfaceProtocol() const;
    String interfaceName() const;
    HeapVector<Member<USBEndpoint>> endpoints() const;

    DECLARE_TRACE();

private:
    Member<const USBInterface> m_interface;
    const size_t m_alternateIndex;
};

} // namespace blink

#endif // USBAlternateInterface_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBDevice_h
#define USBDevice_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBDevice.h"
#include "public/platform/modules/webusb/WebUSBDeviceInfo.h"

namespace blink {

class ScriptPromiseResolver;
class USBConfiguration;

class USBDevice
    : public GarbageCollectedFinalized<USBDevice>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    using WebType = OwnPtr<WebUSBDevice>;

    static USBDevice* create(PassOwnPtr<WebUSBDevice> device)
    {
        return new USBDevice(device);
    }

    static USBDevice* take(ScriptPromiseResolver*, PassOwnPtr<WebUSBDevice> device)
    {
        return create(device);
    }

    explicit USBDevice(PassOwnPtr<WebUSBDevice> device)
        : m_device(device)
    {
    }

    virtual ~USBDevice() { }

    const WebUSBDeviceInfo& info() const { return m_device->info(); }

    String guid() const { return info().guid; }
    uint8_t usbVersionMajor() { return info().usbVersionMajor; }
    uint8_t usbVersionMinor() { return info().usbVersionMinor; }
    uint8_t usbVersionSubminor() { return info().usbVersionSubminor; }
    uint8_t deviceClass() { return info().deviceClass; }
    uint8_t deviceSubclass() const { return info().deviceSubclass; }
    uint8_t deviceProtocol() const { return info().deviceProtocol; }
    uint16_t vendorId() const { return info().vendorID; }
    uint16_t productId() const { return info().productID; }
    uint8_t deviceVersionMajor() const { return info().deviceVersionMajor; }
    uint8_t deviceVersionMinor() const { return info().deviceVersionMinor; }
    uint8_t deviceVersionSubminor() const { return info().deviceVersionSubminor; }
    String manufacturerName() const { return info().manufacturerName; }
    String productName() const { return info().productName; }
    String serialNumber() const { return info().serialNumber; }
    HeapVector<Member<USBConfiguration>> configurations() const;

    DEFINE_INLINE_TRACE() { }

private:
    OwnPtr<WebUSBDevice> m_device;
};

} // namespace blink

#endif // USBDevice_h

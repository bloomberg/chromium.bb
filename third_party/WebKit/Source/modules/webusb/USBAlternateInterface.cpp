// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBAlternateInterface.h"

#include "bindings/core/v8/ExceptionState.h"
#include "modules/webusb/USBEndpoint.h"
#include "modules/webusb/USBInterface.h"

namespace blink {

USBAlternateInterface* USBAlternateInterface::create(const USBInterface* interface, size_t alternateIndex)
{
    return new USBAlternateInterface(interface, alternateIndex);
}

USBAlternateInterface* USBAlternateInterface::create(const USBInterface* interface, size_t alternateSetting, ExceptionState& exceptionState)
{
    for (size_t i = 0; i < interface->info().alternates.size(); ++i) {
        if (interface->info().alternates[i].alternateSetting == alternateSetting)
            return USBAlternateInterface::create(interface, i);
    }
    exceptionState.throwRangeError("Invalid alternate setting.");
    return nullptr;
}

USBAlternateInterface::USBAlternateInterface(const USBInterface* interface, size_t alternateIndex)
    : m_interface(interface)
    , m_alternateIndex(alternateIndex)
{
    ASSERT(m_interface);
    ASSERT(m_alternateIndex < m_interface->info().alternates.size());
}

const WebUSBDeviceInfo::AlternateInterface& USBAlternateInterface::info() const
{
    const WebUSBDeviceInfo::Interface& interfaceInfo = m_interface->info();
    ASSERT(m_alternateIndex < interfaceInfo.alternates.size());
    return interfaceInfo.alternates[m_alternateIndex];
}

uint8_t USBAlternateInterface::alternateSetting() const
{
    return info().alternateSetting;
}

uint8_t USBAlternateInterface::interfaceClass() const
{
    return info().classCode;
}

uint8_t USBAlternateInterface::interfaceSubclass() const
{
    return info().subclassCode;
}

uint8_t USBAlternateInterface::interfaceProtocol() const
{
    return info().protocolCode;
}

String USBAlternateInterface::interfaceName() const
{
    return info().interfaceName;
}

HeapVector<Member<USBEndpoint>> USBAlternateInterface::endpoints() const
{
    HeapVector<Member<USBEndpoint>> endpoints;
    for (size_t i = 0; i < info().endpoints.size(); ++i)
        endpoints.append(USBEndpoint::create(this, i));
    return endpoints;
}

DEFINE_TRACE(USBAlternateInterface)
{
    visitor->trace(m_interface);
}

} // namespace blink

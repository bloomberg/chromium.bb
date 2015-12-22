// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBInterface.h"

#include "bindings/core/v8/ExceptionState.h"
#include "modules/webusb/USBAlternateInterface.h"
#include "modules/webusb/USBConfiguration.h"

namespace blink {

USBInterface* USBInterface::create(const USBConfiguration* configuration, size_t interfaceIndex)
{
    return new USBInterface(configuration, interfaceIndex);
}

USBInterface* USBInterface::create(const USBConfiguration* configuration, size_t interfaceNumber, ExceptionState& exceptionState)
{
    for (size_t i = 0; i < configuration->info().interfaces.size(); ++i) {
        if (configuration->info().interfaces[i].interfaceNumber == interfaceNumber)
            return new USBInterface(configuration, i);
    }
    exceptionState.throwRangeError("Invalid interface index.");
    return nullptr;
}

USBInterface::USBInterface(const USBConfiguration* configuration, size_t interfaceIndex)
    : m_configuration(configuration)
    , m_interfaceIndex(interfaceIndex)
{
    ASSERT(m_configuration);
    ASSERT(m_interfaceIndex < m_configuration->info().interfaces.size());
}

const WebUSBDeviceInfo::Interface& USBInterface::info() const
{
    return m_configuration->info().interfaces[m_interfaceIndex];
}

HeapVector<Member<USBAlternateInterface>> USBInterface::alternates() const
{
    HeapVector<Member<USBAlternateInterface>> alternates;
    for (size_t i = 0; i < info().alternates.size(); ++i)
        alternates.append(USBAlternateInterface::create(this, i));
    return alternates;
}

uint8_t USBInterface::interfaceNumber() const
{
    return info().interfaceNumber;
}

DEFINE_TRACE(USBInterface)
{
    visitor->trace(m_configuration);
}

} // namespace blink

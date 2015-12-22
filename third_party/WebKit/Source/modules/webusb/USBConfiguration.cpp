// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBConfiguration.h"

#include "bindings/core/v8/ExceptionState.h"
#include "modules/webusb/USBDevice.h"
#include "modules/webusb/USBInterface.h"

namespace blink {

USBConfiguration* USBConfiguration::create(const USBDevice* device, size_t configurationIndex)
{
    return new USBConfiguration(device, configurationIndex);
}

USBConfiguration* USBConfiguration::create(const USBDevice* device, size_t configurationValue, ExceptionState& exceptionState)
{
    for (size_t i = 0; i < device->info().configurations.size(); ++i) {
        if (device->info().configurations[i].configurationValue == configurationValue)
            return new USBConfiguration(device, i);
    }
    exceptionState.throwRangeError("Invalid configuration value.");
    return nullptr;
}

USBConfiguration* USBConfiguration::createFromValue(const USBDevice* device, uint8_t configurationValue)
{
    for (size_t i = 0; i < device->info().configurations.size(); ++i) {
        if (device->info().configurations[i].configurationValue == configurationValue)
            return new USBConfiguration(device, i);
    }
    return nullptr;
}

USBConfiguration::USBConfiguration(const USBDevice* device, size_t configurationIndex)
    : m_device(device)
    , m_configurationIndex(configurationIndex)
{
    ASSERT(m_device);
    ASSERT(m_configurationIndex < m_device->info().configurations.size());
}

const WebUSBDeviceInfo::Configuration& USBConfiguration::info() const
{
    return m_device->info().configurations[m_configurationIndex];
}

uint8_t USBConfiguration::configurationValue() const
{
    return info().configurationValue;
}

String USBConfiguration::configurationName() const
{
    return info().configurationName;
}

HeapVector<Member<USBInterface>> USBConfiguration::interfaces() const
{
    HeapVector<Member<USBInterface>> interfaces;
    for (size_t i = 0; i < info().interfaces.size(); ++i)
        interfaces.append(USBInterface::create(this, i));
    return interfaces;
}

DEFINE_TRACE(USBConfiguration)
{
    visitor->trace(m_device);
}

} // namespace blink

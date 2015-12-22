// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBEndpoint.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webusb/USBAlternateInterface.h"
#include "public/platform/modules/webusb/WebUSBDevice.h"

namespace blink {

namespace {

bool convertDirectionFromEnum(const String& direction, WebUSBDevice::TransferDirection* webDirection)
{
    if (direction == "in")
        *webDirection = WebUSBDevice::TransferDirection::In;
    else if (direction == "out")
        *webDirection = WebUSBDevice::TransferDirection::Out;
    else
        return false;
    return true;
}

String convertDirectionToEnum(const WebUSBDevice::TransferDirection& direction)
{
    switch (direction) {
    case WebUSBDevice::TransferDirection::In:
        return "in";
    case WebUSBDevice::TransferDirection::Out:
        return "out";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

String convertTypeToEnum(const WebUSBDeviceInfo::Endpoint::Type& type)
{
    switch (type) {
    case WebUSBDeviceInfo::Endpoint::Type::Bulk:
        return "bulk";
    case WebUSBDeviceInfo::Endpoint::Type::Interrupt:
        return "interrupt";
    case WebUSBDeviceInfo::Endpoint::Type::Isochronous:
        return "isochronous";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

} // namespace

USBEndpoint* USBEndpoint::create(const USBAlternateInterface* alternate, size_t endpointIndex)
{
    return new USBEndpoint(alternate, endpointIndex);
}

USBEndpoint* USBEndpoint::create(const USBAlternateInterface* alternate, size_t endpointNumber, const String& direction, ExceptionState& exceptionState)
{
    WebUSBDevice::TransferDirection webDirection;
    if (!convertDirectionFromEnum(direction, &webDirection)) {
        exceptionState.throwRangeError("Invalid endpoint direction.");
        return nullptr;
    }
    for (size_t i = 0; i < alternate->info().endpoints.size(); ++i) {
        const WebUSBDeviceInfo::Endpoint& endpoint = alternate->info().endpoints[i];
        if (endpoint.endpointNumber == endpointNumber && endpoint.direction == webDirection)
            return USBEndpoint::create(alternate, i);
    }
    exceptionState.throwRangeError("Invalid endpoint number.");
    return nullptr;
}

USBEndpoint::USBEndpoint(const USBAlternateInterface* alternate, size_t endpointIndex)
    : m_alternate(alternate)
    , m_endpointIndex(endpointIndex)
{
    ASSERT(m_alternate);
    ASSERT(m_endpointIndex < m_alternate->info().endpoints.size());
}

const WebUSBDeviceInfo::Endpoint& USBEndpoint::info() const
{
    const WebUSBDeviceInfo::AlternateInterface& alternateInfo = m_alternate->info();
    ASSERT(m_endpointIndex < alternateInfo.endpoints.size());
    return alternateInfo.endpoints[m_endpointIndex];
}

uint8_t USBEndpoint::endpointNumber() const
{
    return info().endpointNumber;
}

String USBEndpoint::direction() const
{
    return convertDirectionToEnum(info().direction);
}

String USBEndpoint::type() const
{
    return convertTypeToEnum(info().type);
}

unsigned USBEndpoint::packetSize() const
{
    return info().packetSize;
}

DEFINE_TRACE(USBEndpoint)
{
    visitor->trace(m_alternate);
}

} // namespace blink

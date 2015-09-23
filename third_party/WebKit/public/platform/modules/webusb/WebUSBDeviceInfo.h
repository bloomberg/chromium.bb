// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBDeviceInfo_h
#define WebUSBDeviceInfo_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/webusb/WebUSBDevice.h"

namespace blink {

struct WebUSBDeviceInfo {
    struct Endpoint {
        enum class Type {
            Bulk,
            Interrupt,
            Isochronous
        };

        Endpoint()
            : endpointNumber(0)
            , direction(WebUSBDevice::TransferDirection::In)
            , type(Type::Bulk)
            , packetSize(0)
        {
        }

        uint8_t endpointNumber;
        WebUSBDevice::TransferDirection direction;
        Type type;
        uint32_t packetSize;
    };

    struct AlternateInterface {
        AlternateInterface()
            : alternateSetting(0)
            , classCode(0)
            , subclassCode(0)
            , protocolCode(0)
        {
        }

        uint8_t alternateSetting;
        uint8_t classCode;
        uint8_t subclassCode;
        uint8_t protocolCode;
        WebString interfaceName;
        WebVector<Endpoint> endpoints;
    };

    struct Interface {
        Interface()
            : interfaceNumber(0)
        {
        }

        uint8_t interfaceNumber;
        WebVector<AlternateInterface> alternates;
    };

    struct Configuration {
        Configuration()
            : configurationValue(0)
        {
        }

        uint8_t configurationValue;
        WebString configurationName;
        WebVector<Interface> interfaces;
    };

    WebUSBDeviceInfo()
        : usbVersionMajor(0)
        , usbVersionMinor(0)
        , usbVersionSubminor(0)
        , deviceClass(0)
        , deviceSubclass(0)
        , deviceProtocol(0)
        , vendorID(0)
        , productID(0)
        , deviceVersionMajor(0)
        , deviceVersionMinor(0)
        , deviceVersionSubminor(0)
    {
    }

    WebString guid;
    uint8_t usbVersionMajor;
    uint8_t usbVersionMinor;
    uint8_t usbVersionSubminor;
    uint8_t deviceClass;
    uint8_t deviceSubclass;
    uint8_t deviceProtocol;
    uint16_t vendorID;
    uint16_t productID;
    uint8_t deviceVersionMajor;
    uint8_t deviceVersionMinor;
    uint8_t deviceVersionSubminor;
    WebString manufacturerName;
    WebString productName;
    WebString serialNumber;
    WebVector<Configuration> configurations;
};

} // namespace blink

#endif // WebUSBDeviceInfo_h

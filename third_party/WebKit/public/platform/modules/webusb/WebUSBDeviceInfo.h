// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBDeviceInfo_h
#define WebUSBDeviceInfo_h

#include "public/platform/WebString.h"

namespace blink {

struct WebUSBDeviceInfo {
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
};

} // namespace blink

#endif // WebUSBDeviceInfo_h

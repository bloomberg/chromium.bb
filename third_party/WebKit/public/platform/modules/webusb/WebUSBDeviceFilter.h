// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBDeviceFilter_h
#define WebUSBDeviceFilter_h

#include "public/platform/WebCommon.h"

namespace blink {

// Filter which specifies what devices the API consumer wants to enumerate.
// Properties which are not present in a filter should be treated as wildcards.
struct WebUSBDeviceFilter {
    WebUSBDeviceFilter() {}

    // Members corresponding to USBDeviceFilter properties specified in the IDL.
    uint16_t vendorID;
    uint16_t productID;
    uint8_t classCode;
    uint8_t subclassCode;
    uint8_t protocolCode;

    // Presence flags for each of the above properties.
    bool hasVendorID : 1;
    bool hasProductID : 1;
    bool hasClassCode : 1;
    bool hasSubclassCode : 1;
    bool hasProtocolCode : 1;
};

} // namespace blink

#endif // WebUSBDeviceFilter_h

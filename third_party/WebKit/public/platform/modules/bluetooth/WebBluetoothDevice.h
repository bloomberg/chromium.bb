// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothDevice_h
#define WebBluetoothDevice_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

// Information describing a Bluetooth device provided by the platform.
struct WebBluetoothDevice {
    enum class VendorIDSource {
        Unknown,
        Bluetooth,
        USB
    };

    WebBluetoothDevice(const WebString& id,
        const WebString& name,
        int8_t txPower,
        int8_t rssi,
        int32_t deviceClass,
        VendorIDSource vendorIDSource,
        uint16_t vendorID,
        uint16_t productID,
        uint16_t productVersion,
        const WebVector<WebString>& uuids)
        : id(id)
        , name(name)
        , txPower(txPower)
        , rssi(rssi)
        , deviceClass(deviceClass)
        , vendorIDSource(vendorIDSource)
        , vendorID(vendorID)
        , productID(productID)
        , productVersion(productVersion)
        , uuids(uuids)
    {
    }

    // Members corresponding to BluetoothDevice attributes as specified in IDL.
    const WebString id;
    const WebString name;
    // Powers:
    // A value of 127 denotes an invalid power.
    const int8_t txPower;
    const int8_t rssi;
    const int32_t deviceClass;
    const VendorIDSource vendorIDSource;
    const uint16_t vendorID;
    const uint16_t productID;
    const uint16_t productVersion;
    const WebVector<WebString> uuids;
};

} // namespace blink

#endif // WebBluetoothDevice_h

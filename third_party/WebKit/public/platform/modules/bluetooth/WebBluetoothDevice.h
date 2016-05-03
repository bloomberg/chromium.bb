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
    WebBluetoothDevice(const WebString& id,
        const WebString& name,
        int8_t txPower,
        int8_t rssi,
        const WebVector<WebString>& uuids)
        : id(id)
        , name(name)
        , txPower(txPower)
        , rssi(rssi)
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
    const WebVector<WebString> uuids;
};

} // namespace blink

#endif // WebBluetoothDevice_h

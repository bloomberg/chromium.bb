// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothGATTCharacteristicInit_h
#define WebBluetoothGATTCharacteristicInit_h

#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

// Contains members corresponding to BluetoothGATTCharacteristic attributes as
// specified in the IDL.
struct WebBluetoothGATTCharacteristicInit {
    WebBluetoothGATTCharacteristicInit(const WebString& characteristicInstanceID,
        const WebString& serviceInstanceID,
        const WebString& uuid,
        uint32_t characteristicProperties)
        : characteristicInstanceID(characteristicInstanceID)
        , serviceInstanceID(serviceInstanceID)
        , uuid(uuid)
        , characteristicProperties(characteristicProperties)
    {
    }

    const WebString characteristicInstanceID;
    const WebString serviceInstanceID;
    const WebString uuid;
    const uint32_t characteristicProperties;
    const WebVector<uint8_t> value;
};

} // namespace blink

#endif // WebBluetoothGATTCharacteristicInit_h

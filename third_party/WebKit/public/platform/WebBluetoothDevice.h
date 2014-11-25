// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothDevice_h
#define WebBluetoothDevice_h

#include "WebString.h"

namespace blink {

// Information describing a Bluetooth device provided by the platform.
struct WebBluetoothDevice {
    WebBluetoothDevice(const WebString& instanceId)
        : instanceId(instanceId)
    {
    }

    // Members corresponding to BluetoothDevice attributes as specified in IDL.
    const WebString instanceId;
};

} // namespace blink

#endif // WebBluetoothDevice_h

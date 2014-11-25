// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetooth_h
#define WebBluetooth_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"

namespace blink {

struct WebBluetoothDevice;
struct WebBluetoothError;

// Success and failure callbacks for requestDevice.
// WebBluetoothDevice and WebBluetoothError object ownership is transfered.
// FIXME: After removing references to old version in crrev.com/699843003, update to:
// typedef WebCallbacks<WebBluetoothDevice, WebBluetoothError> WebBluetoothRequestDeviceCallbacks;
// Old version:
typedef WebCallbacks<void, WebBluetoothError> WebBluetoothRequestDeviceCallbacks;

class WebBluetooth {
public:
    virtual ~WebBluetooth() { }

    // Requests a bluetooth device.
    // WebBluetoothRequestDeviceCallbacks ownership transferred to the client.
    virtual void requestDevice(WebBluetoothRequestDeviceCallbacks*) { BLINK_ASSERT_NOT_REACHED(); }
    virtual void requestDevice(WebCallbacks<WebBluetoothDevice, WebBluetoothError>*) { BLINK_ASSERT_NOT_REACHED(); };
};

} // namespace blink

#endif // WebBluetooth_h

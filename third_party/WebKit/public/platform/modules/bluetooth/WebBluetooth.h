// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetooth_h
#define WebBluetooth_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebString.h"

namespace blink {

struct WebBluetoothDevice;
struct WebBluetoothError;
struct WebBluetoothGATTRemoteServer;

// Success and failure callbacks for requestDevice.
// WebBluetoothDevice and WebBluetoothError object ownership is transfered.
typedef WebCallbacks<WebBluetoothDevice, WebBluetoothError> WebBluetoothRequestDeviceCallbacks;

// Success and failure callbacks for connectGATT.
typedef WebCallbacks<WebBluetoothGATTRemoteServer, WebBluetoothError> WebBluetoothConnectGATTCallbacks;

class WebBluetooth {
public:
    virtual ~WebBluetooth() { }

    // BluetoothDiscovery Methods:
    // See https://webbluetoothcg.github.io/web-bluetooth/#idl-def-bluetoothdiscovery
    // WebBluetoothRequestDeviceCallbacks ownership transferred to the client.
    virtual void requestDevice(WebBluetoothRequestDeviceCallbacks*) = 0;

    // BluetoothDevice methods:
    // See https://webbluetoothcg.github.io/web-bluetooth/#idl-def-bluetoothdevice
    // WebBluetoothConnectGATTCallbacks ownership transferred to the callee.
    virtual void connectGATT(const WebString& /* deviceInstanceID */,
        WebBluetoothConnectGATTCallbacks*) { }

    // BluetoothGATTRemoteServer methods:
    // See https://webbluetoothcg.github.io/web-bluetooth/#idl-def-bluetoothgattremoteserver
    virtual void disconnect() { }
    // TODO(ortuno): Properly define these methods once WebBluetoothServiceUuid
    // and WebBluetoothGATTService are defined.
    // virtual void getPrimaryService() { }
    // virtual void getPrimaryServices() { }
};

} // namespace blink

#endif // WebBluetooth_h

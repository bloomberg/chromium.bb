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
struct WebBluetoothGATTCharacteristic;
struct WebBluetoothGATTRemoteServer;
struct WebBluetoothGATTService;

// Success and failure callbacks for requestDevice.
// WebBluetoothDevice and WebBluetoothError object ownership is transferred.
typedef WebCallbacks<WebBluetoothDevice, WebBluetoothError> WebBluetoothRequestDeviceCallbacks;

// Success and failure callbacks for connectGATT.
// WebBluetoothGATTRemoteServer and WebBluetoothError object ownership is transferred.
typedef WebCallbacks<WebBluetoothGATTRemoteServer, WebBluetoothError> WebBluetoothConnectGATTCallbacks;

// Success and failure callbacks for getPrimaryService.
// WebBluetoothGATTService and WebBluetoothError object ownership is transferred.
typedef WebCallbacks<WebBluetoothGATTService, WebBluetoothError> WebBluetoothGetPrimaryServiceCallbacks;

// Success and failure callbacks for getCharacteristic.
// WebBluetoothCharacteristic and WebBluetoothError object ownership is transferred.
typedef WebCallbacks<WebBluetoothGATTCharacteristic, WebBluetoothError> WebBluetoothGetCharacteristicCallbacks;

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
    virtual void getPrimaryService(const WebString& deviceInstanceID,
        const WebString& serviceUUID,
        WebBluetoothGetPrimaryServiceCallbacks*) { }
    // virtual void getPrimaryServices() { }

    // BluetoothGATTService methods:
    // See https://webbluetoothcg.github.io/web-bluetooth/#idl-def-bluetoothgattservice
    virtual void getCharacteristic(const WebString& serviceInstanceID,
        const WebString& characteristicUUID,
        WebBluetoothGetCharacteristicCallbacks*) { }
};

} // namespace blink

#endif // WebBluetooth_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetooth_h
#define WebBluetooth_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/bluetooth/WebBluetoothError.h"

namespace blink {

class WebBluetoothGATTCharacteristic;

struct WebBluetoothDevice;
struct WebBluetoothGATTCharacteristicInit;
struct WebBluetoothGATTRemoteServer;
struct WebBluetoothGATTService;
struct WebRequestDeviceOptions;

// Success and failure callbacks for requestDevice.
using WebBluetoothRequestDeviceCallbacks = WebCallbacks<WebPassOwnPtr<WebBluetoothDevice>, const WebBluetoothError&>;

// Success and failure callbacks for connectGATT.
using WebBluetoothConnectGATTCallbacks = WebCallbacks<WebPassOwnPtr<WebBluetoothGATTRemoteServer>, const WebBluetoothError&>;

// Success and failure callbacks for getPrimaryService.
using WebBluetoothGetPrimaryServiceCallbacks = WebCallbacks<WebPassOwnPtr<WebBluetoothGATTService>, const WebBluetoothError&>;

// Success and failure callbacks for getCharacteristic.
using WebBluetoothGetCharacteristicCallbacks = WebCallbacks<WebPassOwnPtr<WebBluetoothGATTCharacteristicInit>, const WebBluetoothError&>;

// Success and failure callbacks for readValue.
using WebBluetoothReadValueCallbacks = WebCallbacks<const WebVector<uint8_t>&, const WebBluetoothError&>;

// Success and failure callbacks for writeValue.
using WebBluetoothWriteValueCallbacks = WebCallbacks<void, const WebBluetoothError&>;

// Success and failure callbacks for characteristic.startNotifications and
// characteristic.stopNotifications.
using WebBluetoothNotificationsCallbacks = WebCallbacks<void, const WebBluetoothError&>;

class WebBluetooth {
public:
    virtual ~WebBluetooth() { }

    // Bluetooth Methods:
    // See https://webbluetoothchrome.github.io/web-bluetooth/#device-discovery
    // WebBluetoothRequestDeviceCallbacks ownership transferred to the client.
    virtual void requestDevice(const WebRequestDeviceOptions&, WebBluetoothRequestDeviceCallbacks*) { }

    // BluetoothDevice methods:
    // See https://webbluetoothchrome.github.io/web-bluetooth/#idl-def-bluetoothdevice
    // WebBluetoothConnectGATTCallbacks ownership transferred to the callee.
    virtual void connectGATT(const WebString& deviceId,
        WebBluetoothConnectGATTCallbacks*) { }

    // BluetoothGATTRemoteServer methods:
    // See https://webbluetoothchrome.github.io/web-bluetooth/#idl-def-bluetoothgattremoteserver
    virtual void disconnect() { }
    virtual void getPrimaryService(const WebString& deviceId,
        const WebString& serviceUUID,
        WebBluetoothGetPrimaryServiceCallbacks*) { }
    // virtual void getPrimaryServices() { }

    // BluetoothGATTService methods:
    // See https://webbluetoothchrome.github.io/web-bluetooth/#idl-def-bluetoothgattservice
    virtual void getCharacteristic(const WebString& serviceInstanceID,
        const WebString& characteristicUUID,
        WebBluetoothGetCharacteristicCallbacks*) { }

    // BluetoothGATTCharacteristic methods:
    // See https://webbluetoothchrome.github.io/web-bluetooth/#bluetoothgattcharacteristic
    virtual void readValue(const WebString& characteristicInstanceID,
        WebBluetoothReadValueCallbacks*) { }
    virtual void writeValue(const WebString& characteristicInstanceID,
        const WebVector<uint8_t>& value,
        WebBluetoothWriteValueCallbacks*) {}
    virtual void startNotifications(const WebString& characteristicInstanceID,
        WebBluetoothGATTCharacteristic*,
        WebBluetoothNotificationsCallbacks*) {}
    virtual void stopNotifications(const WebString& characteristicInstanceID,
        WebBluetoothGATTCharacteristic*,
        WebBluetoothNotificationsCallbacks*) {}

    // Called when addEventListener is called on a characteristic.
    virtual void registerCharacteristicObject(
        const WebString& characteristicInstanceID,
        WebBluetoothGATTCharacteristic*) = 0;
    virtual void characteristicObjectRemoved(
        const WebString& characteristicInstanceID,
        WebBluetoothGATTCharacteristic*) {}
};

} // namespace blink

#endif // WebBluetooth_h

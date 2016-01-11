// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothDevice.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothGATTRemoteServer.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

BluetoothDevice::BluetoothDevice(PassOwnPtr<WebBluetoothDevice> webDevice)
    : m_webDevice(webDevice)
    , m_adData(BluetoothAdvertisingData::create(m_webDevice->txPower,
        m_webDevice->rssi))
{
}

BluetoothDevice* BluetoothDevice::take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothDevice> webDevice)
{
    ASSERT(webDevice);
    return new BluetoothDevice(webDevice);
}

DEFINE_TRACE(BluetoothDevice)
{
    visitor->trace(m_adData);
}

unsigned BluetoothDevice::deviceClass(bool& isNull)
{
    isNull = false;
    return m_webDevice->deviceClass;
}

String BluetoothDevice::vendorIDSource()
{
    switch (m_webDevice->vendorIDSource) {
    case WebBluetoothDevice::VendorIDSource::Unknown: return String();
    case WebBluetoothDevice::VendorIDSource::Bluetooth: return "bluetooth";
    case WebBluetoothDevice::VendorIDSource::USB: return "usb";
    }
    ASSERT_NOT_REACHED();
    return String();
}

unsigned BluetoothDevice::vendorID(bool& isNull)
{
    isNull = false;
    return m_webDevice->vendorID;
}

unsigned BluetoothDevice::productID(bool& isNull)
{
    isNull = false;
    return m_webDevice->productID;
}

unsigned BluetoothDevice::productVersion(bool& isNull)
{
    isNull = false;
    return m_webDevice->productVersion;
}

Vector<String> BluetoothDevice::uuids()
{
    Vector<String> uuids(m_webDevice->uuids.size());
    for (size_t i = 0; i < m_webDevice->uuids.size(); ++i)
        uuids[i] = m_webDevice->uuids[i];
    return uuids;
}

ScriptPromise BluetoothDevice::connectGATT(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    if (!webbluetooth)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->connectGATT(id(), new CallbackPromiseAdapter<BluetoothGATTRemoteServer, BluetoothError>(resolver));
    return promise;
}

} // namespace blink

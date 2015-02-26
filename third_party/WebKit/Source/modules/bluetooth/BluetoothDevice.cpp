// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothDevice.h"

#include "wtf/OwnPtr.h"

namespace blink {

BluetoothDevice::BluetoothDevice(const WebBluetoothDevice& webDevice)
    : m_webDevice(webDevice)
{
}

BluetoothDevice* BluetoothDevice::create(const WebBluetoothDevice& webDevice)
{
    return new BluetoothDevice(webDevice);
}

BluetoothDevice* BluetoothDevice::take(ScriptPromiseResolver*, WebBluetoothDevice* webDeviceRawPointer)
{
    OwnPtr<WebBluetoothDevice> webDevice = adoptPtr(webDeviceRawPointer);
    return BluetoothDevice::create(*webDevice);
}

void BluetoothDevice::dispose(WebBluetoothDevice* webDeviceRaw)
{
    delete webDeviceRaw;
}

unsigned BluetoothDevice::deviceClass(bool& isNull)
{
    isNull = false;
    return m_webDevice.deviceClass;
}

String BluetoothDevice::vendorIDSource()
{
    switch (m_webDevice.vendorIDSource) {
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
    return m_webDevice.vendorID;
}

unsigned BluetoothDevice::productID(bool& isNull)
{
    isNull = false;
    return m_webDevice.productID;
}

unsigned BluetoothDevice::productVersion(bool& isNull)
{
    isNull = false;
    return m_webDevice.productVersion;
}

bool BluetoothDevice::paired(bool& isNull)
{
    isNull = false;
    return m_webDevice.paired;
}

bool BluetoothDevice::connected(bool& isNull)
{
    isNull = false;
    return m_webDevice.connected;
}

Vector<String> BluetoothDevice::uuids(bool& isNull)
{
    isNull = false;
    Vector<String> uuids(m_webDevice.uuids.size());
    for (size_t i = 0; i < m_webDevice.uuids.size(); ++i)
        uuids[i] = m_webDevice.uuids[i];
    return uuids;
}

} // namespace blink


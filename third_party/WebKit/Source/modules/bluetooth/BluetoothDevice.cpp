// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothDevice.h"

#include "public/platform/WebBluetoothDevice.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothDevice::BluetoothDevice(const String& instanceId)
    : m_instanceId(instanceId)
{
}

BluetoothDevice* BluetoothDevice::create(const WebBluetoothDevice& webDevice)
{
    return new BluetoothDevice(webDevice.instanceId);
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

} // namespace blink


// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/Bluetooth.h"

#include "bindings/core/v8/ScriptPromise.h"

namespace blink {

ScriptPromise Bluetooth::requestDevice(ScriptState* s)
{
    return bluetoothDiscovery()->requestDevice(s);
}

BluetoothDiscovery* Bluetooth::bluetoothDiscovery()
{
    if (!m_bluetoothDiscovery)
        m_bluetoothDiscovery = new BluetoothDiscovery;
    return m_bluetoothDiscovery.get();
}

}; // blink

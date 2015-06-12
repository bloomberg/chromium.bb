// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothGATTCharacteristic.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothError.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothGATTCharacteristic::BluetoothGATTCharacteristic(PassOwnPtr<WebBluetoothGATTCharacteristic> webCharacteristic)
    : m_webCharacteristic(webCharacteristic)
{
}

BluetoothGATTCharacteristic* BluetoothGATTCharacteristic::take(ScriptPromiseResolver*, WebBluetoothGATTCharacteristic* webCharacteristicRawPointer)
{
    if (!webCharacteristicRawPointer) {
        return nullptr;
    }
    return new BluetoothGATTCharacteristic(adoptPtr(webCharacteristicRawPointer));
}

void BluetoothGATTCharacteristic::dispose(WebBluetoothGATTCharacteristic* webCharacteristic)
{
    delete webCharacteristic;
}

} // namespace blink

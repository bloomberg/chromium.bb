// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothGATTService.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothGATTCharacteristic.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothGATTService::BluetoothGATTService(PassOwnPtr<WebBluetoothGATTService> webService)
    : m_webService(webService)
{
}

BluetoothGATTService* BluetoothGATTService::take(ScriptPromiseResolver*, WebBluetoothGATTService* webServiceRawPointer)
{
    if (!webServiceRawPointer) {
        return nullptr;
    }
    return new BluetoothGATTService(adoptPtr(webServiceRawPointer));
}

void BluetoothGATTService::dispose(WebBluetoothGATTService* webService)
{
    delete webService;
}

ScriptPromise BluetoothGATTService::getCharacteristic(ScriptState* scriptState,
    String characteristicUUID)
{
    WebBluetooth* webbluetooth = Platform::current()->bluetooth();

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->getCharacteristic(m_webService->serviceInstanceID, characteristicUUID, new CallbackPromiseAdapter<BluetoothGATTCharacteristic, BluetoothError>(resolver));

    return promise;
}

} // namespace blink

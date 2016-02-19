// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTService.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

BluetoothRemoteGATTService::BluetoothRemoteGATTService(PassOwnPtr<WebBluetoothRemoteGATTService> webService)
    : m_webService(webService)
{
}

BluetoothRemoteGATTService* BluetoothRemoteGATTService::take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothRemoteGATTService> webService)
{
    if (!webService) {
        return nullptr;
    }
    return new BluetoothRemoteGATTService(webService);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristic(ScriptState* scriptState,
    const StringOrUnsignedLong& characteristic, ExceptionState& exceptionState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);

    String characteristicUUID = BluetoothUUID::getCharacteristic(characteristic, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->getCharacteristic(m_webService->serviceInstanceID, characteristicUUID, new CallbackPromiseAdapter<BluetoothRemoteGATTCharacteristic, BluetoothError>(resolver));

    return promise;
}

} // namespace blink

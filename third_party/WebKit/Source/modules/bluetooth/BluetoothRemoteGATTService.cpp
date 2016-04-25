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
    : m_webService(std::move(webService))
{
}

BluetoothRemoteGATTService* BluetoothRemoteGATTService::take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothRemoteGATTService> webService)
{
    if (!webService) {
        return nullptr;
    }
    return new BluetoothRemoteGATTService(std::move(webService));
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

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(ScriptState* scriptState, ExceptionState&)
{
    return getCharacteristicsImpl(scriptState, String());
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(ScriptState* scriptState, const StringOrUnsignedLong& characteristic, ExceptionState& exceptionState)
{
    String characteristicUUID = BluetoothUUID::getCharacteristic(characteristic, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getCharacteristicsImpl(scriptState, characteristicUUID);
}

// Class that allows us to use CallbackPromiseAdapter to resolve a promise with a
// vector owning BluetoothRemoteGATTCharacteristics.
class RemoteCharacteristicArray {
    STATIC_ONLY(RemoteCharacteristicArray);
public:
    using WebType = OwnPtr<WebVector<WebBluetoothRemoteGATTCharacteristicInit*>>;
    static HeapVector<Member<BluetoothRemoteGATTCharacteristic>> take(ScriptPromiseResolver* resolver, PassOwnPtr<WebVector<WebBluetoothRemoteGATTCharacteristicInit*>> webCharacteristics)
    {
        HeapVector<Member<BluetoothRemoteGATTCharacteristic>> characteristics;
        characteristics.reserveInitialCapacity(webCharacteristics->size());
        for (WebBluetoothRemoteGATTCharacteristicInit* webCharacteristic : *webCharacteristics) {
            characteristics.append(BluetoothRemoteGATTCharacteristic::take(resolver, adoptPtr(webCharacteristic)));
        }
        return characteristics;
    }
};

ScriptPromise BluetoothRemoteGATTService::getCharacteristicsImpl(ScriptState* scriptState, String characteristicsUUID)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->getCharacteristics(m_webService->serviceInstanceID, characteristicsUUID, new CallbackPromiseAdapter<RemoteCharacteristicArray, BluetoothError>(resolver));

    return promise;
}

} // namespace blink

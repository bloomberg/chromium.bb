// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTService.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
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

// Class that allows us to resolve the promise with a single Characteristic or
// with a vector owning the characteristics.
class GetCharacteristicsCallback : public WebBluetoothGetCharacteristicsCallbacks {
public:
    GetCharacteristicsCallback(mojom::WebBluetoothGATTQueryQuantity quantity, ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
        , m_quantity(quantity) {}

    void onSuccess(const WebVector<WebBluetoothRemoteGATTCharacteristicInit*>& webCharacteristics) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;

        if (m_quantity == mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
            DCHECK_EQ(1u, webCharacteristics.size());
            m_resolver->resolve(BluetoothRemoteGATTCharacteristic::take(m_resolver, adoptPtr(webCharacteristics[0])));
            return;
        }

        HeapVector<Member<BluetoothRemoteGATTCharacteristic>> characteristics;
        characteristics.reserveInitialCapacity(webCharacteristics.size());
        for (WebBluetoothRemoteGATTCharacteristicInit* webCharacteristic : webCharacteristics) {
            characteristics.append(BluetoothRemoteGATTCharacteristic::take(m_resolver, adoptPtr(webCharacteristic)));
        }
        m_resolver->resolve(characteristics);
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }
private:
    Persistent<ScriptPromiseResolver> m_resolver;
    mojom::WebBluetoothGATTQueryQuantity m_quantity;
};

ScriptPromise BluetoothRemoteGATTService::getCharacteristic(ScriptState* scriptState, const StringOrUnsignedLong& characteristic, ExceptionState& exceptionState)
{
    String characteristicUUID = BluetoothUUID::getCharacteristic(characteristic, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getCharacteristicsImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::SINGLE, characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(ScriptState* scriptState, const StringOrUnsignedLong& characteristic, ExceptionState& exceptionState)
{
    String characteristicUUID = BluetoothUUID::getCharacteristic(characteristic, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    return getCharacteristicsImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::MULTIPLE, characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(ScriptState* scriptState, ExceptionState&)
{
    return getCharacteristicsImpl(scriptState, mojom::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristicsImpl(ScriptState* scriptState, mojom::WebBluetoothGATTQueryQuantity quantity, String characteristicsUUID)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->getCharacteristics(m_webService->serviceInstanceID, quantity, characteristicsUUID, new GetCharacteristicsCallback(quantity, resolver));

    return promise;
}

} // namespace blink

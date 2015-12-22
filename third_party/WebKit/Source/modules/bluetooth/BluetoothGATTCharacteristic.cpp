// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothGATTCharacteristic.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "modules/bluetooth/BluetoothCharacteristicProperties.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/ConvertWebVectorToArrayBuffer.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

BluetoothGATTCharacteristic::BluetoothGATTCharacteristic(ExecutionContext* context, PassOwnPtr<WebBluetoothGATTCharacteristicInit> webCharacteristic)
    : ActiveDOMObject(context)
    , m_webCharacteristic(webCharacteristic)
    , m_stopped(false)
{
    m_properties = BluetoothCharacteristicProperties::create(m_webCharacteristic->characteristicProperties);
    // See example in Source/platform/heap/ThreadState.h
    ThreadState::current()->registerPreFinalizer(this);
}

BluetoothGATTCharacteristic* BluetoothGATTCharacteristic::take(ScriptPromiseResolver* resolver, PassOwnPtr<WebBluetoothGATTCharacteristicInit> webCharacteristic)
{
    if (!webCharacteristic) {
        return nullptr;
    }
    BluetoothGATTCharacteristic* characteristic = new BluetoothGATTCharacteristic(resolver->executionContext(), webCharacteristic);
    // See note in ActiveDOMObject about suspendIfNeeded.
    characteristic->suspendIfNeeded();
    return characteristic;
}

void BluetoothGATTCharacteristic::dispatchCharacteristicValueChanged(
    const WebVector<uint8_t>& value)
{
    static_assert(sizeof(*value.data()) == 1, "uint8_t should be a single byte");

    m_value = DOMArrayBuffer::create(value.data(), value.size());

    dispatchEvent(Event::create(EventTypeNames::characteristicvaluechanged));
}

void BluetoothGATTCharacteristic::stop()
{
    notifyCharacteristicObjectRemoved();
}

void BluetoothGATTCharacteristic::dispose()
{
    notifyCharacteristicObjectRemoved();
}

void BluetoothGATTCharacteristic::notifyCharacteristicObjectRemoved()
{
    if (!m_stopped) {
        m_stopped = true;
        WebBluetooth* webbluetooth = BluetoothSupplement::fromExecutionContext(ActiveDOMObject::executionContext());
        webbluetooth->characteristicObjectRemoved(m_webCharacteristic->characteristicInstanceID, this);
    }
}

const WTF::AtomicString& BluetoothGATTCharacteristic::interfaceName() const
{
    return EventTargetNames::BluetoothGATTCharacteristic;
}

ExecutionContext* BluetoothGATTCharacteristic::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool BluetoothGATTCharacteristic::addEventListenerInternal(const AtomicString& eventType, PassRefPtrWillBeRawPtr<EventListener> listener, const EventListenerOptions& options)
{
    // We will also need to unregister a characteristic once all the event
    // listeners have been removed. See http://crbug.com/541390
    if (eventType == EventTypeNames::characteristicvaluechanged) {
        WebBluetooth* webbluetooth = BluetoothSupplement::fromExecutionContext(executionContext());
        webbluetooth->registerCharacteristicObject(m_webCharacteristic->characteristicInstanceID, this);
    }
    return EventTarget::addEventListenerInternal(eventType, listener, options);
}

ScriptPromise BluetoothGATTCharacteristic::readValue(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->readValue(m_webCharacteristic->characteristicInstanceID, new CallbackPromiseAdapter<ConvertWebVectorToArrayBuffer, BluetoothError>(resolver));

    return promise;
}

ScriptPromise BluetoothGATTCharacteristic::writeValue(ScriptState* scriptState, const DOMArrayPiece& value)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    // Partial implementation of writeValue algorithm:
    // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothgattcharacteristic-writevalue

    // If bytes is more than 512 bytes long (the maximum length of an attribute
    // value, per Long Attribute Values) return a promise rejected with an
    // InvalidModificationError and abort.
    if (value.byteLength() > 512)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidModificationError, "Value can't exceed 512 bytes."));

    // Let valueVector be a copy of the bytes held by value.
    WebVector<uint8_t> valueVector(value.bytes(), value.byteLength());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);

    ScriptPromise promise = resolver->promise();
    webbluetooth->writeValue(m_webCharacteristic->characteristicInstanceID, valueVector, new CallbackPromiseAdapter<void, BluetoothError>(resolver));

    return promise;
}

ScriptPromise BluetoothGATTCharacteristic::startNotifications(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->startNotifications(m_webCharacteristic->characteristicInstanceID, this, new CallbackPromiseAdapter<void, BluetoothError>(resolver));
    return promise;
}

ScriptPromise BluetoothGATTCharacteristic::stopNotifications(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->stopNotifications(m_webCharacteristic->characteristicInstanceID, this, new CallbackPromiseAdapter<void, BluetoothError>(resolver));
    return promise;
}

DEFINE_TRACE(BluetoothGATTCharacteristic)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<BluetoothGATTCharacteristic>::trace(visitor);
    ActiveDOMObject::trace(visitor);
    visitor->trace(m_properties);
}

} // namespace blink

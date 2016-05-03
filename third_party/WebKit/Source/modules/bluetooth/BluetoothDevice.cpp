// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothDevice.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/events/Event.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTServer.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

BluetoothDevice::BluetoothDevice(ExecutionContext* context, PassOwnPtr<WebBluetoothDevice> webDevice)
    : ActiveDOMObject(context)
    , m_webDevice(std::move(webDevice))
    , m_adData(BluetoothAdvertisingData::create(m_webDevice->txPower, m_webDevice->rssi))
    , m_gatt(BluetoothRemoteGATTServer::create(this))
{
    // See example in Source/platform/heap/ThreadState.h
    ThreadState::current()->registerPreFinalizer(this);
}

BluetoothDevice* BluetoothDevice::take(ScriptPromiseResolver* resolver, PassOwnPtr<WebBluetoothDevice> webDevice)
{
    ASSERT(webDevice);
    BluetoothDevice* device = new BluetoothDevice(resolver->getExecutionContext(), std::move(webDevice));
    device->suspendIfNeeded();
    return device;
}

void BluetoothDevice::dispose()
{
    disconnectGATTIfConnected();
}

void BluetoothDevice::stop()
{
    disconnectGATTIfConnected();
}

bool BluetoothDevice::disconnectGATTIfConnected()
{
    if (m_gatt->connected()) {
        m_gatt->setConnected(false);
        BluetoothSupplement::fromExecutionContext(getExecutionContext())->disconnect(id());
        return true;
    }
    return false;
}

const WTF::AtomicString& BluetoothDevice::interfaceName() const
{
    return EventTargetNames::BluetoothDevice;
}

ExecutionContext* BluetoothDevice::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

DEFINE_TRACE(BluetoothDevice)
{
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
    visitor->trace(m_adData);
    visitor->trace(m_gatt);
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
    return m_gatt->connect(scriptState);
}

} // namespace blink

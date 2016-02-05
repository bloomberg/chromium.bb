// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothDevice.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/page/PageVisibilityState.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothGATTRemoteServer.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {

BluetoothDevice::BluetoothDevice(ExecutionContext* context, PassOwnPtr<WebBluetoothDevice> webDevice)
    : ActiveDOMObject(context)
    , PageLifecycleObserver(toDocument(context)->page())
    , m_webDevice(webDevice)
    , m_adData(BluetoothAdvertisingData::create(m_webDevice->txPower,
        m_webDevice->rssi))
    , m_gatt(BluetoothGATTRemoteServer::create(this))
{
    // See example in Source/platform/heap/ThreadState.h
    ThreadState::current()->registerPreFinalizer(this);
}

BluetoothDevice* BluetoothDevice::take(ScriptPromiseResolver* resolver, PassOwnPtr<WebBluetoothDevice> webDevice)
{
    ASSERT(webDevice);
    BluetoothDevice* device = new BluetoothDevice(resolver->executionContext(), webDevice);
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

void BluetoothDevice::pageVisibilityChanged()
{
    if (!page()->isPageVisible() && disconnectGATTIfConnected()) {
        dispatchEvent(Event::create(EventTypeNames::gattserverdisconnected));
    }
}

bool BluetoothDevice::disconnectGATTIfConnected()
{
    if (m_gatt->connected()) {
        m_gatt->setConnected(false);
        BluetoothSupplement::fromExecutionContext(executionContext())->disconnect(id());
        return true;
    }
    return false;
}

const WTF::AtomicString& BluetoothDevice::interfaceName() const
{
    return EventTargetNames::BluetoothDevice;
}

ExecutionContext* BluetoothDevice::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

DEFINE_TRACE(BluetoothDevice)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<BluetoothDevice>::trace(visitor);
    ActiveDOMObject::trace(visitor);
    PageLifecycleObserver::trace(visitor);
    visitor->trace(m_adData);
    visitor->trace(m_gatt);
}

unsigned BluetoothDevice::deviceClass(bool& isNull)
{
    isNull = false;
    return m_webDevice->deviceClass;
}

String BluetoothDevice::vendorIDSource()
{
    switch (m_webDevice->vendorIDSource) {
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
    return m_webDevice->vendorID;
}

unsigned BluetoothDevice::productID(bool& isNull)
{
    isNull = false;
    return m_webDevice->productID;
}

unsigned BluetoothDevice::productVersion(bool& isNull)
{
    isNull = false;
    return m_webDevice->productVersion;
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

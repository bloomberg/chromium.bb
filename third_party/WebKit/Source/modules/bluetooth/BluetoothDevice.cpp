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
#include <memory>

namespace blink {

BluetoothDevice::BluetoothDevice(
    ExecutionContext* context,
    std::unique_ptr<WebBluetoothDeviceInit> webDevice)
    : ContextLifecycleObserver(context),
      m_webDevice(std::move(webDevice)),
      m_gatt(BluetoothRemoteGATTServer::create(this)) {
  // See example in Source/platform/heap/ThreadState.h
  ThreadState::current()->registerPreFinalizer(this);
}

BluetoothDevice* BluetoothDevice::take(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<WebBluetoothDeviceInit> webDevice) {
  ASSERT(webDevice);
  return new BluetoothDevice(resolver->getExecutionContext(),
                             std::move(webDevice));
}

void BluetoothDevice::dispose() {
  disconnectGATTIfConnected();
}

void BluetoothDevice::contextDestroyed() {
  disconnectGATTIfConnected();
}

bool BluetoothDevice::disconnectGATTIfConnected() {
  if (m_gatt->connected()) {
    m_gatt->setConnected(false);
    m_gatt->ClearActiveAlgorithms();
    BluetoothSupplement::fromExecutionContext(getExecutionContext())
        ->disconnect(id());
    return true;
  }
  return false;
}

const WTF::AtomicString& BluetoothDevice::interfaceName() const {
  return EventTargetNames::BluetoothDevice;
}

ExecutionContext* BluetoothDevice::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

void BluetoothDevice::dispatchGattServerDisconnected() {
  if (m_gatt->connected()) {
    m_gatt->setConnected(false);
    m_gatt->ClearActiveAlgorithms();
    dispatchEvent(Event::createBubble(EventTypeNames::gattserverdisconnected));
  }
}

DEFINE_TRACE(BluetoothDevice) {
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_gatt);
}

}  // namespace blink

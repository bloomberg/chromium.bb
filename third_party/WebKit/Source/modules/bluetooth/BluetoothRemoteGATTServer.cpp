// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTServer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothUUID.h"

namespace blink {

namespace {

const char kGATTServerDisconnected[] =
    "GATT Server disconnected while retrieving services.";
const char kGATTServerNotConnected[] =
    "GATT Server is disconnected. Cannot retrieve services.";

}  // namespace

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(BluetoothDevice* device)
    : m_device(device), m_connected(false) {}

BluetoothRemoteGATTServer* BluetoothRemoteGATTServer::create(
    BluetoothDevice* device) {
  return new BluetoothRemoteGATTServer(device);
}

void BluetoothRemoteGATTServer::AddToActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  auto result = m_activeAlgorithms.add(resolver);
  CHECK(result.isNewEntry);
}

bool BluetoothRemoteGATTServer::RemoveFromActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  if (!m_activeAlgorithms.contains(resolver)) {
    return false;
  }
  m_activeAlgorithms.remove(resolver);
  return true;
}

DEFINE_TRACE(BluetoothRemoteGATTServer) {
  visitor->trace(m_activeAlgorithms);
  visitor->trace(m_device);
}

void BluetoothRemoteGATTServer::ConnectCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    setConnected(true);
    resolver->resolve(this);
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTServer::connect(ScriptState* scriptState) {
  m_device->bluetooth()->addDevice(device()->id(), device());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  mojom::blink::WebBluetoothDeviceIdPtr deviceId =
      BluetoothDevice::createMojoDeviceId(device()->id());
  service->RemoteServerConnect(
      std::move(deviceId),
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTServer::ConnectCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* scriptState) {
  if (!m_connected)
    return;
  device()->cleanupDisconnectedDeviceAndFireEvent();
  m_device->bluetooth()->removeDevice(device()->id());
  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  mojom::blink::WebBluetoothDeviceIdPtr deviceId =
      BluetoothDevice::createMojoDeviceId(device()->id());
  service->RemoteServerDisconnect(std::move(deviceId));
}

// Callback that allows us to resolve the promise with a single service or
// with a vector owning the services.
void BluetoothRemoteGATTServer::GetPrimaryServicesCallback(
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    Optional<Vector<mojom::blink::WebBluetoothRemoteGATTServicePtr>> services) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        DOMException::create(NetworkError, kGATTServerDisconnected));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(services);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, services->size());
      resolver->resolve(m_device->getOrCreateRemoteGATTService(
          services.value()[0]->instance_id, services.value()[0]->uuid,
          true /* isPrimary */, device()->id()));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTService>> gattServices;
    gattServices.reserveInitialCapacity(services->size());

    for (const auto& service : services.value()) {
      gattServices.push_back(m_device->getOrCreateRemoteGATTService(
          service->instance_id, service->uuid, true /* isPrimary */,
          device()->id()));
    }
    resolver->resolve(gattServices);
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryService(
    ScriptState* scriptState,
    const StringOrUnsignedLong& service,
    ExceptionState& exceptionState) {
  String serviceUUID = BluetoothUUID::getService(service, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
      serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* scriptState,
    const StringOrUnsignedLong& service,
    ExceptionState& exceptionState) {
  String serviceUUID = BluetoothUUID::getService(service, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* scriptState,
    ExceptionState&) {
  return getPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServicesImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    String servicesUUID) {
  // We always check that the device is connected.
  if (!connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  mojom::blink::WebBluetoothDeviceIdPtr deviceId =
      BluetoothDevice::createMojoDeviceId(device()->id());
  WTF::Optional<String> uuid = WTF::nullopt;
  if (!servicesUUID.isEmpty())
    uuid = servicesUUID;

  service->RemoteServerGetPrimaryServices(
      std::move(deviceId), quantity, uuid,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTServer::GetPrimaryServicesCallback,
                    wrapPersistent(this), quantity, wrapPersistent(resolver))));
  return promise;
}

}  // namespace blink

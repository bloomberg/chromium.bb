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
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include <utility>

namespace blink {

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(ExecutionContext* context,
                                                     BluetoothDevice* device)
    : ContextLifecycleObserver(context), m_device(device), m_connected(false) {}

BluetoothRemoteGATTServer* BluetoothRemoteGATTServer::Create(
    ExecutionContext* context,
    BluetoothDevice* device) {
  return new BluetoothRemoteGATTServer(context, device);
}

void BluetoothRemoteGATTServer::contextDestroyed(ExecutionContext*) {
  Dispose();
}

void BluetoothRemoteGATTServer::GATTServerDisconnected() {
  DispatchDisconnected();
}

void BluetoothRemoteGATTServer::AddToActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  auto result = m_activeAlgorithms.insert(resolver);
  CHECK(result.isNewEntry);
}

bool BluetoothRemoteGATTServer::RemoveFromActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  if (!m_activeAlgorithms.contains(resolver)) {
    return false;
  }
  m_activeAlgorithms.erase(resolver);
  return true;
}

void BluetoothRemoteGATTServer::DisconnectIfConnected() {
  if (m_connected) {
    SetConnected(false);
    ClearActiveAlgorithms();
    mojom::blink::WebBluetoothService* service =
        m_device->bluetooth()->Service();
    service->RemoteServerDisconnect(m_device->id());
  }
}

void BluetoothRemoteGATTServer::CleanupDisconnectedDeviceAndFireEvent() {
  DCHECK(m_connected);
  SetConnected(false);
  ClearActiveAlgorithms();
  m_device->ClearAttributeInstanceMapAndFireEvent();
}

void BluetoothRemoteGATTServer::DispatchDisconnected() {
  if (!m_connected) {
    return;
  }
  CleanupDisconnectedDeviceAndFireEvent();
}

void BluetoothRemoteGATTServer::Dispose() {
  DisconnectIfConnected();
  // The pipe to this object must be closed when is marked unreachable to
  // prevent messages from being dispatched before lazy sweeping.
  m_clientBindings.CloseAllBindings();
}

DEFINE_TRACE(BluetoothRemoteGATTServer) {
  visitor->trace(m_activeAlgorithms);
  visitor->trace(m_device);
  ContextLifecycleObserver::trace(visitor);
}

void BluetoothRemoteGATTServer::ConnectCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    SetConnected(true);
    resolver->resolve(this);
  } else {
    resolver->reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTServer::connect(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
  mojom::blink::WebBluetoothServerClientAssociatedPtrInfo ptrInfo;
  auto request = mojo::MakeRequest(&ptrInfo);
  m_clientBindings.AddBinding(this, std::move(request));

  service->RemoteServerConnect(
      m_device->id(), std::move(ptrInfo),
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTServer::ConnectCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* scriptState) {
  if (!m_connected)
    return;
  CleanupDisconnectedDeviceAndFireEvent();
  m_clientBindings.CloseAllBindings();
  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
  service->RemoteServerDisconnect(m_device->id());
}

// Callback that allows us to resolve the promise with a single service or
// with a vector owning the services.
void BluetoothRemoteGATTServer::GetPrimaryServicesCallback(
    const String& requestedServiceUUID,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    Optional<Vector<mojom::blink::WebBluetoothRemoteGATTServicePtr>> services) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothError::CreateNotConnectedException(
        BluetoothOperation::ServicesRetrieval));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(services);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, services->size());
      resolver->resolve(m_device->GetOrCreateRemoteGATTService(
          std::move(services.value()[0]), true /* isPrimary */,
          m_device->id()));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTService>> gattServices;
    gattServices.reserveInitialCapacity(services->size());

    for (auto& service : services.value()) {
      gattServices.push_back(m_device->GetOrCreateRemoteGATTService(
          std::move(service), true /* isPrimary */, m_device->id()));
    }
    resolver->resolve(gattServices);
  } else {
    if (result == mojom::blink::WebBluetoothResult::SERVICE_NOT_FOUND) {
      resolver->reject(BluetoothError::CreateDOMException(
          BluetoothErrorCode::ServiceNotFound, "No Services matching UUID " +
                                                   requestedServiceUUID +
                                                   " found in Device."));
    } else {
      resolver->reject(BluetoothError::CreateDOMException(result));
    }
  }
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryService(
    ScriptState* scriptState,
    const StringOrUnsignedLong& service,
    ExceptionState& exceptionState) {
  String serviceUUID = BluetoothUUID::getService(service, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return GetPrimaryServicesImpl(
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

  return GetPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      serviceUUID);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* scriptState,
    ExceptionState&) {
  return GetPrimaryServicesImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTServer::GetPrimaryServicesImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    String servicesUUID) {
  if (!m_connected) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, BluetoothError::CreateNotConnectedException(
                         BluetoothOperation::ServicesRetrieval));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
  service->RemoteServerGetPrimaryServices(
      m_device->id(), quantity, servicesUUID,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTServer::GetPrimaryServicesCallback,
                    wrapPersistent(this), servicesUUID, quantity,
                    wrapPersistent(resolver))));
  return promise;
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/Bluetooth.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "modules/bluetooth/RequestDeviceOptions.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include <memory>
#include <utility>

namespace blink {

namespace {
// Per the Bluetooth Spec: The name is a user-friendly name associated with the
// device and consists of a maximum of 248 bytes coded according to the UTF-8
// standard.
const size_t kMaxDeviceNameLength = 248;
const char kDeviceNameTooLong[] =
    "A device name can't be longer than 248 bytes.";
}  // namespace

static void canonicalizeFilter(
    const BluetoothScanFilterInit& filter,
    mojom::blink::WebBluetoothScanFilterPtr& canonicalizedFilter,
    ExceptionState& exceptionState) {
  if (!(filter.hasServices() || filter.hasName() || filter.hasNamePrefix())) {
    exceptionState.throwTypeError(
        "A filter must restrict the devices in some way.");
    return;
  }

  if (filter.hasServices()) {
    if (filter.services().size() == 0) {
      exceptionState.throwTypeError(
          "'services', if present, must contain at least one service.");
      return;
    }
    canonicalizedFilter->services.emplace();
    for (const StringOrUnsignedLong& service : filter.services()) {
      const String& validatedService =
          BluetoothUUID::getService(service, exceptionState);
      if (exceptionState.hadException())
        return;
      canonicalizedFilter->services->push_back(validatedService);
    }
  }

  if (filter.hasName()) {
    size_t nameLength = filter.name().utf8().length();
    if (nameLength > kMaxDeviceNameLength) {
      exceptionState.throwTypeError(kDeviceNameTooLong);
      return;
    }
    canonicalizedFilter->name = filter.name();
  }

  if (filter.hasNamePrefix()) {
    size_t namePrefixLength = filter.namePrefix().utf8().length();
    if (namePrefixLength > kMaxDeviceNameLength) {
      exceptionState.throwTypeError(kDeviceNameTooLong);
      return;
    }
    if (filter.namePrefix().length() == 0) {
      exceptionState.throwTypeError(
          "'namePrefix', if present, must me non-empty.");
      return;
    }
    canonicalizedFilter->name_prefix = filter.namePrefix();
  }
}

static void convertRequestDeviceOptions(
    const RequestDeviceOptions& options,
    mojom::blink::WebBluetoothRequestDeviceOptionsPtr& result,
    ExceptionState& exceptionState) {
  if (!(options.hasFilters() ^ options.acceptAllDevices())) {
    exceptionState.throwTypeError(
        "Either 'filters' should be present or 'acceptAllDevices' should be "
        "true, but not both.");
    return;
  }

  result->accept_all_devices = options.acceptAllDevices();

  if (options.hasFilters()) {
    if (options.filters().isEmpty()) {
      exceptionState.throwTypeError(
          "'filters' member must be non-empty to find any devices.");
      return;
    }

    result->filters.emplace();

    for (const BluetoothScanFilterInit& filter : options.filters()) {
      auto canonicalizedFilter = mojom::blink::WebBluetoothScanFilter::New();

      canonicalizeFilter(filter, canonicalizedFilter, exceptionState);

      if (exceptionState.hadException())
        return;

      result->filters.value().push_back(std::move(canonicalizedFilter));
    }
  }

  if (options.hasOptionalServices()) {
    for (const StringOrUnsignedLong& optionalService :
         options.optionalServices()) {
      const String& validatedOptionalService =
          BluetoothUUID::getService(optionalService, exceptionState);
      if (exceptionState.hadException())
        return;
      result->optional_services.push_back(validatedOptionalService);
    }
  }
}

void Bluetooth::dispose() {
  // The pipe to this object must be closed when is marked unreachable to
  // prevent messages from being dispatched before lazy sweeping.
  if (m_clientBinding.is_bound())
    m_clientBinding.Close();
}

void Bluetooth::RequestDeviceCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    mojom::blink::WebBluetoothDevicePtr device) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    BluetoothDevice* bluetoothDevice =
        getBluetoothDeviceRepresentingDevice(std::move(device), resolver);
    resolver->resolve(bluetoothDevice);
  } else {
    resolver->reject(BluetoothError::createDOMException(result));
  }
}

// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
ScriptPromise Bluetooth::requestDevice(ScriptState* scriptState,
                                       const RequestDeviceOptions& options,
                                       ExceptionState& exceptionState) {
  ExecutionContext* context = scriptState->getExecutionContext();

  // If the incumbent settings object is not a secure context, reject promise
  // with a SecurityError and abort these steps.
  String errorMessage;
  if (!context->isSecureContext(errorMessage)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SecurityError, errorMessage));
  }

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  if (!UserGestureIndicator::consumeUserGesture()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            SecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  if (!m_service) {
    InterfaceProvider* interfaceProvider = nullptr;
    if (context->isDocument()) {
      Document* document = toDocument(context);
      if (document->frame())
        interfaceProvider = document->frame()->interfaceProvider();
    }

    if (interfaceProvider)
      interfaceProvider->getInterface(mojo::MakeRequest(&m_service));

    if (m_service) {
      // Create an associated interface ptr and pass it to the
      // WebBluetoothService so that it can send us events without us
      // prompting.
      mojom::blink::WebBluetoothServiceClientAssociatedPtrInfo ptrInfo;
      m_clientBinding.Bind(&ptrInfo);
      m_service->SetClient(std::move(ptrInfo));
    }
  }

  if (!m_service) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotSupportedError));
  }

  // In order to convert the arguments from service names and aliases to just
  // UUIDs, do the following substeps:
  auto deviceOptions = mojom::blink::WebBluetoothRequestDeviceOptions::New();
  convertRequestDeviceOptions(options, deviceOptions, exceptionState);

  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // Record the eTLD+1 of the frame using the API.
  Document* document = toDocument(context);
  Platform::current()->recordRapporURL("Bluetooth.APIUsage.Origin",
                                       document->url());

  // Subsequent steps are handled in the browser process.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  service()->RequestDevice(
      std::move(deviceOptions),
      convertToBaseCallback(WTF::bind(&Bluetooth::RequestDeviceCallback,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));
  return promise;
}

void Bluetooth::addToConnectedDevicesMap(const String& deviceId,
                                         BluetoothDevice* device) {
  m_connectedDevices.insert(deviceId, device);
}

void Bluetooth::removeFromConnectedDevicesMap(const String& deviceId) {
  m_connectedDevices.remove(deviceId);
}

void Bluetooth::registerCharacteristicObject(
    const String& characteristicInstanceId,
    BluetoothRemoteGATTCharacteristic* characteristic) {
  m_activeCharacteristics.insert(characteristicInstanceId, characteristic);
}

void Bluetooth::characteristicObjectRemoved(
    const String& characteristicInstanceId) {
  m_activeCharacteristics.remove(characteristicInstanceId);
}

DEFINE_TRACE(Bluetooth) {
  visitor->trace(m_deviceInstanceMap);
  visitor->trace(m_activeCharacteristics);
  visitor->trace(m_connectedDevices);
}

Bluetooth::Bluetooth() : m_clientBinding(this) {}

void Bluetooth::RemoteCharacteristicValueChanged(
    const WTF::String& characteristicInstanceId,
    const WTF::Vector<uint8_t>& value) {
  BluetoothRemoteGATTCharacteristic* characteristic =
      m_activeCharacteristics.at(characteristicInstanceId);
  if (characteristic)
    characteristic->dispatchCharacteristicValueChanged(value);
}

void Bluetooth::GattServerDisconnected(const WTF::String& deviceId) {
  BluetoothDevice* device = m_connectedDevices.at(deviceId);
  if (device) {
    // Remove device from the map before calling dispatchGattServerDisconnected
    // to avoid removing a device the gattserverdisconnected event handler might
    // have re-connected.
    m_connectedDevices.remove(deviceId);
    device->dispatchGattServerDisconnected();
  }
}

BluetoothDevice* Bluetooth::getBluetoothDeviceRepresentingDevice(
    mojom::blink::WebBluetoothDevicePtr devicePtr,
    ScriptPromiseResolver* resolver) {
  WTF::String id = devicePtr->id;
  BluetoothDevice* device = m_deviceInstanceMap.at(id);
  if (!device) {
    device = BluetoothDevice::take(resolver, std::move(devicePtr), this);
    auto result = m_deviceInstanceMap.insert(id, device);
    DCHECK(result.isNewEntry);
  }
  return device;
}

}  // namespace blink

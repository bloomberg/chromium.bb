// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/Bluetooth.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "build/build_config.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "modules/bluetooth/RequestDeviceOptions.h"
#include "public/platform/Platform.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

namespace {
// Per the Bluetooth Spec: The name is a user-friendly name associated with the
// device and consists of a maximum of 248 bytes coded according to the UTF-8
// standard.
const size_t kMaxDeviceNameLength = 248;
const char kDeviceNameTooLong[] =
    "A device name can't be longer than 248 bytes.";
}  // namespace

static void CanonicalizeFilter(
    const BluetoothLEScanFilterInit& filter,
    mojom::blink::WebBluetoothLeScanFilterPtr& canonicalized_filter,
    ExceptionState& exception_state) {
  if (!(filter.hasServices() || filter.hasName() || filter.hasNamePrefix())) {
    exception_state.ThrowTypeError(
        "A filter must restrict the devices in some way.");
    return;
  }

  if (filter.hasServices()) {
    if (filter.services().size() == 0) {
      exception_state.ThrowTypeError(
          "'services', if present, must contain at least one service.");
      return;
    }
    canonicalized_filter->services.emplace();
    for (const StringOrUnsignedLong& service : filter.services()) {
      const String& validated_service =
          BluetoothUUID::getService(service, exception_state);
      if (exception_state.HadException())
        return;
      canonicalized_filter->services->push_back(validated_service);
    }
  }

  if (filter.hasName()) {
    size_t name_length = filter.name().Utf8().length();
    if (name_length > kMaxDeviceNameLength) {
      exception_state.ThrowTypeError(kDeviceNameTooLong);
      return;
    }
    canonicalized_filter->name = filter.name();
  }

  if (filter.hasNamePrefix()) {
    size_t name_prefix_length = filter.namePrefix().Utf8().length();
    if (name_prefix_length > kMaxDeviceNameLength) {
      exception_state.ThrowTypeError(kDeviceNameTooLong);
      return;
    }
    if (filter.namePrefix().length() == 0) {
      exception_state.ThrowTypeError(
          "'namePrefix', if present, must me non-empty.");
      return;
    }
    canonicalized_filter->name_prefix = filter.namePrefix();
  }
}

static void ConvertRequestDeviceOptions(
    const RequestDeviceOptions& options,
    mojom::blink::WebBluetoothRequestDeviceOptionsPtr& result,
    ExceptionState& exception_state) {
  if (!(options.hasFilters() ^ options.acceptAllDevices())) {
    exception_state.ThrowTypeError(
        "Either 'filters' should be present or 'acceptAllDevices' should be "
        "true, but not both.");
    return;
  }

  result->accept_all_devices = options.acceptAllDevices();

  if (options.hasFilters()) {
    if (options.filters().IsEmpty()) {
      exception_state.ThrowTypeError(
          "'filters' member must be non-empty to find any devices.");
      return;
    }

    result->filters.emplace();

    for (const BluetoothLEScanFilterInit& filter : options.filters()) {
      auto canonicalized_filter = mojom::blink::WebBluetoothLeScanFilter::New();

      CanonicalizeFilter(filter, canonicalized_filter, exception_state);

      if (exception_state.HadException())
        return;

      result->filters.value().push_back(std::move(canonicalized_filter));
    }
  }

  if (options.hasOptionalServices()) {
    for (const StringOrUnsignedLong& optional_service :
         options.optionalServices()) {
      const String& validated_optional_service =
          BluetoothUUID::getService(optional_service, exception_state);
      if (exception_state.HadException())
        return;
      result->optional_services.push_back(validated_optional_service);
    }
  }
}

void Bluetooth::RequestDeviceCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    mojom::blink::WebBluetoothDevicePtr device) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    BluetoothDevice* bluetooth_device =
        GetBluetoothDeviceRepresentingDevice(std::move(device), resolver);
    resolver->Resolve(bluetooth_device);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
ScriptPromise Bluetooth::requestDevice(ScriptState* script_state,
                                       const RequestDeviceOptions& options,
                                       ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

// Remind developers when they are using Web Bluetooth on unsupported platforms.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
  context->AddConsoleMessage(ConsoleMessage::Create(
      kJSMessageSource, kInfoMessageLevel,
      "Web Bluetooth is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/gh-pages/"
      "implementation-status.md"));
#endif

  // If the Relevant settings object is not a secure context, reject promise
  // with a SecurityError and abort these steps.
  String error_message;
  if (!context->IsSecureContext(error_message)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, error_message));
  }

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  if (!UserGestureIndicator::ConsumeUserGesture()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kSecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  if (!service_ && context->IsDocument()) {
    LocalFrame* frame = ToDocument(context)->GetFrame();
    if (frame) {
      frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service_));
    }
  }

  if (!service_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError));
  }

  // In order to convert the arguments from service names and aliases to just
  // UUIDs, do the following substeps:
  auto device_options = mojom::blink::WebBluetoothRequestDeviceOptions::New();
  ConvertRequestDeviceOptions(options, device_options, exception_state);

  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  // Record the eTLD+1 of the frame using the API.
  Document* document = ToDocument(context);
  Platform::Current()->RecordRapporURL("Bluetooth.APIUsage.Origin",
                                       document->Url());

  // Subsequent steps are handled in the browser process.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  service_->RequestDevice(std::move(device_options),
                          ConvertToBaseCallback(WTF::Bind(
                              &Bluetooth::RequestDeviceCallback,
                              WrapPersistent(this), WrapPersistent(resolver))));
  return promise;
}

void Bluetooth::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_instance_map_);
}

Bluetooth::Bluetooth() {}

BluetoothDevice* Bluetooth::GetBluetoothDeviceRepresentingDevice(
    mojom::blink::WebBluetoothDevicePtr device_ptr,
    ScriptPromiseResolver* resolver) {
  WTF::String id = device_ptr->id;
  BluetoothDevice* device = device_instance_map_.at(id);
  if (!device) {
    device = BluetoothDevice::Take(resolver, std::move(device_ptr), this);
    auto result = device_instance_map_.insert(id, device);
    DCHECK(result.is_new_entry);
  }
  return device;
}

}  // namespace blink

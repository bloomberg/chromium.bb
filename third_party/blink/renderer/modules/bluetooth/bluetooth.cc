// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"

#include <memory>
#include <utility>
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event_init.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan_options.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/modules/bluetooth/request_device_options.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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
    const BluetoothLEScanFilterInit* filter,
    mojom::blink::WebBluetoothLeScanFilterPtr& canonicalized_filter,
    ExceptionState& exception_state) {
  if (!(filter->hasServices() || filter->hasName() ||
        filter->hasNamePrefix())) {
    exception_state.ThrowTypeError(
        "A filter must restrict the devices in some way.");
    return;
  }

  if (filter->hasServices()) {
    if (filter->services().size() == 0) {
      exception_state.ThrowTypeError(
          "'services', if present, must contain at least one service.");
      return;
    }
    canonicalized_filter->services.emplace();
    for (const StringOrUnsignedLong& service : filter->services()) {
      const String& validated_service =
          BluetoothUUID::getService(service, exception_state);
      if (exception_state.HadException())
        return;
      canonicalized_filter->services->push_back(validated_service);
    }
  }

  if (filter->hasName()) {
    size_t name_length = filter->name().Utf8().length();
    if (name_length > kMaxDeviceNameLength) {
      exception_state.ThrowTypeError(kDeviceNameTooLong);
      return;
    }
    canonicalized_filter->name = filter->name();
  }

  if (filter->hasNamePrefix()) {
    size_t name_prefix_length = filter->namePrefix().Utf8().length();
    if (name_prefix_length > kMaxDeviceNameLength) {
      exception_state.ThrowTypeError(kDeviceNameTooLong);
      return;
    }
    if (filter->namePrefix().length() == 0) {
      exception_state.ThrowTypeError(
          "'namePrefix', if present, must me non-empty.");
      return;
    }
    canonicalized_filter->name_prefix = filter->namePrefix();
  }
}

static void ConvertRequestDeviceOptions(
    const RequestDeviceOptions* options,
    mojom::blink::WebBluetoothRequestDeviceOptionsPtr& result,
    ExceptionState& exception_state) {
  if (!(options->hasFilters() ^ options->acceptAllDevices())) {
    exception_state.ThrowTypeError(
        "Either 'filters' should be present or 'acceptAllDevices' should be "
        "true, but not both.");
    return;
  }

  result->accept_all_devices = options->acceptAllDevices();

  if (options->hasFilters()) {
    if (options->filters().IsEmpty()) {
      exception_state.ThrowTypeError(
          "'filters' member must be non-empty to find any devices.");
      return;
    }

    result->filters.emplace();

    for (const BluetoothLEScanFilterInit* filter : options->filters()) {
      auto canonicalized_filter = mojom::blink::WebBluetoothLeScanFilter::New();

      CanonicalizeFilter(filter, canonicalized_filter, exception_state);

      if (exception_state.HadException())
        return;

      result->filters->push_back(std::move(canonicalized_filter));
    }
  }

  if (options->hasOptionalServices()) {
    for (const StringOrUnsignedLong& optional_service :
         options->optionalServices()) {
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
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    BluetoothDevice* bluetooth_device = GetBluetoothDeviceRepresentingDevice(
        std::move(device), resolver->GetExecutionContext());
    resolver->Resolve(bluetooth_device);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

// https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
ScriptPromise Bluetooth::requestDevice(ScriptState* script_state,
                                       const RequestDeviceOptions* options,
                                       ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

// Remind developers when they are using Web Bluetooth on unsupported platforms.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_MACOSX) && \
    !defined(OS_WIN)
  context->AddConsoleMessage(ConsoleMessage::Create(
      kJSMessageSource, kInfoMessageLevel,
      "Web Bluetooth is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/gh-pages/"
      "implementation-status.md"));
#endif

  CHECK(context->IsSecureContext());

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  auto& doc = *To<Document>(context);
  auto* frame = doc.GetFrame();
  if (!frame) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), "Document not active"));
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kSecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  if (!service_) {
      // See https://bit.ly/2S0zRAS for task types.
      frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(
          &service_, context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  // In order to convert the arguments from service names and aliases to just
  // UUIDs, do the following substeps:
  auto device_options = mojom::blink::WebBluetoothRequestDeviceOptions::New();
  ConvertRequestDeviceOptions(options, device_options, exception_state);

  if (exception_state.HadException())
    return ScriptPromise();

  // Record the eTLD+1 of the frame using the API.
  Platform::Current()->RecordRapporURL("Bluetooth.APIUsage.Origin", doc.Url());

  // Subsequent steps are handled in the browser process.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  service_->RequestDevice(
      std::move(device_options),
      WTF::Bind(&Bluetooth::RequestDeviceCallback, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

static void ConvertRequestLEScanOptions(
    const BluetoothLEScanOptions* options,
    mojom::blink::WebBluetoothRequestLEScanOptionsPtr& result,
    ExceptionState& exception_state) {
  if (!(options->hasFilters() ^ options->acceptAllAdvertisements())) {
    exception_state.ThrowTypeError(
        "Either 'filters' should be present or 'acceptAllAdvertisements' "
        "should be true, but not both.");
    return;
  }

  result->accept_all_advertisements = options->acceptAllAdvertisements();
  result->keep_repeated_devices = options->keepRepeatedDevices();

  if (options->hasFilters()) {
    if (options->filters().IsEmpty()) {
      exception_state.ThrowTypeError(
          "'filters' member must be non-empty to find any devices.");
      return;
    }

    result->filters.emplace();

    for (const BluetoothLEScanFilterInit* filter : options->filters()) {
      auto canonicalized_filter = mojom::blink::WebBluetoothLeScanFilter::New();

      CanonicalizeFilter(filter, canonicalized_filter, exception_state);

      if (exception_state.HadException())
        return;

      result->filters->push_back(std::move(canonicalized_filter));
    }
  }
}

void Bluetooth::RequestScanningCallback(
    ScriptPromiseResolver* resolver,
    mojo::BindingId id,
    mojom::blink::RequestScanningStartResultPtr result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (result->is_error_result()) {
    resolver->Reject(
        BluetoothError::CreateDOMException(result->get_error_result()));
    return;
  }

  auto* scan =
      BluetoothLEScan::Create(id, this, std::move(result->get_options()));
  resolver->Resolve(scan);
}

// https://webbluetoothcg.github.io/web-bluetooth/scanning.html#dom-bluetooth-requestlescan
ScriptPromise Bluetooth::requestLEScan(ScriptState* script_state,
                                       const BluetoothLEScanOptions* options,
                                       ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);

  // Remind developers when they are using Web Bluetooth on unsupported
  // platforms.
  context->AddConsoleMessage(ConsoleMessage::Create(
      kJSMessageSource, kInfoMessageLevel,
      "Web Bluetooth Scanning is experimental on this platform. See "
      "https://github.com/WebBluetoothCG/web-bluetooth/blob/gh-pages/"
      "implementation-status.md"));

  CHECK(context->IsSecureContext());

  // If the algorithm is not allowed to show a popup, reject promise with a
  // SecurityError and abort these steps.
  auto& doc = *To<Document>(context);
  auto* frame = doc.GetFrame();
  if (!frame) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), "Document not active"));
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kSecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  if (!service_) {
    // See https://bit.ly/2S0zRAS for task types.
    frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(
        &service_, context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  auto scan_options = mojom::blink::WebBluetoothRequestLEScanOptions::New();
  ConvertRequestLEScanOptions(options, scan_options, exception_state);

  if (exception_state.HadException())
    return ScriptPromise();

  // Record the eTLD+1 of the frame using the API.
  Platform::Current()->RecordRapporURL("Bluetooth.APIUsage.Origin", doc.Url());

  // Subsequent steps are handled in the browser process.
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::WebBluetoothScanClientAssociatedPtrInfo client;
  mojo::BindingId id =
      client_bindings_.AddBinding(this, mojo::MakeRequest(&client));

  service_->RequestScanningStart(
      std::move(client), std::move(scan_options),
      WTF::Bind(&Bluetooth::RequestScanningCallback, WrapPersistent(this),
                WrapPersistent(resolver), id));

  return promise;
}

void Bluetooth::ScanEvent(mojom::blink::WebBluetoothScanResultPtr result) {
  ExecutionContext* context = ContextLifecycleObserver::GetExecutionContext();
  DCHECK(context);

  BluetoothDevice* bluetooth_device =
      GetBluetoothDeviceRepresentingDevice(std::move(result->device), context);

  HeapVector<blink::StringOrUnsignedLong> uuids;
  for (const String& uuid : result->uuids) {
    StringOrUnsignedLong value;
    value.SetString(uuid);
    uuids.push_back(value);
  }

  auto* manufacturer_data = MakeGarbageCollected<BluetoothManufacturerDataMap>(
      result->manufacturer_data);
  auto* service_data =
      MakeGarbageCollected<BluetoothServiceDataMap>(result->service_data);

  base::Optional<int8_t> rssi;
  if (result->rssi_is_set)
    rssi = result->rssi;

  base::Optional<int8_t> tx_power;
  if (result->tx_power_is_set)
    tx_power = result->tx_power;

  base::Optional<int16_t> appearance;
  if (result->appearance_is_set)
    appearance = result->appearance;

  auto* event = BluetoothAdvertisingEvent::Create(
      event_type_names::kAdvertisementreceived, bluetooth_device, result->name,
      uuids, appearance, tx_power, rssi, manufacturer_data, service_data);
  DispatchEvent(*event);
}

void Bluetooth::CancelScan(mojo::BindingId id) {
  client_bindings_.RemoveBinding(id);
}

const WTF::AtomicString& Bluetooth::InterfaceName() const {
  return event_type_names::kAdvertisementreceived;
}

ExecutionContext* Bluetooth::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void Bluetooth::ContextDestroyed(ExecutionContext*) {
  client_bindings_.CloseAllBindings();
}

void Bluetooth::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_instance_map_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

Bluetooth::Bluetooth(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

Bluetooth::~Bluetooth() {
  DCHECK(client_bindings_.empty());
}

BluetoothDevice* Bluetooth::GetBluetoothDeviceRepresentingDevice(
    mojom::blink::WebBluetoothDevicePtr device_ptr,
    ExecutionContext* context) {
  String& id = device_ptr->id;
  BluetoothDevice* device = device_instance_map_.at(id);
  if (!device) {
    device = MakeGarbageCollected<BluetoothDevice>(context,
                                                   std::move(device_ptr), this);
    auto result = device_instance_map_.insert(id, device);
    DCHECK(result.is_new_entry);
  }
  return device;
}

}  // namespace blink

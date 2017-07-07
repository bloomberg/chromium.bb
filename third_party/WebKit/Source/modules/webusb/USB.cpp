// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USB.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/UserGestureIndicator.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "modules/EventTargetModules.h"
#include "modules/webusb/USBConnectionEvent.h"
#include "modules/webusb/USBDevice.h"
#include "modules/webusb/USBDeviceFilter.h"
#include "modules/webusb/USBDeviceRequestOptions.h"
#include "platform/mojo/MojoHelper.h"
#include "platform/wtf/Functional.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "services/service_manager/public/cpp/interface_provider.h"

using device::mojom::blink::UsbDeviceFilterPtr;
using device::mojom::blink::UsbDeviceInfoPtr;
using device::mojom::blink::UsbDevicePtr;

namespace blink {
namespace {

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"usb\" is disallowed by feature policy.";
const char kIframeBlocked[] =
    "Access to this method is not allowed in embedded frames.";
const char kNoDeviceSelected[] = "No device selected.";

UsbDeviceFilterPtr ConvertDeviceFilter(const USBDeviceFilter& filter) {
  auto mojo_filter = device::mojom::blink::UsbDeviceFilter::New();
  mojo_filter->has_vendor_id = filter.hasVendorId();
  if (mojo_filter->has_vendor_id)
    mojo_filter->vendor_id = filter.vendorId();
  mojo_filter->has_product_id = filter.hasProductId();
  if (mojo_filter->has_product_id)
    mojo_filter->product_id = filter.productId();
  mojo_filter->has_class_code = filter.hasClassCode();
  if (mojo_filter->has_class_code)
    mojo_filter->class_code = filter.classCode();
  mojo_filter->has_subclass_code = filter.hasSubclassCode();
  if (mojo_filter->has_subclass_code)
    mojo_filter->subclass_code = filter.subclassCode();
  mojo_filter->has_protocol_code = filter.hasProtocolCode();
  if (mojo_filter->has_protocol_code)
    mojo_filter->protocol_code = filter.protocolCode();
  if (filter.hasSerialNumber())
    mojo_filter->serial_number = filter.serialNumber();
  return mojo_filter;
}

}  // namespace

USB::USB(LocalFrame& frame)
    : ContextLifecycleObserver(frame.GetDocument()), client_binding_(this) {}

USB::~USB() {
  // |m_deviceManager| and |m_chooserService| may still be valid but there
  // should be no more outstanding requests to them because each holds a
  // persistent handle to this object.
  DCHECK(device_manager_requests_.IsEmpty());
  DCHECK(chooser_service_requests_.IsEmpty());
}

void USB::Dispose() {
  // The pipe to this object must be closed when is marked unreachable to
  // prevent messages from being dispatched before lazy sweeping.
  client_binding_.Close();
}

ScriptPromise USB::getDevices(ScriptState* script_state) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError));
  }

  if (RuntimeEnabledFeatures::FeaturePolicyEnabled()) {
    if (!frame->IsFeatureEnabled(WebFeaturePolicyFeature::kUsb)) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kSecurityError, kFeaturePolicyBlocked));
    }
  } else if (!frame->IsMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, kIframeBlocked));
  }

  EnsureDeviceManagerConnection();
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  device_manager_requests_.insert(resolver);
  device_manager_->GetDevices(
      nullptr,
      ConvertToBaseCallback(WTF::Bind(&USB::OnGetDevices, WrapPersistent(this),
                                      WrapPersistent(resolver))));
  return resolver->Promise();
}

ScriptPromise USB::requestDevice(ScriptState* script_state,
                                 const USBDeviceRequestOptions& options) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError));
  }

  if (RuntimeEnabledFeatures::FeaturePolicyEnabled()) {
    if (!frame->IsFeatureEnabled(WebFeaturePolicyFeature::kUsb)) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kSecurityError, kFeaturePolicyBlocked));
    }
  } else if (!frame->IsMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, kIframeBlocked));
  }

  if (!chooser_service_) {
    GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&chooser_service_));
    chooser_service_.set_connection_error_handler(
        ConvertToBaseCallback(WTF::Bind(&USB::OnChooserServiceConnectionError,
                                        WrapWeakPersistent(this))));
  }

  if (!UserGestureIndicator::ConsumeUserGesture()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kSecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  Vector<UsbDeviceFilterPtr> filters;
  if (options.hasFilters()) {
    filters.ReserveCapacity(options.filters().size());
    for (const auto& filter : options.filters())
      filters.push_back(ConvertDeviceFilter(filter));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  chooser_service_requests_.insert(resolver);
  chooser_service_->GetPermission(
      std::move(filters), ConvertToBaseCallback(WTF::Bind(
                              &USB::OnGetPermission, WrapPersistent(this),
                              WrapPersistent(resolver))));
  return resolver->Promise();
}

ExecutionContext* USB::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& USB::InterfaceName() const {
  return EventTargetNames::USB;
}

void USB::ContextDestroyed(ExecutionContext*) {
  device_manager_.reset();
  device_manager_requests_.clear();
  chooser_service_.reset();
  chooser_service_requests_.clear();
}

USBDevice* USB::GetOrCreateDevice(UsbDeviceInfoPtr device_info) {
  USBDevice* device = device_cache_.at(device_info->guid);
  if (!device) {
    String guid = device_info->guid;
    UsbDevicePtr pipe;
    device_manager_->GetDevice(guid, mojo::MakeRequest(&pipe));
    device = USBDevice::Create(std::move(device_info), std::move(pipe),
                               GetExecutionContext());
    device_cache_.insert(guid, device);
  }
  return device;
}

void USB::OnGetDevices(ScriptPromiseResolver* resolver,
                       Vector<UsbDeviceInfoPtr> device_infos) {
  auto request_entry = device_manager_requests_.find(resolver);
  if (request_entry == device_manager_requests_.end())
    return;
  device_manager_requests_.erase(request_entry);

  HeapVector<Member<USBDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));
  resolver->Resolve(devices);
  device_manager_requests_.erase(resolver);
}

void USB::OnGetPermission(ScriptPromiseResolver* resolver,
                          UsbDeviceInfoPtr device_info) {
  auto request_entry = chooser_service_requests_.find(resolver);
  if (request_entry == chooser_service_requests_.end())
    return;
  chooser_service_requests_.erase(request_entry);

  EnsureDeviceManagerConnection();

  if (device_manager_ && device_info)
    resolver->Resolve(GetOrCreateDevice(std::move(device_info)));
  else
    resolver->Reject(DOMException::Create(kNotFoundError, kNoDeviceSelected));
}

void USB::OnDeviceAdded(UsbDeviceInfoPtr device_info) {
  if (!device_manager_)
    return;

  DispatchEvent(USBConnectionEvent::Create(
      EventTypeNames::connect, GetOrCreateDevice(std::move(device_info))));
}

void USB::OnDeviceRemoved(UsbDeviceInfoPtr device_info) {
  String guid = device_info->guid;
  USBDevice* device = device_cache_.at(guid);
  if (!device) {
    device = USBDevice::Create(std::move(device_info), nullptr,
                               GetExecutionContext());
  }
  DispatchEvent(USBConnectionEvent::Create(EventTypeNames::disconnect, device));
  device_cache_.erase(guid);
}

void USB::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_binding_.Close();
  for (ScriptPromiseResolver* resolver : device_manager_requests_)
    resolver->Resolve(HeapVector<Member<USBDevice>>(0));
  device_manager_requests_.clear();
}

void USB::OnChooserServiceConnectionError() {
  chooser_service_.reset();
  for (ScriptPromiseResolver* resolver : chooser_service_requests_)
    resolver->Reject(DOMException::Create(kNotFoundError, kNoDeviceSelected));
  chooser_service_requests_.clear();
}

void USB::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);
  if (event_type != EventTypeNames::connect &&
      event_type != EventTypeNames::disconnect) {
    return;
  }

  LocalFrame* frame = GetFrame();
  if (!frame)
    return;

  if (RuntimeEnabledFeatures::FeaturePolicyEnabled()) {
    if (frame->IsFeatureEnabled(WebFeaturePolicyFeature::kUsb))
      EnsureDeviceManagerConnection();
  } else if (frame->IsMainFrame()) {
    EnsureDeviceManagerConnection();
  }
}

void USB::EnsureDeviceManagerConnection() {
  if (device_manager_)
    return;

  DCHECK(GetFrame());
  GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&device_manager_));
  device_manager_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &USB::OnDeviceManagerConnectionError, WrapWeakPersistent(this))));

  DCHECK(!client_binding_.is_bound());

  device::mojom::blink::UsbDeviceManagerClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  device_manager_->SetClient(std::move(client));
}

DEFINE_TRACE(USB) {
  visitor->Trace(device_manager_requests_);
  visitor->Trace(chooser_service_requests_);
  visitor->Trace(device_cache_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink

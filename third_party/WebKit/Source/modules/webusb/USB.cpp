// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USB.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "device/usb/public/interfaces/device.mojom-wtf.h"
#include "modules/EventTargetModules.h"
#include "modules/webusb/USBConnectionEvent.h"
#include "modules/webusb/USBDevice.h"
#include "modules/webusb/USBDeviceFilter.h"
#include "modules/webusb/USBDeviceRequestOptions.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/ServiceRegistry.h"
#include "wtf/Functional.h"

namespace usb = device::usb::wtf;

namespace blink {
namespace {

const char kNoServiceError[] = "USB service unavailable.";

usb::DeviceFilterPtr convertDeviceFilter(const USBDeviceFilter& filter)
{
    auto mojoFilter = usb::DeviceFilter::New();
    mojoFilter->has_vendor_id = filter.hasVendorId();
    if (mojoFilter->has_vendor_id)
        mojoFilter->vendor_id = filter.vendorId();
    mojoFilter->has_product_id = filter.hasProductId();
    if (mojoFilter->has_product_id)
        mojoFilter->product_id = filter.productId();
    mojoFilter->has_class_code = filter.hasClassCode();
    if (mojoFilter->has_class_code)
        mojoFilter->class_code = filter.classCode();
    mojoFilter->has_subclass_code = filter.hasSubclassCode();
    if (mojoFilter->has_subclass_code)
        mojoFilter->subclass_code = filter.subclassCode();
    mojoFilter->has_protocol_code = filter.hasProtocolCode();
    if (mojoFilter->has_protocol_code)
        mojoFilter->protocol_code = filter.protocolCode();
    return mojoFilter;
}

bool isActive(ScriptPromiseResolver* resolver)
{
    ExecutionContext* context = resolver->getExecutionContext();
    return context && !context->activeDOMObjectsAreStopped();
}

} // namespace

USB::USB(LocalFrame& frame)
    : ContextLifecycleObserver(frame.document())
{
    frame.serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_deviceManager));
    m_deviceManager.set_connection_error_handler([this]() {
        m_deviceManager.reset();
        for (ScriptPromiseResolver* resolver : m_deviceManagerRequests) {
            if (isActive(resolver))
                resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
        }
        m_deviceManagerRequests.clear();
    });
    // Set up two sequential calls to GetDeviceChanges to avoid latency.
    m_deviceManager->GetDeviceChanges(createBaseCallback(bind<usb::DeviceChangeNotificationPtr>(&USB::onDeviceChanges, this)));
    m_deviceManager->GetDeviceChanges(createBaseCallback(bind<usb::DeviceChangeNotificationPtr>(&USB::onDeviceChanges, this)));
}

USB::~USB()
{
    DCHECK(!m_deviceManager);
    DCHECK(m_deviceManagerRequests.isEmpty());
    DCHECK(!m_chooserService);
    DCHECK(m_chooserServiceRequests.isEmpty());
}

ScriptPromise USB::getDevices(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!m_deviceManager) {
        resolver->reject(DOMException::create(NotSupportedError));
    } else {
        String errorMessage;
        if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
            resolver->reject(DOMException::create(SecurityError, errorMessage));
        } else {
            m_deviceManagerRequests.add(resolver);
            m_deviceManager->GetDevices(nullptr, createBaseCallback(bind<mojo::WTFArray<usb::DeviceInfoPtr>>(&USB::onGetDevices, this, resolver)));
        }
    }
    return promise;
}

ScriptPromise USB::requestDevice(ScriptState* scriptState, const USBDeviceRequestOptions& options)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_chooserService) {
        LocalFrame* frame = getExecutionContext() ? toDocument(getExecutionContext())->frame() : nullptr;
        if (!frame) {
            resolver->reject(DOMException::create(NotSupportedError));
            return promise;
        }
        frame->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_chooserService));
        m_chooserService.set_connection_error_handler([this]() {
            m_chooserService.reset();
            for (ScriptPromiseResolver* resolver : m_chooserServiceRequests) {
                if (isActive(resolver))
                    resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
            }
            m_chooserServiceRequests.clear();
        });
    }

    String errorMessage;
    if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
        resolver->reject(DOMException::create(SecurityError, errorMessage));
    } else if (!UserGestureIndicator::consumeUserGesture()) {
        resolver->reject(DOMException::create(SecurityError, "Must be handling a user gesture to show a permission request."));
    } else {
        Vector<usb::DeviceFilterPtr> filters;
        if (options.hasFilters()) {
            filters.reserveCapacity(options.filters().size());
            for (const auto& filter : options.filters())
                filters.append(convertDeviceFilter(filter));
        }
        m_chooserServiceRequests.add(resolver);
        m_chooserService->GetPermission(std::move(filters), createBaseCallback(bind<usb::DeviceInfoPtr>(&USB::onGetPermission, this, resolver)));
    }
    return promise;
}

ExecutionContext* USB::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

const AtomicString& USB::interfaceName() const
{
    return EventTargetNames::USB;
}

void USB::contextDestroyed()
{
    m_deviceManager.reset();
    m_deviceManagerRequests.clear();
    m_chooserService.reset();
    m_chooserServiceRequests.clear();
}

void USB::onGetDevices(ScriptPromiseResolver* resolver, mojo::WTFArray<usb::DeviceInfoPtr> deviceInfos)
{
    if (!isActive(resolver))
        return;

    HeapVector<Member<USBDevice>> devices;
    for (auto& deviceInfo : deviceInfos.PassStorage()) {
        usb::DevicePtr device;
        m_deviceManager->GetDevice(deviceInfo->guid, mojo::GetProxy(&device));
        devices.append(USBDevice::create(std::move(deviceInfo), std::move(device), resolver->getExecutionContext()));
    }
    resolver->resolve(devices);
    m_deviceManagerRequests.remove(resolver);
}

void USB::onGetPermission(ScriptPromiseResolver* resolver, usb::DeviceInfoPtr deviceInfo)
{
    if (!isActive(resolver))
        return;

    if (deviceInfo) {
        usb::DevicePtr device;
        m_deviceManager->GetDevice(deviceInfo->guid, mojo::GetProxy(&device));
        resolver->resolve(USBDevice::create(std::move(deviceInfo), std::move(device), resolver->getExecutionContext()));
    } else {
        resolver->reject(DOMException::create(NotFoundError, "No device selected."));
    }
}

void USB::onDeviceChanges(usb::DeviceChangeNotificationPtr notification)
{
    m_deviceManager->GetDeviceChanges(createBaseCallback(bind<usb::DeviceChangeNotificationPtr>(&USB::onDeviceChanges, this)));
    for (auto& deviceInfo : notification->devices_added.PassStorage()) {
        usb::DevicePtr device;
        m_deviceManager->GetDevice(deviceInfo->guid, mojo::GetProxy(&device));
        dispatchEvent(USBConnectionEvent::create(EventTypeNames::connect, USBDevice::create(std::move(deviceInfo), std::move(device), getExecutionContext())));
    }
    for (auto& deviceInfo : notification->devices_removed.PassStorage())
        dispatchEvent(USBConnectionEvent::create(EventTypeNames::disconnect, USBDevice::create(std::move(deviceInfo), nullptr, getExecutionContext())));
}

DEFINE_TRACE(USB)
{
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
    visitor->trace(m_deviceManagerRequests);
    visitor->trace(m_chooserServiceRequests);
}

} // namespace blink

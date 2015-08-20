// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webusb/USB.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/EventTargetModules.h"
#include "modules/webusb/USBDevice.h"
#include "modules/webusb/USBDeviceFilter.h"
#include "modules/webusb/USBDeviceRequestOptions.h"
#include "modules/webusb/USBError.h"
#include "public/platform/Platform.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/webusb/WebUSBClient.h"
#include "public/platform/modules/webusb/WebUSBDeviceFilter.h"
#include "public/platform/modules/webusb/WebUSBDeviceRequestOptions.h"
#include "public/platform/modules/webusb/WebUSBError.h"

namespace blink {
namespace {

void convertDeviceFilter(const USBDeviceFilter& filter, WebUSBDeviceFilter* webFilter)
{
    webFilter->hasVendorID = filter.hasVendorId();
    if (filter.hasVendorId())
        webFilter->vendorID = filter.vendorId();
    webFilter->hasProductID = filter.hasProductId();
    if (filter.hasProductId())
        webFilter->productID = filter.productId();
    webFilter->hasClassCode = filter.hasClassCode();
    if (filter.hasClassCode())
        webFilter->classCode = filter.classCode();
    webFilter->hasSubclassCode = filter.hasSubclassCode();
    if (filter.hasSubclassCode())
        webFilter->subclassCode = filter.subclassCode();
    webFilter->hasProtocolCode = filter.hasProtocolCode();
    if (filter.hasProtocolCode())
        webFilter->protocolCode = filter.protocolCode();
}

void convertDeviceRequestOptions(const USBDeviceRequestOptions& options, WebUSBDeviceRequestOptions* webOptions)
{
    ASSERT(options.hasFilters());
    webOptions->filters = WebVector<WebUSBDeviceFilter>(options.filters().size());
    for (size_t i = 0; i < options.filters().size(); ++i) {
        convertDeviceFilter(options.filters()[i], &webOptions->filters[i]);
    }
}

// Allows using a CallbackPromiseAdapter with a WebVector to resolve the
// getDevices() promise with a HeapVector owning USBDevices.
class DeviceArray {
    WTF_MAKE_NONCOPYABLE(DeviceArray);
public:
    using WebType = OwnPtr<WebVector<WebUSBDevice*>>;

    static HeapVector<Member<USBDevice>> take(ScriptPromiseResolver*, PassOwnPtr<WebVector<WebUSBDevice*>> webDevices)
    {
        HeapVector<Member<USBDevice>> devices;
        for (const auto webDevice : *webDevices)
            devices.append(USBDevice::create(adoptPtr(webDevice)));
        return devices;
    }

private:
    DeviceArray() = delete;
};

} // namespace

USB::USB(LocalFrame& frame)
    : m_controller(USBController::from(frame))
{
}

ScriptPromise USB::getDevices(ScriptState* scriptState)
{
    WebUSBClient* client = m_controller->client();
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    client->getDevices(new CallbackPromiseAdapter<DeviceArray, USBError>(resolver));

    return promise;
}

ScriptPromise USB::requestDevice(ScriptState* scriptState, const USBDeviceRequestOptions& options)
{
    WebUSBClient* client = m_controller->client();
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebUSBDeviceRequestOptions webOptions;
    convertDeviceRequestOptions(options, &webOptions);
    client->requestDevice(webOptions, new CallbackPromiseAdapter<USBDevice, USBError>(resolver));

    return promise;
}

ExecutionContext* USB::executionContext() const
{
    LocalFrame* frame = m_controller->frame();
    return frame ? frame->document() : nullptr;
}

const AtomicString& USB::interfaceName() const
{
    return EventTargetNames::USB;
}

DEFINE_TRACE(USB)
{
    visitor->trace(m_controller);
    RefCountedGarbageCollectedEventTargetWithInlineData<USB>::trace(visitor);
}

} // namespace blink

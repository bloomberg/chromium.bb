// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBDevice.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ToV8.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webusb/USBConfiguration.h"
#include "modules/webusb/USBControlTransferParameters.h"
#include "modules/webusb/USBError.h"
#include "modules/webusb/USBInTransferResult.h"
#include "modules/webusb/USBIsochronousInTransferResult.h"
#include "modules/webusb/USBIsochronousOutTransferResult.h"
#include "modules/webusb/USBOutTransferResult.h"
#include "public/platform/modules/webusb/WebUSBTransferInfo.h"
#include "wtf/Assertions.h"

namespace blink {

namespace {

const char kDeviceStateChangeInProgress[] = "An operation that changes the device state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";

DOMException* convertControlTransferParameters(
    WebUSBDevice::TransferDirection direction,
    const USBControlTransferParameters& parameters,
    WebUSBDevice::ControlTransferParameters* webParameters)
{
    webParameters->direction = direction;

    if (parameters.requestType() == "standard")
        webParameters->type = WebUSBDevice::RequestType::Standard;
    else if (parameters.requestType() == "class")
        webParameters->type = WebUSBDevice::RequestType::Class;
    else if (parameters.requestType() == "vendor")
        webParameters->type = WebUSBDevice::RequestType::Vendor;
    else
        return DOMException::create(TypeMismatchError, "The control transfer requestType parameter is invalid.");

    if (parameters.recipient() == "device")
        webParameters->recipient = WebUSBDevice::RequestRecipient::Device;
    else if (parameters.recipient() == "interface")
        webParameters->recipient = WebUSBDevice::RequestRecipient::Interface;
    else if (parameters.recipient() == "endpoint")
        webParameters->recipient = WebUSBDevice::RequestRecipient::Endpoint;
    else if (parameters.recipient() == "other")
        webParameters->recipient = WebUSBDevice::RequestRecipient::Other;
    else
        return DOMException::create(TypeMismatchError, "The control transfer recipient parameter is invalid.");

    webParameters->request = parameters.request();
    webParameters->value = parameters.value();
    webParameters->index = parameters.index();
    return nullptr;
}

String convertTransferStatus(const WebUSBTransferInfo::Status& status)
{
    switch (status) {
    case WebUSBTransferInfo::Status::Ok:
        return "ok";
    case WebUSBTransferInfo::Status::Stall:
        return "stall";
    case WebUSBTransferInfo::Status::Babble:
        return "babble";
    default:
        ASSERT_NOT_REACHED();
        return "";
    }
}

class OpenClosePromiseAdapter : public WebCallbacks<void, const WebUSBError&> {
public:
    OpenClosePromiseAdapter(USBDevice* device, ScriptPromiseResolver* resolver, bool desiredState)
        : m_device(device)
        , m_resolver(resolver)
        , m_desiredState(desiredState)
    {
    }

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        if (m_device)
            m_device->onDeviceOpenedOrClosed(m_desiredState);
        m_resolver->resolve();
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        if (m_device)
            m_device->onDeviceOpenedOrClosed(!m_desiredState);
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    WeakPersistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
    bool m_desiredState; // true: open, false: closed
};

class SelectConfigurationPromiseAdapter : public WebCallbacks<void, const WebUSBError&> {
public:
    SelectConfigurationPromiseAdapter(USBDevice* device, ScriptPromiseResolver* resolver, int configurationIndex)
        : m_device(device)
        , m_resolver(resolver)
        , m_configurationIndex(configurationIndex)
    {
    }

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        if (m_device)
            m_device->onConfigurationSelected(true /* success */, m_configurationIndex);
        m_resolver->resolve();
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        if (m_device)
            m_device->onConfigurationSelected(false /* failure */, m_configurationIndex);
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    WeakPersistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
    int m_configurationIndex;
};

class InputTransferResult {
    WTF_MAKE_NONCOPYABLE(InputTransferResult);
public:
    using WebType = OwnPtr<WebUSBTransferInfo>;

    static USBInTransferResult* take(ScriptPromiseResolver*, PassOwnPtr<WebUSBTransferInfo> webTransferInfo)
    {
        ASSERT(webTransferInfo->status.size() == 1);
        return USBInTransferResult::create(convertTransferStatus(webTransferInfo->status[0]), webTransferInfo->data);
    }

private:
    InputTransferResult() = delete;
};

class OutputTransferResult {
    WTF_MAKE_NONCOPYABLE(OutputTransferResult);
public:
    using WebType = OwnPtr<WebUSBTransferInfo>;

    static USBOutTransferResult* take(ScriptPromiseResolver*, PassOwnPtr<WebUSBTransferInfo> webTransferInfo)
    {
        ASSERT(webTransferInfo->status.size() == 1);
        ASSERT(webTransferInfo->bytesTransferred.size() == 1);
        return USBOutTransferResult::create(convertTransferStatus(webTransferInfo->status[0]), webTransferInfo->bytesTransferred[0]);
    }

private:
    OutputTransferResult() = delete;
};

class IsochronousInputTransferResult {
    WTF_MAKE_NONCOPYABLE(IsochronousInputTransferResult);

public:
    using WebType = OwnPtr<WebUSBTransferInfo>;

    static USBIsochronousInTransferResult* take(ScriptPromiseResolver*, PassOwnPtr<WebUSBTransferInfo> webTransferInfo)
    {
        ASSERT(webTransferInfo->status.size() == webTransferInfo->packetLength.size() && webTransferInfo->packetLength.size() == webTransferInfo->bytesTransferred.size());
        RefPtr<DOMArrayBuffer> buffer = DOMArrayBuffer::create(webTransferInfo->data.data(), webTransferInfo->data.size());
        HeapVector<Member<USBIsochronousInTransferPacket>> packets(webTransferInfo->status.size());
        size_t byteOffset = 0;
        for (size_t i = 0; i < webTransferInfo->status.size(); ++i) {
            packets[i] = USBIsochronousInTransferPacket::create(convertTransferStatus(webTransferInfo->status[i]), DOMDataView::create(buffer, byteOffset, webTransferInfo->bytesTransferred[i]));
            byteOffset += webTransferInfo->packetLength[i];
        }
        return USBIsochronousInTransferResult::create(buffer, packets);
    }
};

class IsochronousOutputTransferResult {
    WTF_MAKE_NONCOPYABLE(IsochronousOutputTransferResult);

public:
    using WebType = OwnPtr<WebUSBTransferInfo>;

    static USBIsochronousOutTransferResult* take(ScriptPromiseResolver*, PassOwnPtr<WebUSBTransferInfo> webTransferInfo)
    {
        ASSERT(webTransferInfo->status.size() == webTransferInfo->bytesTransferred.size());
        HeapVector<Member<USBIsochronousOutTransferPacket>> packets(webTransferInfo->status.size());
        for (size_t i = 0; i < webTransferInfo->status.size(); ++i)
            packets[i] = USBIsochronousOutTransferPacket::create(convertTransferStatus(webTransferInfo->status[i]), webTransferInfo->bytesTransferred[i]);
        return USBIsochronousOutTransferResult::create(packets);
    }
};

class BufferSource {
    WTF_MAKE_NONCOPYABLE(BufferSource);
public:
    BufferSource(const ArrayBufferOrArrayBufferView& buffer) : m_buffer(buffer)
    {
        ASSERT(!m_buffer.isNull());
    }

    uint8_t* data() const
    {
        if (m_buffer.isArrayBuffer())
            return static_cast<uint8_t*>(m_buffer.getAsArrayBuffer()->data());
        return static_cast<uint8_t*>(m_buffer.getAsArrayBufferView()->baseAddress());
    }

    unsigned size() const
    {
        if (m_buffer.isArrayBuffer())
            return m_buffer.getAsArrayBuffer()->byteLength();
        return m_buffer.getAsArrayBufferView()->byteLength();
    }

private:
    const ArrayBufferOrArrayBufferView& m_buffer;
};

} // namespace

// static
USBDevice* USBDevice::take(ScriptPromiseResolver* resolver, PassOwnPtr<WebUSBDevice> device)
{
    return USBDevice::create(device, resolver->getExecutionContext());
}

USBDevice::USBDevice(PassOwnPtr<WebUSBDevice> device, ExecutionContext* context)
    : ContextLifecycleObserver(context)
    , m_device(device)
    , m_opened(false)
    , m_deviceStateChangeInProgress(false)
{
    m_configurationIndex = findConfigurationIndex(info().activeConfiguration);
}

void USBDevice::onDeviceOpenedOrClosed(bool opened)
{
    m_opened = opened;
    m_deviceStateChangeInProgress = false;
}

void USBDevice::onConfigurationSelected(bool success, int configurationIndex)
{
    if (success)
        m_configurationIndex = configurationIndex;
    m_deviceStateChangeInProgress = false;
}

USBConfiguration* USBDevice::configuration() const
{
    if (m_configurationIndex != -1)
        return USBConfiguration::create(this, m_configurationIndex);
    return nullptr;
}

HeapVector<Member<USBConfiguration>> USBDevice::configurations() const
{
    HeapVector<Member<USBConfiguration>> configurations;
    size_t numConfigurations = info().configurations.size();
    for (size_t i = 0; i < numConfigurations; ++i)
        configurations.append(USBConfiguration::create(this, i));
    return configurations;
}

ScriptPromise USBDevice::open(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (m_deviceStateChangeInProgress) {
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    } else if (m_opened) {
        resolver->resolve();
    } else {
        m_deviceStateChangeInProgress = true;
        m_device->open(new OpenClosePromiseAdapter(this, resolver, true /* open */));
    }
    return promise;
}

ScriptPromise USBDevice::close(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (m_deviceStateChangeInProgress) {
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    } else if (!m_opened) {
        resolver->resolve();
    } else {
        m_deviceStateChangeInProgress = true;
        m_device->close(new OpenClosePromiseAdapter(this, resolver, false /* closed */));
    }
    return promise;
}

ScriptPromise USBDevice::selectConfiguration(ScriptState* scriptState, uint8_t configurationValue)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!m_opened) {
        resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
    } else if (m_deviceStateChangeInProgress) {
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    } else {
        int configurationIndex = findConfigurationIndex(configurationValue);
        if (configurationIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, "The configuration value provided is not supported by the device."));
        } else if (m_configurationIndex == configurationIndex) {
            resolver->resolve();
        } else {
            m_deviceStateChangeInProgress = true;
            m_device->setConfiguration(configurationValue, new SelectConfigurationPromiseAdapter(this, resolver, configurationIndex));
        }
    }
    return promise;
}

ScriptPromise USBDevice::claimInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver))
        m_device->claimInterface(interfaceNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::releaseInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver))
        m_device->releaseInterface(interfaceNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::setInterface(ScriptState* scriptState, uint8_t interfaceNumber, uint8_t alternateSetting)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver))
        m_device->setInterface(interfaceNumber, alternateSetting, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::controlTransferIn(ScriptState* scriptState, const USBControlTransferParameters& setup, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        WebUSBDevice::ControlTransferParameters parameters;
        DOMException* error = convertControlTransferParameters(WebUSBDevice::TransferDirection::In, setup, &parameters);
        if (error)
            resolver->reject(error);
        else
            m_device->controlTransfer(parameters, nullptr, length, 0, new CallbackPromiseAdapter<InputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::controlTransferOut(ScriptState* scriptState, const USBControlTransferParameters& setup)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        WebUSBDevice::ControlTransferParameters parameters;
        DOMException* error = convertControlTransferParameters(WebUSBDevice::TransferDirection::Out, setup, &parameters);
        if (error)
            resolver->reject(error);
        else
            m_device->controlTransfer(parameters, nullptr, 0, 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::controlTransferOut(ScriptState* scriptState, const USBControlTransferParameters& setup, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        WebUSBDevice::ControlTransferParameters parameters;
        DOMException* error = convertControlTransferParameters(WebUSBDevice::TransferDirection::Out, setup, &parameters);
        if (error) {
            resolver->reject(error);
        } else {
            BufferSource buffer(data);
            m_device->controlTransfer(parameters, buffer.data(), buffer.size(), 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
        }
    }
    return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* scriptState, uint8_t endpointNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver))
        m_device->clearHalt(endpointNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* scriptState, uint8_t endpointNumber, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver))
        m_device->transfer(WebUSBDevice::TransferDirection::In, endpointNumber, nullptr, length, 0, new CallbackPromiseAdapter<InputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::transferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        BufferSource buffer(data);
        m_device->transfer(WebUSBDevice::TransferDirection::Out, endpointNumber, buffer.data(), buffer.size(), 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::isochronousTransferIn(ScriptState* scriptState, uint8_t endpointNumber, Vector<unsigned> packetLengths)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver))
        m_device->isochronousTransfer(WebUSBDevice::TransferDirection::In, endpointNumber, nullptr, 0, packetLengths, 0, new CallbackPromiseAdapter<IsochronousInputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::isochronousTransferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data, Vector<unsigned> packetLengths)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        BufferSource buffer(data);
        m_device->isochronousTransfer(WebUSBDevice::TransferDirection::Out, endpointNumber, buffer.data(), buffer.size(), packetLengths, 0, new CallbackPromiseAdapter<IsochronousOutputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::reset(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (!m_opened)
        resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
    else if (m_deviceStateChangeInProgress)
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    else
        m_device->reset(new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

void USBDevice::contextDestroyed()
{
    if (m_opened)
        m_device->close(new WebUSBDeviceCloseCallbacks());
}

DEFINE_TRACE(USBDevice)
{
    ContextLifecycleObserver::trace(visitor);
}

int USBDevice::findConfigurationIndex(uint8_t configurationValue) const
{
    const auto& configurations = info().configurations;
    for (size_t i = 0; i < configurations.size(); ++i) {
        if (configurations[i].configurationValue == configurationValue)
            return i;
    }
    return -1;
}

bool USBDevice::ensureDeviceConfigured(ScriptPromiseResolver* resolver) const
{
    if (!m_opened) {
        resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
    } else if (m_deviceStateChangeInProgress) {
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    } else if (m_configurationIndex == -1) {
        resolver->reject(DOMException::create(InvalidStateError, "The device must have a configuration selected."));
    } else {
        return true;
    }
    return false;
}

} // namespace blink

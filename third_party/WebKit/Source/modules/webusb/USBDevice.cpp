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
const char kInterfaceNotFound[] = "The interface number provided is not supported by the device in its current configuration.";
const char kInterfaceStateChangeInProgress[] = "An operation that changes interface state is in progress.";
const char kOpenRequired[] = "The device must be opened first.";

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

        m_device->onDeviceOpenedOrClosed(m_desiredState);
        m_resolver->resolve();
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onDeviceOpenedOrClosed(!m_desiredState);
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    Persistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
    bool m_desiredState; // true: open, false: closed
};

class SelectConfigurationPromiseAdapter : public WebCallbacks<void, const WebUSBError&> {
public:
    SelectConfigurationPromiseAdapter(USBDevice* device, ScriptPromiseResolver* resolver, size_t configurationIndex)
        : m_device(device)
        , m_resolver(resolver)
        , m_configurationIndex(configurationIndex)
    {
    }

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onConfigurationSelected(true /* success */, m_configurationIndex);
        m_resolver->resolve();
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onConfigurationSelected(false /* failure */, m_configurationIndex);
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    Persistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
    size_t m_configurationIndex;
};

class ClaimInterfacePromiseAdapter : public WebCallbacks<void, const WebUSBError&> {
public:
    ClaimInterfacePromiseAdapter(USBDevice* device, ScriptPromiseResolver* resolver, size_t interfaceIndex, bool desiredState)
        : m_device(device)
        , m_resolver(resolver)
        , m_interfaceIndex(interfaceIndex)
        , m_desiredState(desiredState)
    {
    }

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onInterfaceClaimedOrUnclaimed(m_desiredState, m_interfaceIndex);
        m_resolver->resolve();
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onInterfaceClaimedOrUnclaimed(!m_desiredState, m_interfaceIndex);
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    Persistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
    size_t m_interfaceIndex;
    bool m_desiredState; // true: claimed, false: unclaimed
};

class SelectAlternateInterfacePromiseAdapter : public WebCallbacks<void, const WebUSBError&> {
public:
    SelectAlternateInterfacePromiseAdapter(USBDevice* device, ScriptPromiseResolver* resolver, size_t interfaceIndex, size_t alternateIndex)
        : m_device(device)
        , m_resolver(resolver)
        , m_interfaceIndex(interfaceIndex)
        , m_alternateIndex(alternateIndex)
    {
    }

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onAlternateInterfaceSelected(true /* success */, m_interfaceIndex, m_alternateIndex);
        m_resolver->resolve();
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->onAlternateInterfaceSelected(false /* failure */, m_interfaceIndex, m_alternateIndex);
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    Persistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
    size_t m_interfaceIndex;
    size_t m_alternateIndex;
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
    , m_configurationIndex(-1)
    , m_inEndpoints(15)
    , m_outEndpoints(15)
{
    int configurationIndex = findConfigurationIndex(info().activeConfiguration);
    if (configurationIndex != -1)
        onConfigurationSelected(true /* success */, configurationIndex);
}

void USBDevice::onDeviceOpenedOrClosed(bool opened)
{
    m_opened = opened;
    m_deviceStateChangeInProgress = false;
}

void USBDevice::onConfigurationSelected(bool success, size_t configurationIndex)
{
    if (success) {
        m_configurationIndex = configurationIndex;
        size_t numInterfaces = info().configurations[m_configurationIndex].interfaces.size();
        m_claimedInterfaces.clearAll();
        m_claimedInterfaces.resize(numInterfaces);
        m_interfaceStateChangeInProgress.clearAll();
        m_interfaceStateChangeInProgress.resize(numInterfaces);
        m_selectedAlternates.resize(numInterfaces);
        m_selectedAlternates.fill(0);
        m_inEndpoints.clearAll();
        m_outEndpoints.clearAll();
    }
    m_deviceStateChangeInProgress = false;
}

void USBDevice::onInterfaceClaimedOrUnclaimed(bool claimed, size_t interfaceIndex)
{
    if (claimed) {
        m_claimedInterfaces.set(interfaceIndex);
    } else {
        m_claimedInterfaces.clear(interfaceIndex);
        m_selectedAlternates[interfaceIndex] = 0;
    }
    setEndpointsForInterface(interfaceIndex, claimed);
    m_interfaceStateChangeInProgress.clear(interfaceIndex);
}

void USBDevice::onAlternateInterfaceSelected(bool success, size_t interfaceIndex, size_t alternateIndex)
{
    if (success)
        m_selectedAlternates[interfaceIndex] = alternateIndex;
    setEndpointsForInterface(interfaceIndex, success);
    m_interfaceStateChangeInProgress.clear(interfaceIndex);
}

bool USBDevice::isInterfaceClaimed(size_t configurationIndex, size_t interfaceIndex) const
{
    return m_configurationIndex != -1 && static_cast<size_t>(m_configurationIndex) == configurationIndex && m_claimedInterfaces.get(interfaceIndex);
}

size_t USBDevice::selectedAlternateInterface(size_t interfaceIndex) const
{
    return m_selectedAlternates[interfaceIndex];
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
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (m_opened) {
            resolver->resolve();
        } else {
            m_deviceStateChangeInProgress = true;
            m_device->open(new OpenClosePromiseAdapter(this, resolver, true /* open */));
        }
    }
    return promise;
}

ScriptPromise USBDevice::close(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (!m_opened) {
            resolver->resolve();
        } else {
            m_deviceStateChangeInProgress = true;
            m_device->close(new OpenClosePromiseAdapter(this, resolver, false /* closed */));
        }
    }
    return promise;
}

ScriptPromise USBDevice::selectConfiguration(ScriptState* scriptState, uint8_t configurationValue)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (!m_opened) {
            resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
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
    }
    return promise;
}

ScriptPromise USBDevice::claimInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        int interfaceIndex = findInterfaceIndex(interfaceNumber);
        if (interfaceIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, kInterfaceNotFound));
        } else if (m_interfaceStateChangeInProgress.get(interfaceIndex)) {
            resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
        } else if (m_claimedInterfaces.get(interfaceIndex)) {
            resolver->resolve();
        } else {
            m_interfaceStateChangeInProgress.set(interfaceIndex);
            m_device->claimInterface(interfaceNumber, new ClaimInterfacePromiseAdapter(this, resolver, interfaceIndex, true /* claim */));
        }
    }
    return promise;
}

ScriptPromise USBDevice::releaseInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        int interfaceIndex = findInterfaceIndex(interfaceNumber);
        if (interfaceIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, "The interface number provided is not supported by the device in its current configuration."));
        } else if (m_interfaceStateChangeInProgress.get(interfaceIndex)) {
            resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
        } else if (!m_claimedInterfaces.get(interfaceIndex)) {
            resolver->resolve();
        } else {
            // Mark this interface's endpoints unavailable while its state is
            // changing.
            setEndpointsForInterface(interfaceIndex, false);
            m_interfaceStateChangeInProgress.set(interfaceIndex);
            m_device->releaseInterface(interfaceNumber, new ClaimInterfacePromiseAdapter(this, resolver, interfaceIndex, false /* release */));
        }
    }
    return promise;
}

ScriptPromise USBDevice::selectAlternateInterface(ScriptState* scriptState, uint8_t interfaceNumber, uint8_t alternateSetting)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureInterfaceClaimed(interfaceNumber, resolver)) {
        // TODO(reillyg): This is duplicated work.
        int interfaceIndex = findInterfaceIndex(interfaceNumber);
        ASSERT(interfaceIndex != -1);
        int alternateIndex = findAlternateIndex(interfaceIndex, alternateSetting);
        if (alternateIndex == -1) {
            resolver->reject(DOMException::create(NotFoundError, "The alternate setting provided is not supported by the device in its current configuration."));
        } else {
            // Mark this old alternate interface's endpoints unavailable while
            // the change is in progress.
            setEndpointsForInterface(interfaceIndex, false);
            m_interfaceStateChangeInProgress.set(interfaceIndex);
            m_device->setInterface(interfaceNumber, alternateSetting, new SelectAlternateInterfacePromiseAdapter(this, resolver, interfaceIndex, alternateIndex));
        }
    }
    return promise;
}

ScriptPromise USBDevice::controlTransferIn(ScriptState* scriptState, const USBControlTransferParameters& setup, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureDeviceConfigured(resolver)) {
        WebUSBDevice::ControlTransferParameters parameters;
        if (convertControlTransferParameters(WebUSBDevice::TransferDirection::In, setup, &parameters, resolver))
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
        if (convertControlTransferParameters(WebUSBDevice::TransferDirection::Out, setup, &parameters, resolver))
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
        if (convertControlTransferParameters(WebUSBDevice::TransferDirection::Out, setup, &parameters, resolver)) {
            BufferSource buffer(data);
            m_device->controlTransfer(parameters, buffer.data(), buffer.size(), 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
        }
    }
    return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* scriptState, String direction, uint8_t endpointNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(direction == "in", endpointNumber, resolver))
        m_device->clearHalt(endpointNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* scriptState, uint8_t endpointNumber, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(true /* in */, endpointNumber, resolver))
        m_device->transfer(WebUSBDevice::TransferDirection::In, endpointNumber, nullptr, length, 0, new CallbackPromiseAdapter<InputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::transferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(false /* out */, endpointNumber, resolver)) {
        BufferSource buffer(data);
        m_device->transfer(WebUSBDevice::TransferDirection::Out, endpointNumber, buffer.data(), buffer.size(), 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::isochronousTransferIn(ScriptState* scriptState, uint8_t endpointNumber, Vector<unsigned> packetLengths)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(true /* in */, endpointNumber, resolver))
        m_device->isochronousTransfer(WebUSBDevice::TransferDirection::In, endpointNumber, nullptr, 0, packetLengths, 0, new CallbackPromiseAdapter<IsochronousInputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::isochronousTransferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data, Vector<unsigned> packetLengths)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureEndpointAvailable(false /* out */, endpointNumber, resolver)) {
        BufferSource buffer(data);
        m_device->isochronousTransfer(WebUSBDevice::TransferDirection::Out, endpointNumber, buffer.data(), buffer.size(), packetLengths, 0, new CallbackPromiseAdapter<IsochronousOutputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::reset(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (ensureNoDeviceOrInterfaceChangeInProgress(resolver)) {
        if (!m_opened)
            resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
        else
            m_device->reset(new CallbackPromiseAdapter<void, USBError>(resolver));
    }
    return promise;
}

void USBDevice::contextDestroyed()
{
    if (m_opened) {
        m_device->close(new WebUSBDeviceCloseCallbacks());
        m_opened = false;
    }
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

int USBDevice::findInterfaceIndex(uint8_t interfaceNumber) const
{
    ASSERT(m_configurationIndex != -1);
    const auto& interfaces = info().configurations[m_configurationIndex].interfaces;
    for (size_t i = 0; i < interfaces.size(); ++i) {
        if (interfaces[i].interfaceNumber == interfaceNumber)
            return i;
    }
    return -1;
}

int USBDevice::findAlternateIndex(size_t interfaceIndex, uint8_t alternateSetting) const
{
    ASSERT(m_configurationIndex != -1);
    const auto& alternates = info().configurations[m_configurationIndex].interfaces[interfaceIndex].alternates;
    for (size_t i = 0; i < alternates.size(); ++i) {
        if (alternates[i].alternateSetting == alternateSetting)
            return i;
    }
    return -1;
}

bool USBDevice::ensureNoDeviceOrInterfaceChangeInProgress(ScriptPromiseResolver* resolver) const
{
    if (m_deviceStateChangeInProgress)
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    else if (anyInterfaceChangeInProgress())
        resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
    else
        return true;
    return false;
}

bool USBDevice::ensureDeviceConfigured(ScriptPromiseResolver* resolver) const
{
    if (m_deviceStateChangeInProgress)
        resolver->reject(DOMException::create(InvalidStateError, kDeviceStateChangeInProgress));
    else if (!m_opened)
        resolver->reject(DOMException::create(InvalidStateError, kOpenRequired));
    else if (m_configurationIndex == -1)
        resolver->reject(DOMException::create(InvalidStateError, "The device must have a configuration selected."));
    else
        return true;
    return false;
}

bool USBDevice::ensureInterfaceClaimed(uint8_t interfaceNumber, ScriptPromiseResolver* resolver) const
{
    if (!ensureDeviceConfigured(resolver))
        return false;
    int interfaceIndex = findInterfaceIndex(interfaceNumber);
    if (interfaceIndex == -1)
        resolver->reject(DOMException::create(NotFoundError, kInterfaceNotFound));
    else if (m_interfaceStateChangeInProgress.get(interfaceIndex))
        resolver->reject(DOMException::create(InvalidStateError, kInterfaceStateChangeInProgress));
    else if (!m_claimedInterfaces.get(interfaceIndex))
        resolver->reject(DOMException::create(InvalidStateError, "The specified interface has not been claimed."));
    else
        return true;
    return false;
}

bool USBDevice::ensureEndpointAvailable(bool inTransfer, uint8_t endpointNumber, ScriptPromiseResolver* resolver) const
{
    if (!ensureDeviceConfigured(resolver))
        return false;
    if (endpointNumber == 0 || endpointNumber >= 16) {
        resolver->reject(DOMException::create(IndexSizeError, "The specified endpoint number is out of range."));
        return false;
    }
    auto& bitVector = inTransfer ? m_inEndpoints : m_outEndpoints;
    if (!bitVector.get(endpointNumber - 1)) {
        resolver->reject(DOMException::create(NotFoundError, "The specified endpoint is not part of a claimed and selected alternate interface."));
        return false;
    }
    return true;
}

bool USBDevice::anyInterfaceChangeInProgress() const
{
    for (size_t i = 0; i < m_interfaceStateChangeInProgress.size(); ++i) {
        if (m_interfaceStateChangeInProgress.quickGet(i))
            return true;
    }
    return false;
}

bool USBDevice::convertControlTransferParameters(
    WebUSBDevice::TransferDirection direction,
    const USBControlTransferParameters& parameters,
    WebUSBDevice::ControlTransferParameters* webParameters,
    ScriptPromiseResolver* resolver) const
{
    webParameters->direction = direction;

    if (parameters.requestType() == "standard") {
        webParameters->type = WebUSBDevice::RequestType::Standard;
    } else if (parameters.requestType() == "class") {
        webParameters->type = WebUSBDevice::RequestType::Class;
    } else if (parameters.requestType() == "vendor") {
        webParameters->type = WebUSBDevice::RequestType::Vendor;
    } else {
        resolver->reject(DOMException::create(TypeMismatchError, "The control transfer requestType parameter is invalid."));
        return false;
    }

    if (parameters.recipient() == "device") {
        webParameters->recipient = WebUSBDevice::RequestRecipient::Device;
    } else if (parameters.recipient() == "interface") {
        size_t interfaceNumber = parameters.index() & 0xff;
        if (!ensureInterfaceClaimed(interfaceNumber, resolver))
            return false;
        webParameters->recipient = WebUSBDevice::RequestRecipient::Interface;
    } else if (parameters.recipient() == "endpoint") {
        bool inTransfer = parameters.index() & 0x80;
        size_t endpointNumber = parameters.index() & 0x0f;
        if (!ensureEndpointAvailable(inTransfer, endpointNumber, resolver))
            return false;
        webParameters->recipient = WebUSBDevice::RequestRecipient::Endpoint;
    } else if (parameters.recipient() == "other") {
        webParameters->recipient = WebUSBDevice::RequestRecipient::Other;
    } else {
        resolver->reject(DOMException::create(TypeMismatchError, "The control transfer recipient parameter is invalid."));
        return false;
    }

    webParameters->request = parameters.request();
    webParameters->value = parameters.value();
    webParameters->index = parameters.index();
    return true;
}

void USBDevice::setEndpointsForInterface(size_t interfaceIndex, bool set)
{
    const auto& configuration = info().configurations[m_configurationIndex];
    const auto& interface = configuration.interfaces[interfaceIndex];
    const auto& alternate = interface.alternates[m_selectedAlternates[interfaceIndex]];
    for (const auto& endpoint : alternate.endpoints) {
        if (endpoint.endpointNumber == 0 || endpoint.endpointNumber >= 16)
            continue; // Ignore endpoints with invalid indices.
        auto& bitVector = endpoint.direction == WebUSBDevice::TransferDirection::In ? m_inEndpoints : m_outEndpoints;
        if (set)
            bitVector.set(endpoint.endpointNumber - 1);
        else
            bitVector.clear(endpoint.endpointNumber - 1);
    }
}

} // namespace blink

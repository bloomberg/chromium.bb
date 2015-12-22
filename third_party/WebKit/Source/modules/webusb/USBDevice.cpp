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
#include "modules/webusb/USBOutTransferResult.h"
#include "public/platform/modules/webusb/WebUSBTransferInfo.h"

namespace blink {

namespace {

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

class GetConfigurationPromiseAdapter : public WebCallbacks<uint8_t, const WebUSBError&> {
public:
    GetConfigurationPromiseAdapter(USBDevice* device, ScriptPromiseResolver* resolver) : m_device(device), m_resolver(resolver) {}

    void onSuccess(uint8_t value) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->resolve(USBConfiguration::createFromValue(m_device, value));
    }

    void onError(const WebUSBError& e) override
    {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(USBError::take(m_resolver, e));
    }

private:
    Persistent<USBDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
};

class InputTransferResult {
    WTF_MAKE_NONCOPYABLE(InputTransferResult);
public:
    using WebType = OwnPtr<WebUSBTransferInfo>;

    static USBInTransferResult* take(ScriptPromiseResolver*, PassOwnPtr<WebUSBTransferInfo> webTransferInfo)
    {
        return USBInTransferResult::create(convertTransferStatus(webTransferInfo->status), webTransferInfo->data);
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
        return USBOutTransferResult::create(convertTransferStatus(webTransferInfo->status), webTransferInfo->bytesWritten);
    }

private:
    OutputTransferResult() = delete;
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

HeapVector<Member<USBConfiguration>> USBDevice::configurations() const
{
    HeapVector<Member<USBConfiguration>> configurations;
    for (size_t i = 0; i < info().configurations.size(); ++i)
        configurations.append(USBConfiguration::create(this, i));
    return configurations;
}

ScriptPromise USBDevice::open(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->open(new CallbackPromiseAdapter<void, USBError>(resolver));
    setContext(scriptState->executionContext());
    return promise;
}

ScriptPromise USBDevice::close(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->close(new CallbackPromiseAdapter<void, USBError>(resolver));
    setContext(nullptr);
    return promise;
}

ScriptPromise USBDevice::getConfiguration(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->getConfiguration(new GetConfigurationPromiseAdapter(this, resolver));
    return promise;
}

ScriptPromise USBDevice::setConfiguration(ScriptState* scriptState, uint8_t configurationValue)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->setConfiguration(configurationValue, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::claimInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->claimInterface(interfaceNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::releaseInterface(ScriptState* scriptState, uint8_t interfaceNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->releaseInterface(interfaceNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::setInterface(ScriptState* scriptState, uint8_t interfaceNumber, uint8_t alternateSetting)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->setInterface(interfaceNumber, alternateSetting, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::controlTransferIn(ScriptState* scriptState, const USBControlTransferParameters& setup, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    WebUSBDevice::ControlTransferParameters parameters;
    DOMException* error = convertControlTransferParameters(WebUSBDevice::TransferDirection::In, setup, &parameters);
    if (error)
        resolver->reject(error);
    else
        m_device->controlTransfer(parameters, nullptr, length, 0, new CallbackPromiseAdapter<InputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::controlTransferOut(ScriptState* scriptState, const USBControlTransferParameters& setup)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    WebUSBDevice::ControlTransferParameters parameters;
    DOMException* error = convertControlTransferParameters(WebUSBDevice::TransferDirection::Out, setup, &parameters);
    if (error)
        resolver->reject(error);
    else
        m_device->controlTransfer(parameters, nullptr, 0, 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::controlTransferOut(ScriptState* scriptState, const USBControlTransferParameters& setup, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    WebUSBDevice::ControlTransferParameters parameters;
    DOMException* error = convertControlTransferParameters(WebUSBDevice::TransferDirection::Out, setup, &parameters);
    if (error) {
        resolver->reject(error);
    } else {
        BufferSource buffer(data);
        m_device->controlTransfer(parameters, buffer.data(), buffer.size(), 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
    }
    return promise;
}

ScriptPromise USBDevice::clearHalt(ScriptState* scriptState, uint8_t endpointNumber)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->clearHalt(endpointNumber, new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::transferIn(ScriptState* scriptState, uint8_t endpointNumber, unsigned length)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->transfer(WebUSBDevice::TransferDirection::In, endpointNumber, nullptr, length, 0, new CallbackPromiseAdapter<InputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::transferOut(ScriptState* scriptState, uint8_t endpointNumber, const ArrayBufferOrArrayBufferView& data)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    BufferSource buffer(data);
    m_device->transfer(WebUSBDevice::TransferDirection::Out, endpointNumber, buffer.data(), buffer.size(), 0, new CallbackPromiseAdapter<OutputTransferResult, USBError>(resolver));
    return promise;
}

ScriptPromise USBDevice::reset(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_device->reset(new CallbackPromiseAdapter<void, USBError>(resolver));
    return promise;
}

void USBDevice::contextDestroyed()
{
    m_device->close(new WebUSBDeviceCloseCallbacks());
}

DEFINE_TRACE(USBDevice)
{
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink

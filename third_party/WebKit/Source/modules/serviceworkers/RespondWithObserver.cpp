// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/RespondWithObserver.h"

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/streams/Stream.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {
namespace {

class StreamUploader : public BodyStreamBuffer::Observer {
public:
    StreamUploader(BodyStreamBuffer* buffer, PassRefPtrWillBeRawPtr<Stream> outStream)
        : m_buffer(buffer), m_outStream(outStream)
    {
    }
    ~StreamUploader() override { }
    void onWrite() override
    {
        bool needToFlush = false;
        while (RefPtr<DOMArrayBuffer> buf = m_buffer->read()) {
            needToFlush = true;
            m_outStream->addData(static_cast<const char*>(buf->data()), buf->byteLength());
        }
        if (needToFlush)
            m_outStream->flush();
    }
    void onClose() override
    {
        m_outStream->finalize();
        cleanup();
    }
    void onError() override
    {
        // If the stream is aborted soon after the stream is registered to the
        // StreamRegistry, ServiceWorkerURLRequestJob may not notice the error
        // and continue waiting forever.
        // FIXME: Add new message to report the error to the browser process.
        m_outStream->abort();
        cleanup();
    }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_buffer);
        visitor->trace(m_outStream);
        BodyStreamBuffer::Observer::trace(visitor);
    }
    void start()
    {
        m_buffer->registerObserver(this);
        onWrite();
        if (m_buffer->hasError())
            return onError();
        if (m_buffer->isClosed())
            return onClose();
    }

private:
    void cleanup()
    {
        m_buffer->unregisterObserver();
        m_buffer.clear();
        m_outStream.clear();
    }
    Member<BodyStreamBuffer> m_buffer;
    RefPtrWillBeMember<Stream> m_outStream;
};

} // namespace

class RespondWithObserver::ThenFunction final : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, RespondWithObserver* observer, ResolveType type)
    {
        ThenFunction* self = new ThenFunction(scriptState, observer, type);
        return self->bindToV8Function();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_observer);
        ScriptFunction::trace(visitor);
    }

private:
    ThenFunction(ScriptState* scriptState, RespondWithObserver* observer, ResolveType type)
        : ScriptFunction(scriptState)
        , m_observer(observer)
        , m_resolveType(type)
    {
    }

    virtual ScriptValue call(ScriptValue value) override
    {
        ASSERT(m_observer);
        ASSERT(m_resolveType == Fulfilled || m_resolveType == Rejected);
        if (m_resolveType == Rejected)
            m_observer->responseWasRejected();
        else
            m_observer->responseWasFulfilled(value);
        m_observer = nullptr;
        return value;
    }

    Member<RespondWithObserver> m_observer;
    ResolveType m_resolveType;
};

RespondWithObserver* RespondWithObserver::create(ExecutionContext* context, int eventID, WebURLRequest::FetchRequestMode requestMode, WebURLRequest::FrameType frameType)
{
    return new RespondWithObserver(context, eventID, requestMode, frameType);
}

void RespondWithObserver::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();
    m_state = Done;
}

void RespondWithObserver::didDispatchEvent(bool defaultPrevented)
{
    ASSERT(executionContext());
    if (m_state != Initial)
        return;

    if (defaultPrevented) {
        responseWasRejected();
        return;
    }

    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID);
    m_state = Done;
}

void RespondWithObserver::respondWith(ScriptState* scriptState, const ScriptValue& value, ExceptionState& exceptionState)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerOnFetchEnabled());
    if (m_state != Initial) {
        exceptionState.throwDOMException(InvalidStateError, "The fetch event has already been responded to.");
        return;
    }

    m_state = Pending;
    ScriptPromise::cast(scriptState, value).then(
        ThenFunction::createFunction(scriptState, this, ThenFunction::Fulfilled),
        ThenFunction::createFunction(scriptState, this, ThenFunction::Rejected));
}

void RespondWithObserver::responseWasRejected()
{
    ASSERT(executionContext());
    // The default value of WebServiceWorkerResponse's status is 0, which maps
    // to a network error.
    WebServiceWorkerResponse webResponse;
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, webResponse);
    m_state = Done;
}

void RespondWithObserver::responseWasFulfilled(const ScriptValue& value)
{
    ASSERT(executionContext());
    if (!V8Response::hasInstance(value.v8Value(), toIsolate(executionContext()))) {
        responseWasRejected();
        return;
    }
    Response* response = V8Response::toImplWithTypeCheck(toIsolate(executionContext()), value.v8Value());
    // "If either |response|'s type is |opaque| and |request|'s mode is not
    // |no CORS| or |response|'s type is |error|, return a network error."
    const FetchResponseData::Type responseType = response->response()->type();
    if ((responseType == FetchResponseData::OpaqueType && m_requestMode != WebURLRequest::FetchRequestModeNoCORS) || responseType == FetchResponseData::ErrorType) {
        responseWasRejected();
        return;
    }
    // Treat the opaque response as a network error for frame loading.
    if (responseType == FetchResponseData::OpaqueType && m_frameType != WebURLRequest::FrameTypeNone) {
        responseWasRejected();
        return;
    }
    if (response->bodyUsed()) {
        responseWasRejected();
        return;
    }
    response->setBodyUsed();
    if (BodyStreamBuffer* buffer = response->internalBuffer()) {
        if (buffer == response->buffer() && response->streamAccessed())
            buffer = response->createDrainingStream();
        WebServiceWorkerResponse webResponse;
        response->populateWebServiceWorkerResponse(webResponse);
        RefPtrWillBeMember<Stream> outStream(Stream::create(executionContext(), ""));
        webResponse.setStreamURL(outStream->url());
        ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, webResponse);
        StreamUploader* uploader = new StreamUploader(buffer, outStream);
        uploader->start();
        m_state = Done;
        return;
    }
    WebServiceWorkerResponse webResponse;
    response->populateWebServiceWorkerResponse(webResponse);
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, webResponse);
    m_state = Done;
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context, int eventID, WebURLRequest::FetchRequestMode requestMode, WebURLRequest::FrameType frameType)
    : ContextLifecycleObserver(context)
    , m_eventID(eventID)
    , m_requestMode(requestMode)
    , m_frameType(frameType)
    , m_state(Initial)
{
}

DEFINE_TRACE(RespondWithObserver)
{
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink

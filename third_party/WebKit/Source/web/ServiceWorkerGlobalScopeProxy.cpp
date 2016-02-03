/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/ServiceWorkerGlobalScopeProxy.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "modules/background_sync/SyncEvent.h"
#include "modules/background_sync/SyncRegistration.h"
#include "modules/fetch/Headers.h"
#include "modules/geofencing/CircularGeofencingRegion.h"
#include "modules/geofencing/GeofencingEvent.h"
#include "modules/navigatorconnect/AcceptConnectionObserver.h"
#include "modules/navigatorconnect/CrossOriginServiceWorkerClient.h"
#include "modules/navigatorconnect/ServicePortCollection.h"
#include "modules/navigatorconnect/WorkerNavigatorServices.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationEvent.h"
#include "modules/notifications/NotificationEventInit.h"
#include "modules/push_messaging/PushEvent.h"
#include "modules/push_messaging/PushMessageData.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/ExtendableMessageEvent.h"
#include "modules/serviceworkers/FetchEvent.h"
#include "modules/serviceworkers/InstallEvent.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebCrossOriginServiceWorkerClient.h"
#include "public/platform/modules/background_sync/WebSyncRegistration.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/web/WebSerializedScriptValue.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "web/WebEmbeddedWorkerImpl.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

PassOwnPtrWillBeRawPtr<ServiceWorkerGlobalScopeProxy> ServiceWorkerGlobalScopeProxy::create(WebEmbeddedWorkerImpl& embeddedWorker, Document& document, WebServiceWorkerContextClient& client)
{
    return adoptPtrWillBeNoop(new ServiceWorkerGlobalScopeProxy(embeddedWorker, document, client));
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy()
{
    // Verify that the proxy has been detached.
    ASSERT(!m_embeddedWorker);
}

DEFINE_TRACE(ServiceWorkerGlobalScopeProxy)
{
    visitor->trace(m_document);
    visitor->trace(m_workerGlobalScope);
}

void ServiceWorkerGlobalScopeProxy::setRegistration(WebPassOwnPtr<WebServiceWorkerRegistration::Handle> handle)
{
    workerGlobalScope()->setRegistration(handle);
}

void ServiceWorkerGlobalScopeProxy::dispatchActivateEvent(int eventID)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Activate, eventID);
    RefPtrWillBeRawPtr<Event> event(ExtendableEvent::create(EventTypeNames::activate, ExtendableEventInit(), observer));
    workerGlobalScope()->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchExtendableMessageEvent(int eventID, const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerExtendableMessageEventEnabled());

    ExtendableMessageEventInit initializer;
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Message, eventID);

    RefPtrWillBeRawPtr<Event> event(ExtendableMessageEvent::create(EventTypeNames::message, initializer, observer));
    workerGlobalScope()->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchFetchEvent(int eventID, const WebServiceWorkerRequest& webRequest)
{
    dispatchFetchEventImpl(eventID, webRequest, EventTypeNames::fetch);
}

void ServiceWorkerGlobalScopeProxy::dispatchForeignFetchEvent(int eventID, const WebServiceWorkerRequest& webRequest)
{
    dispatchFetchEventImpl(eventID, webRequest, EventTypeNames::foreignfetch);
}

void ServiceWorkerGlobalScopeProxy::dispatchGeofencingEvent(int eventID, WebGeofencingEventType eventType, const WebString& regionID, const WebCircularGeofencingRegion& region)
{
    const AtomicString& type = eventType == WebGeofencingEventTypeEnter ? EventTypeNames::geofenceenter : EventTypeNames::geofenceleave;
    workerGlobalScope()->dispatchEvent(GeofencingEvent::create(type, regionID, CircularGeofencingRegion::create(regionID, region)));
}

void ServiceWorkerGlobalScopeProxy::dispatchInstallEvent(int eventID)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Install, eventID);
    RefPtrWillBeRawPtr<Event> event;
    if (RuntimeEnabledFeatures::foreignFetchEnabled())
        event = InstallEvent::create(EventTypeNames::install, ExtendableEventInit(), observer);
    else
        event = ExtendableEvent::create(EventTypeNames::install, ExtendableEventInit(), observer);
    workerGlobalScope()->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    MessagePortArray* ports = MessagePort::toMessagePortArray(workerGlobalScope(), webChannels);
    WebSerializedScriptValue value = WebSerializedScriptValue::fromString(message);
    workerGlobalScope()->dispatchEvent(MessageEvent::create(ports, value));
}

void ServiceWorkerGlobalScopeProxy::dispatchNotificationClickEvent(int eventID, int64_t notificationID, const WebNotificationData& data, int actionIndex)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::NotificationClick, eventID);
    NotificationEventInit eventInit;
    eventInit.setNotification(Notification::create(workerGlobalScope(), notificationID, data));
    if (0 <= actionIndex && actionIndex < static_cast<int>(data.actions.size()))
        eventInit.setAction(data.actions[actionIndex].action);
    RefPtrWillBeRawPtr<Event> event(NotificationEvent::create(EventTypeNames::notificationclick, eventInit, observer));
    workerGlobalScope()->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchPushEvent(int eventID, const WebString& data)
{
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Push, eventID);
    RefPtrWillBeRawPtr<Event> event(PushEvent::create(EventTypeNames::push, PushMessageData::create(data), observer));
    workerGlobalScope()->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchServicePortConnectEvent(WebServicePortConnectEventCallbacks* rawCallbacks, const WebURL& targetURL, const WebString& origin, WebServicePortID portID)
{
    OwnPtr<WebServicePortConnectEventCallbacks> callbacks = adoptPtr(rawCallbacks);
    ServicePortCollection* collection = WorkerNavigatorServices::services(workerGlobalScope(), *workerGlobalScope()->navigator());
    collection->dispatchConnectEvent(callbacks.release(), targetURL, origin, portID);
}

void ServiceWorkerGlobalScopeProxy::dispatchSyncEvent(int eventID, const WebSyncRegistration& registration, LastChanceOption lastChance)
{
    if (!RuntimeEnabledFeatures::backgroundSyncEnabled()) {
        ServiceWorkerGlobalScopeClient::from(workerGlobalScope())->didHandleSyncEvent(eventID, WebServiceWorkerEventResultCompleted);
        return;
    }
    WaitUntilObserver* observer = WaitUntilObserver::create(workerGlobalScope(), WaitUntilObserver::Sync, eventID);
    RefPtrWillBeRawPtr<Event> event(SyncEvent::create(EventTypeNames::sync, registration.tag, lastChance == IsLastChance, observer));
    workerGlobalScope()->dispatchExtendableEvent(event.release(), observer);
}

void ServiceWorkerGlobalScopeProxy::dispatchCrossOriginMessageEvent(const WebCrossOriginServiceWorkerClient& webClient, const WebString& message, const WebMessagePortChannelArray& webChannels)
{
    MessagePortArray* ports = MessagePort::toMessagePortArray(workerGlobalScope(), webChannels);
    WebSerializedScriptValue value = WebSerializedScriptValue::fromString(message);
    // FIXME: Have proper source for this MessageEvent.
    RefPtrWillBeRawPtr<MessageEvent> event = MessageEvent::create(ports, value, webClient.origin.string());
    event->setType(EventTypeNames::crossoriginmessage);
    workerGlobalScope()->dispatchEvent(event);
}

void ServiceWorkerGlobalScopeProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, int)
{
    client().reportException(errorMessage, lineNumber, columnNumber, sourceURL);
}

void ServiceWorkerGlobalScopeProxy::reportConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage> consoleMessage)
{
    client().reportConsoleMessage(consoleMessage->source(), consoleMessage->level(), consoleMessage->message(), consoleMessage->lineNumber(), consoleMessage->url());
}

void ServiceWorkerGlobalScopeProxy::postMessageToPageInspector(const String& message)
{
    ASSERT(m_embeddedWorker);
    document().postInspectorTask(BLINK_FROM_HERE, createCrossThreadTask(&WebEmbeddedWorkerImpl::postMessageToPageInspector, m_embeddedWorker, message));
}

void ServiceWorkerGlobalScopeProxy::didEvaluateWorkerScript(bool success)
{
    client().didEvaluateWorkerScript(success);
}

void ServiceWorkerGlobalScopeProxy::didInitializeWorkerContext()
{
    ScriptState::Scope scope(workerGlobalScope()->script()->scriptState());
    client().didInitializeWorkerContext(workerGlobalScope()->script()->context(), WebURL(m_documentURL));
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeStarted(WorkerGlobalScope* workerGlobalScope)
{
    ASSERT(!m_workerGlobalScope);
    m_workerGlobalScope = static_cast<ServiceWorkerGlobalScope*>(workerGlobalScope);
    client().workerContextStarted(this);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeClosed()
{
    ASSERT(m_embeddedWorker);
    document().postTask(BLINK_FROM_HERE, createCrossThreadTask(&WebEmbeddedWorkerImpl::terminateWorkerContext, m_embeddedWorker));
}

void ServiceWorkerGlobalScopeProxy::willDestroyWorkerGlobalScope()
{
    v8::HandleScope handleScope(workerGlobalScope()->thread()->isolate());
    client().willDestroyWorkerContext(workerGlobalScope()->script()->context());
    m_workerGlobalScope = nullptr;
}

void ServiceWorkerGlobalScopeProxy::workerThreadTerminated()
{
    client().workerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl& embeddedWorker, Document& document, WebServiceWorkerContextClient& client)
    : m_embeddedWorker(&embeddedWorker)
    , m_document(&document)
    , m_documentURL(document.url().copy())
    , m_client(&client)
    , m_workerGlobalScope(nullptr)
{
}

void ServiceWorkerGlobalScopeProxy::detach()
{
    m_embeddedWorker = nullptr;
    m_document = nullptr;
    m_client = nullptr;
    m_workerGlobalScope = nullptr;
}

WebServiceWorkerContextClient& ServiceWorkerGlobalScopeProxy::client() const
{
    ASSERT(m_client);
    return *m_client;
}

Document& ServiceWorkerGlobalScopeProxy::document() const
{
    ASSERT(m_document);
    return *m_document;
}

ServiceWorkerGlobalScope* ServiceWorkerGlobalScopeProxy::workerGlobalScope() const
{
    ASSERT(m_workerGlobalScope);
    return m_workerGlobalScope;
}

void ServiceWorkerGlobalScopeProxy::dispatchFetchEventImpl(int eventID, const WebServiceWorkerRequest& webRequest, const AtomicString& eventTypeName)
{
    RespondWithObserver* observer = RespondWithObserver::create(workerGlobalScope(), eventID, webRequest.url(), webRequest.mode(), webRequest.frameType(), webRequest.requestContext());
    bool defaultPrevented = false;
    Request* request = Request::create(workerGlobalScope(), webRequest);
    request->headers()->setGuard(Headers::ImmutableGuard);
    FetchEventInit eventInit;
    eventInit.setCancelable(true);
    eventInit.setRequest(request);
    eventInit.setClientId(webRequest.isMainResourceLoad() ? WebString() : webRequest.clientId());
    eventInit.setIsReload(webRequest.isReload());
    RefPtrWillBeRawPtr<FetchEvent> fetchEvent(FetchEvent::create(eventTypeName, eventInit, observer));
    defaultPrevented = !workerGlobalScope()->dispatchEvent(fetchEvent.release());
    observer->didDispatchEvent(defaultPrevented);
}

} // namespace blink

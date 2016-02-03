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

#ifndef WebServiceWorkerContextProxy_h
#define WebServiceWorkerContextProxy_h

#include "public/platform/WebGeofencingEventType.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebPassOwnPtr.h"
#include "public/platform/modules/navigator_services/WebServicePortCallbacks.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {

struct WebCircularGeofencingRegion;
struct WebCrossOriginServiceWorkerClient;
struct WebNotificationData;
class WebServiceWorkerRequest;
class WebString;
struct WebSyncRegistration;

// A proxy interface to talk to the worker's GlobalScope implementation.
// All methods of this class must be called on the worker thread.
class WebServiceWorkerContextProxy {
public:
    virtual ~WebServiceWorkerContextProxy() { }

    virtual void setRegistration(WebPassOwnPtr<WebServiceWorkerRegistration::Handle>) = 0;

    virtual void dispatchActivateEvent(int eventID) = 0;
    virtual void dispatchExtendableMessageEvent(int eventID, const WebString& message, const WebMessagePortChannelArray&) = 0;
    virtual void dispatchInstallEvent(int eventID) = 0;
    virtual void dispatchFetchEvent(int eventID, const WebServiceWorkerRequest& webRequest) = 0;
    virtual void dispatchForeignFetchEvent(int eventID, const WebServiceWorkerRequest& webRequest) = 0;
    virtual void dispatchGeofencingEvent(int eventID, WebGeofencingEventType, const WebString& regionID, const WebCircularGeofencingRegion&) = 0;

    // TODO(nhiroki): Remove this after ExtendableMessageEvent is enabled by
    // default (crbug.com/543198).
    virtual void dispatchMessageEvent(const WebString& message, const WebMessagePortChannelArray&) = 0;

    virtual void dispatchNotificationClickEvent(int eventID, int64_t notificationID, const WebNotificationData&, int actionIndex) = 0;
    virtual void dispatchPushEvent(int eventID, const WebString& data) = 0;
    virtual void dispatchCrossOriginMessageEvent(const WebCrossOriginServiceWorkerClient&, const WebString& message, const WebMessagePortChannelArray&) = 0;

    // Passes ownership of the callbacks.
    virtual void dispatchServicePortConnectEvent(WebServicePortConnectEventCallbacks*, const WebURL& targetURL, const WebString& origin, WebServicePortID) = 0;

    enum LastChanceOption {
        IsNotLastChance,
        IsLastChance
    };

    // Once the ServiceWorker has finished handling the sync event,
    // didHandleSyncEvent is called on the context client.
    virtual void dispatchSyncEvent(int syncEventID, const WebSyncRegistration&, LastChanceOption) = 0;
};

} // namespace blink

#endif // WebServiceWorkerContextProxy_h

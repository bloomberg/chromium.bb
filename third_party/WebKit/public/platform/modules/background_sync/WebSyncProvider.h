// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncProvider_h
#define WebSyncProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

class WebServiceWorkerRegistration;
struct WebSyncError;
struct WebSyncRegistration;

using WebSyncRegistrationCallbacks = WebCallbacks<WebSyncRegistration, WebSyncError>;
using WebSyncUnregistrationCallbacks = WebCallbacks<bool, WebSyncError>;
using WebSyncGetRegistrationsCallbacks = WebCallbacks<WebVector<WebSyncRegistration>, WebSyncError>;

class WebSyncProvider {
public:
    virtual ~WebSyncProvider() { }

    // Takes ownership of the WebSyncRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void registerBackgroundSync(const WebSyncRegistration*, WebServiceWorkerRegistration*, WebSyncRegistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncUnregistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void unregisterBackgroundSync(const WebString&, WebServiceWorkerRegistration*, WebSyncUnregistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getRegistration(const WebString&, WebServiceWorkerRegistration*, WebSyncRegistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncGetRegistrationsCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getRegistrations(WebServiceWorkerRegistration*, WebSyncGetRegistrationsCallbacks*) = 0;
};

} // namespace blink

#endif // WebSyncProvider_h

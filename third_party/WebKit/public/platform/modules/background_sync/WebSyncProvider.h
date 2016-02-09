// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncProvider_h
#define WebSyncProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebPassOwnPtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/background_sync/WebSyncError.h"
#include "public/platform/modules/background_sync/WebSyncRegistration.h"

namespace blink {

class WebServiceWorkerRegistration;
using WebSyncRegistrationCallbacks = WebCallbacks<WebPassOwnPtr<WebSyncRegistration>, const WebSyncError&>;
using WebSyncUnregistrationCallbacks = WebCallbacks<bool, const WebSyncError&>;
using WebSyncGetRegistrationsCallbacks = WebCallbacks<const WebVector<WebSyncRegistration*>&, const WebSyncError&>;

class WebSyncProvider {
public:
    virtual ~WebSyncProvider() { }

    // Takes ownership of the WebSyncRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void registerBackgroundSync(const WebSyncRegistration*, WebServiceWorkerRegistration*, bool, WebSyncRegistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncUnregistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void unregisterBackgroundSync(int64_t handleId, WebServiceWorkerRegistration*, WebSyncUnregistrationCallbacks*) = 0;

    // Takes ownership of the WebSyncGetRegistrationsCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void getRegistrations(WebServiceWorkerRegistration*, WebSyncGetRegistrationsCallbacks*) = 0;

    virtual void releaseRegistration(int64_t handleId) = 0;
};

} // namespace blink

#endif // WebSyncProvider_h

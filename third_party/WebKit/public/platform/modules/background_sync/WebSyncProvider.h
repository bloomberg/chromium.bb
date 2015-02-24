// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncProvider_h
#define WebSyncProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"

namespace blink {

class WebServiceWorkerRegistration;
struct WebSyncError;
struct WebSyncRegistration;
struct WebSyncRegistrationOptions;

using WebSyncRegistrationCallbacks = WebCallbacks<WebSyncRegistration, WebSyncError>;

class WebSyncProvider {
public:
    virtual ~WebSyncProvider() { }

    // Takes ownership of the WebSyncRegistrationCallbacks.
    // Does not take ownership of the WebServiceWorkerRegistration.
    virtual void registerBackgroundSync(const WebSyncRegistrationOptions*, WebSyncRegistrationCallbacks*) = 0;
};

} // namespace blink

#endif // WebSyncProvider_h

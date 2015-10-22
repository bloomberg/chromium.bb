// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRegistrationProxy_h
#define WebServiceWorkerRegistrationProxy_h

#include "public/platform/WebPassOwnPtr.h"

namespace blink {

class WebServiceWorker;

// A proxy interface, passed via WebServiceWorkerRegistration.setProxy() from
// blink to the embedder, to talk to the ServiceWorkerRegistration object from
// embedder.
class WebServiceWorkerRegistrationProxy {
public:
    // Notifies that the registration entered the installation process.
    // The installing worker should be accessible via
    // WebServiceWorkerRegistration.installing.
    virtual void dispatchUpdateFoundEvent() = 0;

    virtual void setInstalling(WebPassOwnPtr<WebServiceWorker::Handle>) = 0;
    virtual void setWaiting(WebPassOwnPtr<WebServiceWorker::Handle>) = 0;
    virtual void setActive(WebPassOwnPtr<WebServiceWorker::Handle>) = 0;

protected:
    virtual ~WebServiceWorkerRegistrationProxy() { }
};

} // namespace blink

#endif // WebServiceWorkerRegistrationProxy_h

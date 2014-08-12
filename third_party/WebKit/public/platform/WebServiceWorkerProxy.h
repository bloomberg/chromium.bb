// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerProxy_h
#define WebServiceWorkerProxy_h

#include "WebCommon.h"

namespace blink {

class ServiceWorker;

// A proxy interface, passed via WebServiceWorker.setProxy() from blink to
// the embedder, to talk to the ServiceWorker object from embedder.
class WebServiceWorkerProxy {
public:
    WebServiceWorkerProxy() : m_private(0) { }
    virtual ~WebServiceWorkerProxy() { }

    // Returns true if the proxy is ready to be notified of service worker state
    // changes. It may not be if it's waiting for the registration promise to
    // resolve, while the browser side has registered and is proceeding to
    // install and activate the worker.
    virtual bool isReady() = 0;

    // Notifies the proxy that the service worker state changed. The new state
    // should be accessible via WebServiceWorker.state().
    virtual void dispatchStateChangeEvent() = 0;

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT WebServiceWorkerProxy(ServiceWorker*);
    BLINK_PLATFORM_EXPORT ServiceWorker* unwrap() const;
#endif

protected:
    ServiceWorker* m_private;
};

} // namespace blink

#endif // WebServiceWorkerProxy_h

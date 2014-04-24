// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerProxy_h
#define WebServiceWorkerProxy_h

namespace blink {

// A proxy interface, passed via WebServiceWorker.setProxy() from blink to
// the embedder, to talk to the ServiceWorker object from embedder.
class WebServiceWorkerProxy {
public:
    // Informs the proxy that the service worker's state changed. The state
    // should be accessible via WebServiceWorker.state() but may not necessarily
    // be immediately reflected. For example, this happens if the proxy is
    // waiting for the registration promise to resolve, while the browser has
    // already registered and activated the worker.
    virtual void onStateChanged(WebServiceWorkerState) = 0;

    // FIXME: To be removed, this is just here as part of a three-sided patch.
    virtual void dispatchStateChangeEvent() = 0;
};

} // namespace blink

#endif // WebServiceWorkerProxy_h

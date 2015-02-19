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
    // FIXME: Remove after Chromium-side patch lands to not call this.
    virtual bool isReady() { return true; }

    // Notifies the proxy that the service worker state changed. The new state
    // should be accessible via WebServiceWorker.state().
    virtual void dispatchStateChangeEvent() = 0;

protected:
    virtual ~WebServiceWorkerProxy() { }
};

} // namespace blink

#endif // WebServiceWorkerProxy_h

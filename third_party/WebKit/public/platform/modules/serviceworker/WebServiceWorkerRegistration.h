// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRegistration_h
#define WebServiceWorkerRegistration_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebServiceWorkerError.h"
#include "public/platform/WebURL.h"

namespace blink {

class WebServiceWorkerProvider;
class WebServiceWorkerRegistrationProxy;

class WebServiceWorkerRegistration {
public:
    virtual ~WebServiceWorkerRegistration() { }

    using WebServiceWorkerUpdateCallbacks = WebCallbacks<void, const WebServiceWorkerError&>;
    using WebServiceWorkerUnregistrationCallbacks = WebCallbacks<bool, const WebServiceWorkerError&>;

    virtual void setProxy(WebServiceWorkerRegistrationProxy*) { }
    virtual WebServiceWorkerRegistrationProxy* proxy() { return nullptr; }
    virtual void proxyStopped() { }

    virtual WebURL scope() const { return WebURL(); }
    virtual void update(WebServiceWorkerProvider*, WebServiceWorkerUpdateCallbacks*) { }
    virtual void unregister(WebServiceWorkerProvider*, WebServiceWorkerUnregistrationCallbacks*) { }
};

} // namespace blink

#endif // WebServiceWorkerRegistration_h

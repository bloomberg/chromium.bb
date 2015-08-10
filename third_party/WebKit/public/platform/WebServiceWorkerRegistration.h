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
    // TODO(jungkees):
    // void update(p) remains temporarily before the chromium-side companion
    // patch lands: https://codereview.chromium.org/1270513002/.
    // Before the chromium-side patch lands, the new update(p, c) will
    // internally call update(provider) to work with the existing code base.
    // The final changes will be incorporated with the Blink layout patches.
    virtual void update(WebServiceWorkerProvider*) { }
    virtual void update(WebServiceWorkerProvider* provider, WebServiceWorkerUpdateCallbacks* callback)
    {
        update(provider);
        callback->onSuccess();
    }
    virtual void unregister(WebServiceWorkerProvider*, WebServiceWorkerUnregistrationCallbacks*) { }
};

} // namespace blink

#endif // WebServiceWorkerRegistration_h

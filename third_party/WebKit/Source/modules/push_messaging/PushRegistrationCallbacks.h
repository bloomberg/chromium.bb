// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushRegistrationCallbacks_h
#define PushRegistrationCallbacks_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class ServiceWorkerRegistration;
class ScriptPromiseResolver;
struct WebPushError;
struct WebPushRegistration;

// PushRegistrationCallbacks is an implementation of WebPushRegistrationCallbacks
// that will resolve the underlying promise depending on the result passed to
// the callback. It takes a ServiceWorkerRegistration in its constructor and
// will pass it to the PushRegistration.
class PushRegistrationCallbacks final : public WebCallbacks<WebPushRegistration, WebPushError> {
    WTF_MAKE_NONCOPYABLE(PushRegistrationCallbacks);
public:
    PushRegistrationCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver>, ServiceWorkerRegistration*);
    ~PushRegistrationCallbacks() override;

    void onSuccess(WebPushRegistration*) override;
    void onError(WebPushError*) override;

private:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
    Persistent<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

} // namespace blink

#endif // PushRegistrationCallbacks_h

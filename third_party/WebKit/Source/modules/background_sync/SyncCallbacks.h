// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncCallbacks_h
#define SyncCallbacks_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/background_sync/WebSyncProvider.h"
#include "public/platform/modules/background_sync/WebSyncRegistration.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class ServiceWorkerRegistration;
class ScriptPromiseResolver;
struct WebSyncError;
struct WebSyncRegistration;
class WebString;

using WebSyncGetRegistrationsCallbacks = WebCallbacks<WebVector<WebSyncRegistration>, WebSyncError>;

// SyncRegistrationCallbacks is an implementation of WebSyncRegistrationCallbacks
// that will resolve the underlying promise depending on the result passed to
// the callback. It takes a ServiceWorkerRegistration in its constructor and
// will pass it to the SyncRegistration.
class SyncRegistrationCallbacks final : public WebCallbacks<WebSyncRegistration, WebSyncError> {
    WTF_MAKE_NONCOPYABLE(SyncRegistrationCallbacks);
public:
    SyncRegistrationCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver>, ServiceWorkerRegistration*);
    ~SyncRegistrationCallbacks() override;

    void onSuccess(WebSyncRegistration*) override;
    void onError(WebSyncError*) override;

private:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
    Persistent<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

// SyncUnregistrationCallbacks is an implementation of
// WebSyncUnregistrationCallbacks that will resolve the underlying promise
// depending on the result passed to the callback. It takes a
// ServiceWorkerRegistration in its constructor and will pass it to the
// SyncProvider.
class SyncUnregistrationCallbacks final : public WebCallbacks<bool, WebSyncError> {
    WTF_MAKE_NONCOPYABLE(SyncUnregistrationCallbacks);
public:
    SyncUnregistrationCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver>, ServiceWorkerRegistration*);
    ~SyncUnregistrationCallbacks() override;

    void onSuccess(bool*) override;
    void onError(WebSyncError*) override;

private:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
    Persistent<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

// SyncGetRegistrationsCallbacks is an implementation of WebSyncGetRegistrationsCallbacks
// that will resolve the underlying promise depending on the result passed to
// the callback. It takes a ServiceWorkerRegistration in its constructor and
// will pass it to the SyncRegistration.
class SyncGetRegistrationsCallbacks final : public WebCallbacks<WebVector<WebSyncRegistration>, WebSyncError> {
    WTF_MAKE_NONCOPYABLE(SyncGetRegistrationsCallbacks);
public:
    SyncGetRegistrationsCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver>, ServiceWorkerRegistration*);
    ~SyncGetRegistrationsCallbacks() override;

    void onSuccess(WebVector<WebSyncRegistration>*) override;
    void onError(WebSyncError*) override;

private:
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;
    Persistent<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

} // namespace blink

#endif // SyncCallbacks_h

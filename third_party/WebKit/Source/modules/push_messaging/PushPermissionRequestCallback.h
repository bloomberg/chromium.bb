// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermissionRequestCallback_h
#define PushPermissionRequestCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallback.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class PushManager;
class ScriptPromiseResolver;
class WebPushClient;
class WebServiceWorkerProvider;

class PushPermissionRequestCallback final : public WebCallback {
    WTF_MAKE_NONCOPYABLE(PushPermissionRequestCallback);

public:
    // Since |PushManager| is oilpan garbage collected this will keep it alive.
    // Does not take ownership of |WebPushClient|.
    // Since |ScriptPromiseResolver| is ref counted this will keep it alive.
    // Does not take ownership of |WebServiceWorkerProvider|.
    PushPermissionRequestCallback(PushManager*, WebPushClient*, PassRefPtr<ScriptPromiseResolver>, WebServiceWorkerProvider*);
    ~PushPermissionRequestCallback() override;

    void run() override;

private:
    Persistent<PushManager> m_manager;
    WebPushClient* m_client;
    RefPtr<ScriptPromiseResolver> m_resolver;
    WebServiceWorkerProvider* m_serviceWorkerProvider;
};

} // namespace blink

#endif // PushPermissionRequestCallback_h

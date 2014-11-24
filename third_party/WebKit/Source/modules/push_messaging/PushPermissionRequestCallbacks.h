// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermissionRequestCallbacks_h
#define PushPermissionRequestCallbacks_h

#include "platform/heap/Handle.h"
#include "public/platform/WebPushClient.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class PushManager;
class ScriptPromiseResolver;
class WebServiceWorkerProvider;

class PushPermissionRequestCallbacks final : public WebPushPermissionRequestCallbacks {
    WTF_MAKE_NONCOPYABLE(PushPermissionRequestCallbacks);

public:
    // Will keep a persistent reference to |PushManager| in order to keep it alive.
    // Does not take ownership of |WebPushClient| or |WebServiceWorkerProvider|.
    PushPermissionRequestCallbacks(PushManager*, WebPushClient*, PassRefPtr<ScriptPromiseResolver>, WebServiceWorkerProvider*);
    ~PushPermissionRequestCallbacks() override;

    void onSuccess() override;
    void onError() override;

private:
    Persistent<PushManager> m_manager;
    WebPushClient* m_client;
    RefPtr<ScriptPromiseResolver> m_resolver;
    WebServiceWorkerProvider* m_serviceWorkerProvider;
};

} // namespace blink

#endif // PushPermissionRequestCallbacks_h

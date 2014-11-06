// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermissionStatusCallback_h
#define PushPermissionStatusCallback_h

#include "public/platform/WebPushClient.h"
#include "public/platform/WebPushPermissionStatus.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WTF {
class String;
}

namespace blink {

class ScriptPromiseResolver;

// Will resolve the underlying promise depending on the permission passed to the callback.
class PushPermissionStatusCallback final : public WebPushPermissionStatusCallback {
    WTF_MAKE_NONCOPYABLE(PushPermissionStatusCallback);

public:
    explicit PushPermissionStatusCallback(PassRefPtr<ScriptPromiseResolver>);
    virtual ~PushPermissionStatusCallback();

    void onSuccess(WebPushPermissionStatus*) override;

    // Called if for some reason the status of the push permission cannot be checked.
    void onError() override;

private:
    static const WTF::String& permissionString(WebPushPermissionStatus);
    RefPtr<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif // PushPermissionStatusCallback_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPermissionCallback_h
#define PushPermissionCallback_h

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
class PushPermissionCallback final : public WebPushPermissionCallback {
    WTF_MAKE_NONCOPYABLE(PushPermissionCallback);

public:
    explicit PushPermissionCallback(PassRefPtr<ScriptPromiseResolver>);
    virtual ~PushPermissionCallback();

    void onSuccess(WebPushPermissionStatus*) override;

    // Called if for some reason the status of the push permission cannot be checked.
    void onError() override;

private:
    static const WTF::String& permissionString(WebPushPermissionStatus);
    RefPtr<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif // PushPermissionCallback_h

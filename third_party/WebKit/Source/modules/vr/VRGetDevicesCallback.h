// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRGetDevicesCallback_h
#define VRGetDevicesCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/vr/WebVR.h"

namespace blink {

class VRDisplayCollection;
class ScriptPromiseResolver;

// Success and failure callbacks for getDevices.
using WebVRGetDevicesCallback = WebCallbacks<const WebVector<WebVRDevice>&, void>;
class VRGetDevicesCallback final : public WebVRGetDevicesCallback {
    USING_FAST_MALLOC(VRGetDevicesCallback);
public:
    VRGetDevicesCallback(ScriptPromiseResolver*, VRDisplayCollection*);
    ~VRGetDevicesCallback() override;

    void onSuccess(const WebVector<WebVRDevice>&) override;
    void onError() override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    Persistent<VRDisplayCollection> m_displays;
};

} // namespace blink

#endif // VRGetDevicesCallback_h

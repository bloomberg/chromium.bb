// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRGetDevicesCallback_h
#define VRGetDevicesCallback_h

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplay.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"

namespace blink {

class ScriptPromiseResolver;

// Success and failure callbacks for getDisplays.
using WebVRGetDisplaysCallback = WebCallbacks<VRDisplayVector, void>;
class VRGetDevicesCallback final : public WebVRGetDisplaysCallback {
  USING_FAST_MALLOC(VRGetDevicesCallback);

 public:
  VRGetDevicesCallback(ScriptPromiseResolver*);
  ~VRGetDevicesCallback() override;

  void OnSuccess(VRDisplayVector) override;
  void OnError() override;

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // VRGetDevicesCallback_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRGetDevicesCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"

namespace blink {

VRGetDevicesCallback::VRGetDevicesCallback(ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

VRGetDevicesCallback::~VRGetDevicesCallback() {}

void VRGetDevicesCallback::OnSuccess(VRDisplayVector displays) {
  resolver_->Resolve(displays);
}

void VRGetDevicesCallback::OnError() {
  resolver_->Reject();
}

}  // namespace blink

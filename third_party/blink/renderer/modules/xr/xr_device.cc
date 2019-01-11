// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_device.h"

#include "third_party/blink/renderer/modules/xr/xr.h"

namespace blink {

XRDevice::XRDevice(XR* xr) : xr_(xr) {}

ScriptPromise XRDevice::supportsSession(
    ScriptState* script_state,
    const XRSessionCreationOptions* options) {
  return xr_->supportsSession(script_state, options);
}

ScriptPromise XRDevice::requestSession(
    ScriptState* script_state,
    const XRSessionCreationOptions* options) {
  return xr_->requestSession(script_state, options);
}

void XRDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

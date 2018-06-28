// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/lock_orientation_callback.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"

namespace blink {

LockOrientationCallback::LockOrientationCallback(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

LockOrientationCallback::~LockOrientationCallback() = default;

void LockOrientationCallback::OnSuccess() {
  resolver_->Resolve();
}

void LockOrientationCallback::OnError(WebLockOrientationError error) {
  DOMExceptionCode code = DOMExceptionCode::kUnknownError;
  String message = "";

  switch (error) {
    case kWebLockOrientationErrorNotAvailable:
      code = DOMExceptionCode::kNotSupportedError;
      message = "screen.orientation.lock() is not available on this device.";
      break;
    case kWebLockOrientationErrorFullscreenRequired:
      code = DOMExceptionCode::kSecurityError;
      message =
          "The page needs to be fullscreen in order to call "
          "screen.orientation.lock().";
      break;
    case kWebLockOrientationErrorCanceled:
      code = DOMExceptionCode::kAbortError;
      message =
          "A call to screen.orientation.lock() or screen.orientation.unlock() "
          "canceled this call.";
      break;
  }

  resolver_->Reject(DOMException::Create(code, message));
}

}  // namespace blink

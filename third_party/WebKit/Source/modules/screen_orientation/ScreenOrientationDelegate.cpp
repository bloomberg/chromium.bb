// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientationDelegate.h"

#include "common/associated_interfaces/associated_interface_provider.h"
#include "platform/wtf/Functional.h"

namespace blink {

using device::mojom::blink::ScreenOrientationLockResult;

ScreenOrientationDelegate::ScreenOrientationDelegate(
    AssociatedInterfaceProvider* provider) {
  if (provider)
    provider->GetInterface(&screen_orientation_);
}

void ScreenOrientationDelegate::LockOrientation(
    WebScreenOrientationLockType orientation,
    std::unique_ptr<WebLockOrientationCallback> callback) {
  CancelPendingLocks();

  pending_callback_ = std::move(callback);
  screen_orientation_->LockOrientation(
      orientation,
      WTF::Bind(&ScreenOrientationDelegate::OnLockOrientationResult,
                WTF::Unretained(this), ++request_id_));
}

void ScreenOrientationDelegate::UnlockOrientation() {
  CancelPendingLocks();
  screen_orientation_->UnlockOrientation();
}

void ScreenOrientationDelegate::OnLockOrientationResult(
    int request_id,
    ScreenOrientationLockResult result) {
  if (!pending_callback_ || request_id != request_id_)
    return;

  switch (result) {
    case ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS:
      pending_callback_->OnSuccess();
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE:
      pending_callback_->OnError(kWebLockOrientationErrorNotAvailable);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED:
      pending_callback_->OnError(kWebLockOrientationErrorFullscreenRequired);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED:
      pending_callback_->OnError(kWebLockOrientationErrorCanceled);
      break;
    default:
      NOTREACHED();
      break;
  }

  pending_callback_.reset();
}

void ScreenOrientationDelegate::CancelPendingLocks() {
  if (!pending_callback_)
    return;

  pending_callback_->OnError(kWebLockOrientationErrorCanceled);
  pending_callback_.reset();
}

int ScreenOrientationDelegate::GetRequestIdForTests() {
  return pending_callback_ ? request_id_ : -1;
}

void ScreenOrientationDelegate::SetScreenOrientationAssociatedPtrForTests(
    ScreenOrientationAssociatedPtr screen_orientation_associated_ptr) {
  screen_orientation_ = std::move(screen_orientation_associated_ptr);
}

}  // namespace blink

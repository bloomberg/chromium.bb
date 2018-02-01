// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationDelegate_h
#define ScreenOrientationDelegate_h

#include <memory>
#include "modules/ModulesExport.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "services/device/public/interfaces/screen_orientation.mojom-blink.h"

namespace blink {

using device::mojom::blink::ScreenOrientationAssociatedPtr;
using device::mojom::blink::ScreenOrientationLockResult;

class AssociatedInterfaceProvider;

// ScreenOrientationDelegate holds a
// device::mojom::blink::ScreenOrientationAssociatedPtr by which it sends lock
// (or unlock) requests to the browser process. It also listens for responses
// and let Blink know about the result of the request via
// WebLockOrientationCallback.
class MODULES_EXPORT ScreenOrientationDelegate {
  WTF_MAKE_NONCOPYABLE(ScreenOrientationDelegate);

 public:
  explicit ScreenOrientationDelegate(AssociatedInterfaceProvider*);
  virtual ~ScreenOrientationDelegate() = default;

  void LockOrientation(blink::WebScreenOrientationLockType,
                       std::unique_ptr<blink::WebLockOrientationCallback>);

  void UnlockOrientation();

  void SetScreenOrientationAssociatedPtrForTests(
      ScreenOrientationAssociatedPtr);

 private:
  friend class ScreenOrientationDelegateTest;

  void OnLockOrientationResult(int, ScreenOrientationLockResult);
  void CancelPendingLocks();

  int GetRequestIdForTests();

  std::unique_ptr<WebLockOrientationCallback> pending_callback_;
  int request_id_ = 0;

  ScreenOrientationAssociatedPtr screen_orientation_;
};

}  // namespace blink

#endif  // ScreenOrientationDelegate_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_ORIENTATION_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_ORIENTATION_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "services/device/public/mojom/screen_orientation.mojom-blink.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/screen_orientation/web_lock_orientation_callback.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class ScreenOrientation;

using device::mojom::blink::ScreenOrientationLockResult;

class MODULES_EXPORT ScreenOrientationController final
    : public GarbageCollected<ScreenOrientationController>,
      public ExecutionContextLifecycleObserver,
      public PageVisibilityObserver,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);

 public:
  explicit ScreenOrientationController(LocalDOMWindow&);
  ~ScreenOrientationController();

  void SetOrientation(ScreenOrientation*);
  void NotifyOrientationChanged();

  void lock(WebScreenOrientationLockType,
            std::unique_ptr<WebLockOrientationCallback>);
  void unlock();
  bool MaybeHasActiveLock() const;

  static const char kSupplementName[];
  static ScreenOrientationController* From(LocalDOMWindow&);
  static ScreenOrientationController* FromIfExists(LocalDOMWindow&);

  void SetScreenOrientationAssociatedRemoteForTests(
      HeapMojoAssociatedRemote<device::mojom::blink::ScreenOrientation>);

  void Trace(Visitor*) override;

 private:
  friend class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest;
  friend class ScreenOrientationControllerTest;

  static WebScreenOrientationType ComputeOrientation(const IntRect&, uint16_t);

  // Inherited from ExecutionContextLifecycleObserver and
  // PageVisibilityObserver.
  void ContextDestroyed() override;
  void PageVisibilityChanged() override;

  void UpdateOrientation();

  bool IsActive() const;
  bool IsVisible() const;
  bool IsActiveAndVisible() const;

  void OnLockOrientationResult(int, ScreenOrientationLockResult);
  void CancelPendingLocks();
  int GetRequestIdForTests();

  Member<ScreenOrientation> orientation_;
  bool active_lock_ = false;
  HeapMojoAssociatedRemote<device::mojom::blink::ScreenOrientation>
      screen_orientation_service_;
  std::unique_ptr<WebLockOrientationCallback> pending_callback_;
  int request_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_ORIENTATION_CONTROLLER_H_

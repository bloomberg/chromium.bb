// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationControllerImpl_h
#define ScreenOrientationControllerImpl_h

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/PlatformEventController.h"
#include "core/frame/ScreenOrientationController.h"
#include "modules/ModulesExport.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "services/device/public/mojom/screen_orientation.mojom-blink.h"

namespace blink {

class ScreenOrientation;

using device::mojom::blink::ScreenOrientationAssociatedPtr;
using device::mojom::blink::ScreenOrientationLockResult;

class MODULES_EXPORT ScreenOrientationControllerImpl final
    : public ScreenOrientationController,
      public ContextLifecycleObserver,
      public PlatformEventController {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationControllerImpl);
  WTF_MAKE_NONCOPYABLE(ScreenOrientationControllerImpl);

 public:
  ~ScreenOrientationControllerImpl() override;

  void SetOrientation(ScreenOrientation*);
  void NotifyOrientationChanged() override;

  // Implementation of ScreenOrientationController.
  void lock(WebScreenOrientationLockType,
            std::unique_ptr<WebLockOrientationCallback>) override;
  void unlock() override;
  bool MaybeHasActiveLock() const override;

  static void ProvideTo(LocalFrame&);
  static ScreenOrientationControllerImpl* From(LocalFrame&);

  void SetScreenOrientationAssociatedPtrForTests(
      ScreenOrientationAssociatedPtr);

  virtual void Trace(blink::Visitor*);

 private:
  friend class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest;
  friend class ScreenOrientationControllerImplTest;

  explicit ScreenOrientationControllerImpl(LocalFrame&);

  static WebScreenOrientationType ComputeOrientation(const IntRect&, uint16_t);

  // Inherited from PlatformEventController.
  void DidUpdateData() override;
  void RegisterWithDispatcher() override;
  void UnregisterWithDispatcher() override;
  bool HasLastData() override;

  // Inherited from ContextLifecycleObserver and PageVisibilityObserver.
  void ContextDestroyed(ExecutionContext*) override;
  void PageVisibilityChanged() override;

  void NotifyDispatcher();

  void UpdateOrientation();

  void DispatchEventTimerFired(TimerBase*);

  bool IsActive() const;
  bool IsVisible() const;
  bool IsActiveAndVisible() const;

  void OnLockOrientationResult(int, ScreenOrientationLockResult);
  void CancelPendingLocks();
  int GetRequestIdForTests();

  Member<ScreenOrientation> orientation_;
  TaskRunnerTimer<ScreenOrientationControllerImpl> dispatch_event_timer_;
  bool active_lock_ = false;
  ScreenOrientationAssociatedPtr screen_orientation_service_;
  std::unique_ptr<WebLockOrientationCallback> pending_callback_;
  int request_id_ = 0;
};

}  // namespace blink

#endif  // ScreenOrientationControllerImpl_h

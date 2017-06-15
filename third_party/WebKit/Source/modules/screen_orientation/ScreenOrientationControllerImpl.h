// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationControllerImpl_h
#define ScreenOrientationControllerImpl_h

#include "core/dom/Document.h"
#include "core/frame/PlatformEventController.h"
#include "core/frame/ScreenOrientationController.h"
#include "modules/ModulesExport.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include <memory>

namespace blink {

class ScreenOrientation;
class WebScreenOrientationClient;

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

  static void ProvideTo(LocalFrame&, WebScreenOrientationClient*);
  static ScreenOrientationControllerImpl* From(LocalFrame&);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest;

  ScreenOrientationControllerImpl(LocalFrame&, WebScreenOrientationClient*);

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

  Member<ScreenOrientation> orientation_;
  WebScreenOrientationClient* client_;
  TaskRunnerTimer<ScreenOrientationControllerImpl> dispatch_event_timer_;
  bool active_lock_ = false;
};

}  // namespace blink

#endif  // ScreenOrientationControllerImpl_h

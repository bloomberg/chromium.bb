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

  void setOrientation(ScreenOrientation*);
  void notifyOrientationChanged();

  // Implementation of ScreenOrientationController.
  void lock(WebScreenOrientationLockType,
            std::unique_ptr<WebLockOrientationCallback>) override;
  void unlock() override;
  bool maybeHasActiveLock() const override;

  static void provideTo(LocalFrame&, WebScreenOrientationClient*);
  static ScreenOrientationControllerImpl* from(LocalFrame&);

  DECLARE_VIRTUAL_TRACE();

 private:
  ScreenOrientationControllerImpl(LocalFrame&, WebScreenOrientationClient*);

  static WebScreenOrientationType computeOrientation(const IntRect&, uint16_t);

  // Inherited from PlatformEventController.
  void didUpdateData() override;
  void registerWithDispatcher() override;
  void unregisterWithDispatcher() override;
  bool hasLastData() override;

  // Inherited from ContextLifecycleObserver and PageVisibilityObserver.
  void contextDestroyed(ExecutionContext*) override;
  void pageVisibilityChanged() override;

  void notifyDispatcher();

  void updateOrientation();

  void dispatchEventTimerFired(TimerBase*);

  bool isActive() const;
  bool isVisible() const;
  bool isActiveAndVisible() const;

  Member<ScreenOrientation> m_orientation;
  WebScreenOrientationClient* m_client;
  TaskRunnerTimer<ScreenOrientationControllerImpl> m_dispatchEventTimer;
  bool m_activeLock = false;
};

}  // namespace blink

#endif  // ScreenOrientationControllerImpl_h

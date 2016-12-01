// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationControllerImpl_h
#define ScreenOrientationControllerImpl_h

#include "core/frame/DOMWindowProperty.h"
#include "core/frame/PlatformEventController.h"
#include "core/frame/ScreenOrientationController.h"
#include "modules/ModulesExport.h"
#include "platform/Timer.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include <memory>

namespace blink {

class ScreenOrientation;
class WebScreenOrientationClient;

class MODULES_EXPORT ScreenOrientationControllerImpl final
    : public GarbageCollectedFinalized<ScreenOrientationControllerImpl>,
      public ScreenOrientationController,
      public DOMWindowProperty,
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
  void pageVisibilityChanged() override;

  // Inherited from DOMWindowProperty.
  void frameDestroyed() override;

  void notifyDispatcher();

  void updateOrientation();

  void dispatchEventTimerFired(TimerBase*);

  bool isActiveAndVisible() const;

  Member<ScreenOrientation> m_orientation;
  WebScreenOrientationClient* m_client;
  Timer<ScreenOrientationControllerImpl> m_dispatchEventTimer;
};

}  // namespace blink

#endif  // ScreenOrientationControllerImpl_h

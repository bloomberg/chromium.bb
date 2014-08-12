// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/frame/PlatformEventController.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "public/platform/WebLockOrientationCallback.h"
#include "public/platform/WebScreenOrientationLockType.h"
#include "public/platform/WebScreenOrientationType.h"

namespace blink {

class FrameView;
class ScreenOrientation;
class WebScreenOrientationClient;

class ScreenOrientationController FINAL
    : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientationController>
    , public WillBeHeapSupplement<LocalFrame>
    , public PlatformEventController {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
    WTF_MAKE_NONCOPYABLE(ScreenOrientationController);
public:
    virtual ~ScreenOrientationController();

    virtual void persistentHostHasBeenDestroyed() OVERRIDE;

    void setOrientation(ScreenOrientation*);
    void notifyOrientationChanged();

    void lock(WebScreenOrientationLockType, WebLockOrientationCallback*);
    void unlock();

    const LocalFrame& frame() const;

    static void provideTo(LocalFrame&, WebScreenOrientationClient*);
    static ScreenOrientationController* from(LocalFrame&);
    static const char* supplementName();

    virtual void trace(Visitor*);

private:
    explicit ScreenOrientationController(LocalFrame&, WebScreenOrientationClient*);
    static WebScreenOrientationType computeOrientation(FrameView*);

    // Inherited from PlatformEventController.
    virtual void didUpdateData() OVERRIDE;
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;
    virtual bool hasLastData() OVERRIDE;
    virtual void pageVisibilityChanged() OVERRIDE;

    void notifyDispatcher();

    void updateOrientation();

    void dispatchEventTimerFired(Timer<ScreenOrientationController>*);

    PersistentWillBeMember<ScreenOrientation> m_orientation;
    WebScreenOrientationClient* m_client;
    LocalFrame& m_frame;
    Timer<ScreenOrientationController> m_dispatchEventTimer;
};

} // namespace blink

#endif // ScreenOrientationController_h

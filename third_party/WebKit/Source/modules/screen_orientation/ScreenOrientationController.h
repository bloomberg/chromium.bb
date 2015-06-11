// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/frame/PlatformEventController.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

class FrameView;
class ScreenOrientation;
class WebScreenOrientationClient;

class MODULES_EXPORT ScreenOrientationController final
    : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientationController>
    , public WillBeHeapSupplement<LocalFrame>
    , public LocalFrameLifecycleObserver
    , public PlatformEventController {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
    WTF_MAKE_NONCOPYABLE(ScreenOrientationController);
public:
    virtual ~ScreenOrientationController();

    void setOrientation(ScreenOrientation*);
    void notifyOrientationChanged();

    void lock(WebScreenOrientationLockType, WebLockOrientationCallback*);
    void unlock();

    static void provideTo(LocalFrame&, WebScreenOrientationClient*);
    static ScreenOrientationController* from(LocalFrame&);
    static const char* supplementName();

    DECLARE_VIRTUAL_TRACE();

private:
    ScreenOrientationController(LocalFrame&, WebScreenOrientationClient*);

    static WebScreenOrientationType computeOrientation(ChromeClient&);

    // Inherited from PlatformEventController.
    virtual void didUpdateData() override;
    virtual void registerWithDispatcher() override;
    virtual void unregisterWithDispatcher() override;
    virtual bool hasLastData() override;
    virtual void pageVisibilityChanged() override;

    // Inherited from LocalFrameLifecycleObserver.
    virtual void willDetachFrameHost() override;

    void notifyDispatcher();

    void updateOrientation();

    void dispatchChangeEvent();

    bool isActiveAndVisible() const;

    PersistentWillBeMember<ScreenOrientation> m_orientation;
    WebScreenOrientationClient* m_client;
    bool m_isDispatchingEvent;
};

} // namespace blink

#endif // ScreenOrientationController_h

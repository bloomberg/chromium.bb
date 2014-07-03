// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/page/PageLifecycleObserver.h"
#include "platform/Supplementable.h"
#include "public/platform/WebLockOrientationCallback.h"
#include "public/platform/WebScreenOrientationLockType.h"
#include "public/platform/WebScreenOrientationType.h"

namespace blink {
class WebScreenOrientationClient;
}

namespace WebCore {

class FrameView;
class OrientationInformation;

class ScreenOrientationController FINAL : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientationController>, public WillBeHeapSupplement<LocalFrame>, public PageLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
    WTF_MAKE_NONCOPYABLE(ScreenOrientationController);
public:
    virtual ~ScreenOrientationController();

    virtual void persistentHostHasBeenDestroyed() OVERRIDE;

    OrientationInformation* orientation();
    void notifyOrientationChanged();

    void lockOrientation(blink::WebScreenOrientationLockType, blink::WebLockOrientationCallback*);
    void unlockOrientation();

    static void provideTo(LocalFrame&, blink::WebScreenOrientationClient*);
    static ScreenOrientationController& from(LocalFrame&);
    static const char* supplementName();

    virtual void trace(Visitor*);

private:
    explicit ScreenOrientationController(LocalFrame&, blink::WebScreenOrientationClient*);
    static blink::WebScreenOrientationType computeOrientation(FrameView*);

    // Inherited from PageLifecycleObserver.
    virtual void pageVisibilityChanged() OVERRIDE;

    void updateOrientation();

    PersistentWillBeMember<OrientationInformation> m_orientation;
    blink::WebScreenOrientationClient* m_client;
    LocalFrame& m_frame;
};

} // namespace WebCore

#endif // ScreenOrientationController_h

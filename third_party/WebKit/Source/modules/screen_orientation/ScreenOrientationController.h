// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/page/Page.h"
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

class ScreenOrientationController FINAL : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientationController>, public WillBeHeapSupplement<Page>, public PageLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
public:
    virtual ~ScreenOrientationController();

    blink::WebScreenOrientationType orientation() const;

    static void provideTo(Page&, blink::WebScreenOrientationClient*);
    static ScreenOrientationController& from(Page&);
    static const char* supplementName();

    void lockOrientation(blink::WebScreenOrientationLockType, blink::WebLockOrientationCallback*);
    void unlockOrientation();

private:
    explicit ScreenOrientationController(Page&, blink::WebScreenOrientationClient*);
    static blink::WebScreenOrientationType computeOrientation(FrameView*);

    // Inherited from PageLifecycleObserver.
    virtual void pageVisibilityChanged() OVERRIDE;

    blink::WebScreenOrientationType m_overrideOrientation;
    blink::WebScreenOrientationClient* m_client;
};

} // namespace WebCore

#endif // ScreenOrientationController_h

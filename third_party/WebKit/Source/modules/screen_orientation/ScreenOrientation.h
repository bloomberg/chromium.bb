// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientation_h
#define ScreenOrientation_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebScreenOrientationLockType.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class ExceptionState;
class Screen;

class ScreenOrientation FINAL : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientation>, public WillBeHeapSupplement<Screen>, DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientation);
public:
    static ScreenOrientation& from(Screen&);
    virtual ~ScreenOrientation();

    static const AtomicString& orientation(Screen&);
    static bool lockOrientation(Screen&, const AtomicString& orientation, ExceptionState&);
    static void unlockOrientation(Screen&);

    virtual void trace(Visitor* visitor) OVERRIDE { WillBeHeapSupplement<Screen>::trace(visitor); }

private:
    explicit ScreenOrientation(Screen&);

    void lockOrientationAsync(blink::WebScreenOrientationLockType);
    void orientationLockTimerFired(Timer<ScreenOrientation>*);

    static const char* supplementName();
    Document* document() const;

    Timer<ScreenOrientation> m_orientationLockTimer;
    blink::WebScreenOrientationLockType m_prospectiveLock;
};

} // namespace WebCore

#endif // ScreenOrientation_h

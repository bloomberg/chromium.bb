// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientation_h
#define ScreenOrientation_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebScreenOrientationType.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class ScriptPromise;
class ScriptState;
class ScreenOrientationController;

class ScreenOrientation FINAL
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<ScreenOrientation>
    , public EventTargetWithInlineData
    , public DOMWindowProperty {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<ScreenOrientation>);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientation);
public:
    static ScreenOrientation* create(LocalFrame*);

    virtual ~ScreenOrientation();

    // EventTarget implementation.
    virtual const WTF::AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    String type() const;
    unsigned short angle() const;

    void setType(WebScreenOrientationType);
    void setAngle(unsigned short);

    ScriptPromise lock(ScriptState*, const AtomicString& orientation);
    void unlock();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

    // Helper being used by this class and LockOrientationCallback.
    static const AtomicString& orientationTypeToString(WebScreenOrientationType);

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit ScreenOrientation(LocalFrame*);

    ScreenOrientationController* controller();

    WebScreenOrientationType m_type;
    unsigned short m_angle;
};

} // namespace blink

#endif // ScreenOrientation_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenScreenOrientation_h
#define ScreenScreenOrientation_h

#include "platform/Supplementable.h"

namespace blink {

class ScreenOrientation;
class Screen;
class ScriptState;

class ScreenScreenOrientation final
    : public NoBaseWillBeGarbageCollectedFinalized<ScreenScreenOrientation>
    , public WillBeHeapSupplement<Screen> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenScreenOrientation);
public:
    static ScreenScreenOrientation& from(Screen&);
    virtual ~ScreenScreenOrientation();

    static ScreenOrientation* orientation(ScriptState*, Screen&);

    virtual void trace(Visitor*) override;

private:
    static const char* supplementName();

    PersistentWillBeMember<ScreenOrientation> m_orientation;
};

} // namespace blink

#endif // ScreenScreenOrientation_h

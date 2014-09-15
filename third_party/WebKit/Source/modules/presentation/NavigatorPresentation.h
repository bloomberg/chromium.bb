// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPresentation_h
#define NavigatorPresentation_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"

namespace blink {

class Navigator;
class Presentation;

class NavigatorPresentation FINAL
    : public NoBaseWillBeGarbageCollectedFinalized<NavigatorPresentation>
    , public WillBeHeapSupplement<Navigator>
    , public DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorPresentation);
public:
    virtual ~NavigatorPresentation();

    static NavigatorPresentation& from(Navigator&);
    static Presentation* presentation(Navigator&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    static const char* supplementName();

    explicit NavigatorPresentation(LocalFrame*);

    Presentation* presentation();

    PersistentWillBeMember<Presentation> m_presentation;
};

} // namespace blink

#endif // NavigatorPresentation_h

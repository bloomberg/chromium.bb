// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorPresentation_h
#define NavigatorPresentation_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Navigator;
class Presentation;

class NavigatorPresentation final
    : public NoBaseWillBeGarbageCollectedFinalized<NavigatorPresentation>
    , public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorPresentation);
public:
    virtual ~NavigatorPresentation();

    static NavigatorPresentation& from(Navigator&);
    static Presentation* presentation(Navigator&);

    DECLARE_VIRTUAL_TRACE();

private:
    static const char* supplementName();

    NavigatorPresentation();

    Presentation* presentation();

    PersistentWillBeMember<Presentation> m_presentation;
};

} // namespace blink

#endif // NavigatorPresentation_h

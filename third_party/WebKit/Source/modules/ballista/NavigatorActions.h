// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorActions_h
#define NavigatorActions_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Actions;
class Navigator;

class NavigatorActions final : public GarbageCollected<NavigatorActions>, public Supplement<Navigator> {
    USING_GARBAGE_COLLECTED_MIXIN(NavigatorActions);
public:
    // Gets, or creates, NavigatorActions supplement on Navigator.
    // See platform/Supplementable.h
    static NavigatorActions& from(Navigator&);

    static Actions* actions(Navigator&);
    Actions* actions();

    DECLARE_TRACE();

private:
    NavigatorActions();
    static const char* supplementName();

    Member<Actions> m_actions;
};

} // namespace blink

#endif // NavigatorActions_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorBluetooth_h
#define NavigatorBluetooth_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Bluetooth;
class Navigator;

class NavigatorBluetooth FINAL
    : public NoBaseWillBeGarbageCollected<NavigatorBluetooth>
    , public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorBluetooth);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(NavigatorBluetooth);
public:
    static NavigatorBluetooth& from(Navigator&);

    static Bluetooth* bluetooth(Navigator&);
    Bluetooth* bluetooth();

    void trace(Visitor*);

private:
    NavigatorBluetooth();
    static const char* supplementName();

    PersistentWillBeMember<Bluetooth> m_bluetooth;
};

} // namespace blink

#endif // NavigatorBluetooth_h

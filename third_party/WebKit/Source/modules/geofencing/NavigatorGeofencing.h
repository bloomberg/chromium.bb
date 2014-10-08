// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorGeofencing_h
#define NavigatorGeofencing_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Geofencing;
class Navigator;

class NavigatorGeofencing final
    : public NoBaseWillBeGarbageCollectedFinalized<NavigatorGeofencing>
    , public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorGeofencing);

public:
    virtual ~NavigatorGeofencing();
    static NavigatorGeofencing& from(Navigator&);

    static Geofencing* geofencing(Navigator&);
    Geofencing* geofencing();

    virtual void trace(Visitor*) override;

private:
    NavigatorGeofencing();
    static const char* supplementName();

    PersistentWillBeMember<Geofencing> m_geofencing;
};

} // namespace blink

#endif // NavigatorGeofencing_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerNavigatorGeofencing_h
#define WorkerNavigatorGeofencing_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Geofencing;
class WorkerNavigator;

class WorkerNavigatorGeofencing final : public NoBaseWillBeGarbageCollectedFinalized<WorkerNavigatorGeofencing>, public WillBeHeapSupplement<WorkerNavigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorGeofencing);
    WTF_MAKE_NONCOPYABLE(WorkerNavigatorGeofencing);
public:
    virtual ~WorkerNavigatorGeofencing();
    static WorkerNavigatorGeofencing& from(WorkerNavigator&);

    static Geofencing* geofencing(WorkerNavigator&);
    Geofencing* geofencing();

    virtual void trace(Visitor*) override;

private:
    WorkerNavigatorGeofencing();
    static const char* supplementName();

    PersistentWillBeMember<Geofencing> m_geofencing;
};

} // namespace blink

#endif // WorkerNavigatorGeofencing_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorVRDevice_h
#define NavigatorVRDevice_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/vr/VRDevice.h"
#include "modules/vr/VRHardwareUnit.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Navigator;

class NavigatorVRDevice final : public NoBaseWillBeGarbageCollectedFinalized<NavigatorVRDevice>, public WillBeHeapSupplement<Navigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorVRDevice);
public:
    static NavigatorVRDevice* from(Document&);
    static NavigatorVRDevice& from(Navigator&);
    virtual ~NavigatorVRDevice();

    static ScriptPromise getVRDevices(ScriptState*, Navigator&);
    ScriptPromise getVRDevices(ScriptState*);

    virtual void trace(Visitor*);

private:
    friend VRHardwareUnit;

    NavigatorVRDevice();

    static const char* supplementName();

    VRDeviceVector getUpdatedVRHardwareUnits();
    VRHardwareUnit* getHardwareUnitForIndex(unsigned index);

    PersistentHeapVectorWillBeHeapVector<Member<VRHardwareUnit>> m_hardwareUnits;
};

} // namespace blink

#endif // NavigatorVRDevice_h

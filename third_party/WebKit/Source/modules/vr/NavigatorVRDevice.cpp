// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/vr/NavigatorVRDevice.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "modules/vr/HMDVRDevice.h"
#include "modules/vr/PositionSensorVRDevice.h"
#include "modules/vr/VRHardwareUnit.h"
#include "modules/vr/VRPositionState.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"

namespace blink {

NavigatorVRDevice* NavigatorVRDevice::from(Document& document)
{
    if (!document.frame() || !document.frame()->domWindow())
        return 0;
    Navigator& navigator = *document.frame()->domWindow()->navigator();
    return &from(navigator);
}

NavigatorVRDevice& NavigatorVRDevice::from(Navigator& navigator)
{
    NavigatorVRDevice* supplement = static_cast<NavigatorVRDevice*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorVRDevice();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

ScriptPromise NavigatorVRDevice::getVRDevices(ScriptState* scriptState, Navigator& navigator)
{
    return NavigatorVRDevice::from(navigator).getVRDevices(scriptState);
}

ScriptPromise NavigatorVRDevice::getVRDevices(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->resolve(getUpdatedVRHardwareUnits());

    return promise;
}

HeapVector<Member<VRDevice>> NavigatorVRDevice::getUpdatedVRHardwareUnits()
{
    WebVector<blink::WebVRDevice> devices;
    blink::Platform::current()->getVRDevices(&devices);

    VRDeviceVector vrDevices;
    for (size_t i = 0; i < devices.size(); ++i) {
        const blink::WebVRDevice& device = devices[i];

        VRHardwareUnit* hardwareUnit = getHardwareUnitForIndex(device.index);
        if (!hardwareUnit) {
            hardwareUnit = new VRHardwareUnit();
            m_hardwareUnits.append(hardwareUnit);
        }

        hardwareUnit->updateFromWebVRDevice(device);
        hardwareUnit->addDevicesToVector(vrDevices);
    }

    return vrDevices;
}

VRHardwareUnit* NavigatorVRDevice::getHardwareUnitForIndex(unsigned index)
{
    VRHardwareUnit* hardwareUnit;
    for (size_t i = 0; i < m_hardwareUnits.size(); ++i) {
        hardwareUnit = m_hardwareUnits[i];
        if (hardwareUnit->index() == index) {
            return hardwareUnit;
        }
    }

    return 0;
}

void NavigatorVRDevice::trace(Visitor* visitor)
{
    visitor->trace(m_hardwareUnits);

    WillBeHeapSupplement<Navigator>::trace(visitor);
}

NavigatorVRDevice::NavigatorVRDevice()
{
}

NavigatorVRDevice::~NavigatorVRDevice()
{
}

const char* NavigatorVRDevice::supplementName()
{
    return "NavigatorVRDevice";
}

} // namespace blink

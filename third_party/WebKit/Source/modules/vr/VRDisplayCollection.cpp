// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplayCollection.h"

#include "modules/vr/NavigatorVR.h"

namespace blink {

VRDisplayCollection::VRDisplayCollection(NavigatorVR* navigatorVR)
    : m_navigatorVR(navigatorVR)
{
}

VRDisplayVector VRDisplayCollection::updateDisplays(const WebVector<WebVRDevice>& devices)
{
    VRDisplayVector vrDevices;

    for (const auto& device : devices) {
        VRDisplay* display = getDisplayForIndex(device.index);
        if (!display) {
            display = new VRDisplay(m_navigatorVR);
            m_displays.append(display);
        }

        display->updateFromWebVRDevice(device);
        vrDevices.append(display);
    }

    return vrDevices;
}

VRDisplay* VRDisplayCollection::getDisplayForIndex(unsigned index)
{
    VRDisplay* display;
    for (size_t i = 0; i < m_displays.size(); ++i) {
        display = m_displays[i];
        if (display->displayId() == index) {
            return display;
        }
    }

    return 0;
}

DEFINE_TRACE(VRDisplayCollection)
{
    visitor->trace(m_navigatorVR);
    visitor->trace(m_displays);
}

} // namespace blink

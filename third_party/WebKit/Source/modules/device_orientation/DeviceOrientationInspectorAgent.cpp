// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"

#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/page/Page.h"

#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationData.h"

namespace WebCore {

PassOwnPtr<DeviceOrientationInspectorAgent> DeviceOrientationInspectorAgent::create(Page* page)
{
    return adoptPtr(new DeviceOrientationInspectorAgent(page));
}

DeviceOrientationInspectorAgent::~DeviceOrientationInspectorAgent()
{
}

DeviceOrientationInspectorAgent::DeviceOrientationInspectorAgent(Page* page)
    : InspectorBaseAgent<DeviceOrientationInspectorAgent>("DeviceOrientation")
    , m_page(page)
{
}

void DeviceOrientationInspectorAgent::setDeviceOrientationOverride(ErrorString* error, double alpha, double beta, double gamma)
{
    DeviceOrientationController* controller = DeviceOrientationController::from(m_page->mainFrame()->document());
    if (!controller) {
        *error = "Internal error: unable to override device orientation";
        return;
    }
    controller->didChangeDeviceOrientation(DeviceOrientationData::create(true, alpha, true, beta, true, gamma).get());
}

void DeviceOrientationInspectorAgent::clearDeviceOrientationOverride(ErrorString* error)
{
    setDeviceOrientationOverride(error, 0, 0, 0);
}

} // namespace WebCore

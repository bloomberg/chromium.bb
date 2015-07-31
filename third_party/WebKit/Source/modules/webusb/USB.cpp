// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webusb/USB.h"

#include "modules/webusb/USBDeviceEnumerationOptions.h"
#include "modules/webusb/USBDeviceFilter.h"
#include "public/platform/modules/webusb/WebUSBClient.h"
#include "public/platform/modules/webusb/WebUSBDeviceEnumerationOptions.h"
#include "public/platform/modules/webusb/WebUSBDeviceFilter.h"

namespace blink {

USB::USB(LocalFrame& frame)
    : m_controller(USBController::from(frame))
{
}

ScriptPromise USB::getDevices(ScriptState* scriptState, const USBDeviceEnumerationOptions& options)
{
    ASSERT_NOT_REACHED();
    return ScriptPromise();
}

DEFINE_TRACE(USB)
{
    visitor->trace(m_controller);
}

} // namespace blink
